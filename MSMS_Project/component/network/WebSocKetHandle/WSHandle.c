#include "WSHandle.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <limits.h>
#include <string.h>

#define WS_TEXT_OPCODE 0x01
#define WS_BINARY_OPCODE 0x02

static const char *TAG_WS = "WSHandle";

typedef struct {
  esp_websocket_client_handle_t client;
  SemaphoreHandle_t lock;
  char url[WS_HANDLE_URL_MAX_LEN];
  char headers[256];
  int network_timeout_ms;
  int reconnect_timeout_ms;
  size_t ping_interval_sec;
  bool connected;
  bool initialized;
  ws_handle_event_cb_t event_cb;
  ws_handle_rx_cb_t rx_cb;
  void *user_ctx;
} ws_handle_state_t;

static ws_handle_state_t s_ws = {
    .network_timeout_ms = 10000,
    .reconnect_timeout_ms = 10000,
    .ping_interval_sec = 10,
};

static void websocket_event_handler(void *arg, esp_event_base_t base,
                                    int32_t event_id, void *event_data);

static esp_err_t ws_lock_init(void) {
  if (s_ws.lock != NULL) {
    return ESP_OK;
  }

  s_ws.lock = xSemaphoreCreateMutex();
  if (s_ws.lock == NULL) {
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

static bool ws_take_lock(TickType_t timeout_ticks) {
  return s_ws.lock != NULL &&
         xSemaphoreTake(s_ws.lock, timeout_ticks) == pdTRUE;
}

static void ws_give_lock(void) {
  if (s_ws.lock != NULL) {
    xSemaphoreGive(s_ws.lock);
  }
}

static void ws_copy_string(char *dst, size_t dst_len, const char *src) {
  if (dst == NULL || dst_len == 0) {
    return;
  }
  if (src == NULL) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dst_len - 1);
  dst[dst_len - 1] = '\0';
}

static esp_err_t ws_create_client_locked(void) {
  if (s_ws.client != NULL) {
    return ESP_OK;
  }
  if (s_ws.url[0] == '\0') {
    (void)WSHandle_LoadUrl(s_ws.url, sizeof(s_ws.url));
  }
  if (s_ws.url[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }

  esp_websocket_client_config_t cfg = {
      .uri = s_ws.url,
      .network_timeout_ms = s_ws.network_timeout_ms,
      .reconnect_timeout_ms = s_ws.reconnect_timeout_ms,
  };

  if (s_ws.headers[0] != '\0') {
    cfg.headers = s_ws.headers;
  }

  s_ws.client = esp_websocket_client_init(&cfg);
  if (s_ws.client == NULL) {
    return ESP_FAIL;
  }

  if (s_ws.ping_interval_sec > 0) {
    (void)esp_websocket_client_set_ping_interval_sec(s_ws.client,
                                                     s_ws.ping_interval_sec);
  }

  esp_err_t ret = esp_websocket_register_events(
      s_ws.client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
  if (ret != ESP_OK) {
    esp_websocket_client_destroy(s_ws.client);
    s_ws.client = NULL;
    return ret;
  }

  return ESP_OK;
}

static void ws_dispatch_event(const ws_handle_event_data_t *event) {
  ws_handle_event_cb_t event_cb = NULL;
  ws_handle_rx_cb_t rx_cb = NULL;
  void *user_ctx = NULL;

  if (ws_take_lock(pdMS_TO_TICKS(100)) == true) {
    event_cb = s_ws.event_cb;
    rx_cb = s_ws.rx_cb;
    user_ctx = s_ws.user_ctx;
    ws_give_lock();
  }

  if (event_cb != NULL) {
    event_cb(event, user_ctx);
  }

  if (event != NULL && event->event == WS_HANDLE_EVENT_DATA &&
      rx_cb != NULL) {
    rx_cb(event->data, event->data_len, event->is_binary, user_ctx);
  }
}

static void websocket_event_handler(void *arg, esp_event_base_t base,
                                    int32_t event_id, void *event_data) {
  (void)arg;
  (void)base;

  esp_websocket_event_data_t *data =
      (esp_websocket_event_data_t *)event_data;
  ws_handle_event_data_t event = {
      .event = WS_HANDLE_EVENT_ERROR,
      .data = NULL,
      .data_len = 0,
      .is_binary = false,
      .fin = false,
      .op_code = 0,
      .payload_len = 0,
      .payload_offset = 0,
      .error = NULL,
  };

  if (data != NULL) {
    event.data = (const uint8_t *)data->data_ptr;
    event.data_len = data->data_len > 0 ? (size_t)data->data_len : 0;
    event.is_binary = data->op_code == WS_BINARY_OPCODE;
    event.fin = data->fin;
    event.op_code = data->op_code;
    event.payload_len = data->payload_len;
    event.payload_offset = data->payload_offset;
    event.error = &data->error_handle;
  }

  switch (event_id) {
  case WEBSOCKET_EVENT_CONNECTED:
    if (ws_take_lock(pdMS_TO_TICKS(100)) == true) {
      s_ws.connected = true;
      ws_give_lock();
    }
    event.event = WS_HANDLE_EVENT_CONNECTED;
    ESP_LOGI(TAG_WS, "connected");
    break;

  case WEBSOCKET_EVENT_DISCONNECTED:
    if (ws_take_lock(pdMS_TO_TICKS(100)) == true) {
      s_ws.connected = false;
      ws_give_lock();
    }
    event.event = WS_HANDLE_EVENT_DISCONNECTED;
    ESP_LOGW(TAG_WS, "disconnected");
    break;

  case WEBSOCKET_EVENT_DATA:
    event.event = WS_HANDLE_EVENT_DATA;
    ESP_LOGD(TAG_WS, "rx %u bytes, opcode=%u, fin=%d",
             (unsigned)event.data_len, (unsigned)event.op_code,
             event.fin ? 1 : 0);
    break;

  case WEBSOCKET_EVENT_ERROR:
    event.event = WS_HANDLE_EVENT_ERROR;
    ESP_LOGE(TAG_WS, "error");
    break;

  default:
    return;
  }

  ws_dispatch_event(&event);
}

esp_err_t WSHandle_Init(const ws_handle_config_t *config) {
  esp_err_t ret = ws_lock_init();
  if (ret != ESP_OK) {
    return ret;
  }

  if (ws_take_lock(pdMS_TO_TICKS(1000)) != true) {
    return ESP_ERR_TIMEOUT;
  }

  if (s_ws.initialized && s_ws.client != NULL) {
    ws_give_lock();
    return ESP_ERR_INVALID_STATE;
  }

  if (config != NULL) {
    ws_copy_string(s_ws.url, sizeof(s_ws.url), config->uri);
    ws_copy_string(s_ws.headers, sizeof(s_ws.headers), config->headers);
    if (config->network_timeout_ms > 0) {
      s_ws.network_timeout_ms = config->network_timeout_ms;
    }
    if (config->reconnect_timeout_ms > 0) {
      s_ws.reconnect_timeout_ms = config->reconnect_timeout_ms;
    }
    if (config->ping_interval_sec > 0) {
      s_ws.ping_interval_sec = config->ping_interval_sec;
    }
    s_ws.event_cb = config->event_cb;
    s_ws.rx_cb = config->rx_cb;
    s_ws.user_ctx = config->user_ctx;
  }

  if (s_ws.url[0] == '\0') {
    (void)WSHandle_LoadUrl(s_ws.url, sizeof(s_ws.url));
  }

  s_ws.initialized = true;
  bool auto_start = config != NULL && config->auto_start;
  ws_give_lock();

  if (auto_start) {
    return WSHandle_Start();
  }

  return ESP_OK;
}

esp_err_t WSHandle_SetCallbacks(ws_handle_event_cb_t event_cb,
                                ws_handle_rx_cb_t rx_cb, void *user_ctx) {
  esp_err_t ret = ws_lock_init();
  if (ret != ESP_OK) {
    return ret;
  }

  if (ws_take_lock(pdMS_TO_TICKS(1000)) != true) {
    return ESP_ERR_TIMEOUT;
  }

  s_ws.event_cb = event_cb;
  s_ws.rx_cb = rx_cb;
  s_ws.user_ctx = user_ctx;
  ws_give_lock();

  return ESP_OK;
}

esp_err_t WSHandle_Start(void) {
  esp_err_t ret = ws_lock_init();
  if (ret != ESP_OK) {
    return ret;
  }

  if (ws_take_lock(pdMS_TO_TICKS(1000)) != true) {
    return ESP_ERR_TIMEOUT;
  }

  ret = ws_create_client_locked();
  if (ret == ESP_OK) {
    ret = esp_websocket_client_start(s_ws.client);
  }

  ws_give_lock();
  return ret;
}

esp_err_t WSHandle_Stop(void) {
  if (s_ws.lock == NULL) {
    return ESP_OK;
  }

  if (ws_take_lock(pdMS_TO_TICKS(1000)) != true) {
    return ESP_ERR_TIMEOUT;
  }

  esp_websocket_client_handle_t client = s_ws.client;
  s_ws.client = NULL;
  s_ws.connected = false;
  ws_give_lock();

  if (client == NULL) {
    return ESP_OK;
  }

  esp_err_t stop_ret = esp_websocket_client_stop(client);
  esp_err_t destroy_ret = esp_websocket_client_destroy(client);
  return stop_ret != ESP_OK ? stop_ret : destroy_ret;
}

esp_err_t WSHandle_Deinit(void) {
  esp_err_t ret = WSHandle_Stop();

  if (ws_take_lock(pdMS_TO_TICKS(1000)) == true) {
    s_ws.initialized = false;
    s_ws.event_cb = NULL;
    s_ws.rx_cb = NULL;
    s_ws.user_ctx = NULL;
    ws_give_lock();
  }

  return ret;
}

bool WSHandle_IsStarted(void) {
  bool started = false;
  if (ws_take_lock(pdMS_TO_TICKS(100)) == true) {
    started = s_ws.client != NULL;
    ws_give_lock();
  }
  return started;
}

bool WSHandle_IsConnected(void) {
  bool connected = false;
  if (ws_take_lock(pdMS_TO_TICKS(100)) == true) {
    connected = s_ws.client != NULL && s_ws.connected &&
                esp_websocket_client_is_connected(s_ws.client);
    ws_give_lock();
  }
  return connected;
}

esp_websocket_client_handle_t WSHandle_GetClient(void) {
  esp_websocket_client_handle_t client = NULL;
  if (ws_take_lock(pdMS_TO_TICKS(100)) == true) {
    client = s_ws.client;
    ws_give_lock();
  }
  return client;
}

esp_err_t WSHandle_SetUrl(const char *url) {
  if (url == NULL || url[0] == '\0' || strlen(url) >= WS_HANDLE_URL_MAX_LEN) {
    return ESP_ERR_INVALID_ARG;
  }

  bool restart = WSHandle_IsStarted();
  if (restart) {
    esp_err_t ret = WSHandle_Stop();
    if (ret != ESP_OK) {
      return ret;
    }
  }

  if (ws_lock_init() != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }
  if (ws_take_lock(pdMS_TO_TICKS(1000)) != true) {
    return ESP_ERR_TIMEOUT;
  }
  ws_copy_string(s_ws.url, sizeof(s_ws.url), url);
  ws_give_lock();

  return restart ? WSHandle_Start() : ESP_OK;
}

const char *WSHandle_GetUrl(void) {
  if (s_ws.url[0] == '\0') {
    (void)WSHandle_LoadUrl(s_ws.url, sizeof(s_ws.url));
  }
  return s_ws.url;
}

esp_err_t WSHandle_SaveUrl(const char *url) {
  esp_err_t ret = WSHandle_SetUrl(url);
  if (ret != ESP_OK) {
    return ret;
  }

  nvs_handle_t nvs_handle;
  ret = nvs_open(WS_HANDLE_DEFAULT_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    return ret;
  }

  ret = nvs_set_str(nvs_handle, WS_HANDLE_DEFAULT_NVS_KEY, url);
  if (ret == ESP_OK) {
    ret = nvs_commit(nvs_handle);
  }
  nvs_close(nvs_handle);

  return ret;
}

esp_err_t WSHandle_LoadUrl(char *url, size_t len) {
  if (url == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t nvs_handle;
  esp_err_t ret =
      nvs_open(WS_HANDLE_DEFAULT_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  if (ret != ESP_OK) {
    url[0] = '\0';
    return ret;
  }

  size_t required_size = len;
  ret = nvs_get_str(nvs_handle, WS_HANDLE_DEFAULT_NVS_KEY, url,
                    &required_size);
  nvs_close(nvs_handle);

  if (ret != ESP_OK) {
    url[0] = '\0';
    return ret;
  }

  if (url == s_ws.url) {
    return ESP_OK;
  }

  (void)WSHandle_SetUrl(url);
  return ESP_OK;
}

esp_err_t WSHandle_SendText(const char *text, TickType_t timeout_ticks) {
  if (text == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (ws_take_lock(pdMS_TO_TICKS(1000)) != true) {
    return ESP_ERR_TIMEOUT;
  }

  if (s_ws.client == NULL || !esp_websocket_client_is_connected(s_ws.client)) {
    ws_give_lock();
    return ESP_ERR_INVALID_STATE;
  }

  int written = esp_websocket_client_send_text(
      s_ws.client, text, (int)strlen(text), timeout_ticks);
  ws_give_lock();

  return written < 0 ? ESP_FAIL : ESP_OK;
}

esp_err_t WSHandle_SendBinary(const void *data, size_t len,
                              TickType_t timeout_ticks) {
  if (data == NULL || len == 0 || len > INT_MAX) {
    return ESP_ERR_INVALID_ARG;
  }

  if (ws_take_lock(pdMS_TO_TICKS(1000)) != true) {
    return ESP_ERR_TIMEOUT;
  }

  if (s_ws.client == NULL || !esp_websocket_client_is_connected(s_ws.client)) {
    ws_give_lock();
    return ESP_ERR_INVALID_STATE;
  }

  int written = esp_websocket_client_send_bin(
      s_ws.client, (const char *)data, (int)len, timeout_ticks);
  ws_give_lock();

  return written < 0 ? ESP_FAIL : ESP_OK;
}

esp_err_t save_ws_url(const char *url) { return WSHandle_SaveUrl(url); }

esp_err_t load_ws_url(char *url, size_t len) {
  return WSHandle_LoadUrl(url, len);
}

const char *get_ws_url(void) { return WSHandle_GetUrl(); }

void WebSocket_Handler(void *pvParameter) {
  const char *url = (const char *)pvParameter;
  ws_handle_config_t cfg = {
      .uri = url,
      .auto_start = true,
  };
  (void)WSHandle_Init(&cfg);
  vTaskDelete(NULL);
}
