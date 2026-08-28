/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "WSHandle.h"

#include "FOTAManager.h"
#include "WifiManager.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"

#include "cJSON.h"

static esp_websocket_client_handle_t client = NULL;

static ws_handler_ctx_t *s_ws_ctx = NULL;
static dm_ws_t *s_ws_state = NULL;
static dm_telemetry_t *s_ws_telemetry = NULL;
static char ws_last_type[32] = {0};
static char ws_last_version[32] = {0};
static char ws_url[128] = {0};
static char ws_resolved_url[128] = {0};
static volatile bool s_ws_restart_requested = false;
static websocket_target_t s_ws_selected_target = WEBSOCKET_TARGET_CUSTOM;
static volatile uint32_t s_ws_reconnect_count = 0;
static volatile bool s_ws_connected_once = false;
static volatile bool s_ws_reconnect_pending = false;

#define TAG_WEBSOCKET "WebSocket Handler"
#define GATEWAY_STATUS_INTERVAL_MS 5000
#define WS_RECONNECT_TIMEOUT_MS 15000
#define WS_NETWORK_TIMEOUT_MS 15000
#define WS_MDNS_QUERY_TIMEOUT_MS 3000
#define WS_OTA_PROGRESS_INTERVAL_MS 1000

static bool ws_client_can_send(void) {
  return client != NULL && esp_websocket_client_is_connected(client) &&
         is_wifi_connected();
}

static void ws_count_tx(int bytes) {
  if (bytes >= 0 && s_ws_telemetry) {
    s_ws_telemetry->tx_packet_count++;
    s_ws_telemetry->tx_byte_count += (uint32_t)bytes;
  }
}

static void ws_send_ota_gateway_progress(void) {
  if (!ws_client_can_send()) {
    return;
  }

  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return;
  }

  uint8_t percent = fota_get_progress_percent();
  bool running = fota_is_running();
  esp_err_t result = fota_get_last_result();

  cJSON_AddStringToObject(root, "type", "ota_gateway_progress");
  cJSON_AddStringToObject(root, "clientType", "esp32");
  cJSON_AddNumberToObject(root, "percent", (double)percent);
  cJSON_AddBoolToObject(root, "running", running ? 1 : 0);
  cJSON_AddNumberToObject(root, "result", (double)result);
  cJSON_AddStringToObject(root, "result_name", esp_err_to_name(result));

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json_str != NULL) {
    int ret = esp_websocket_client_send_text(client, json_str, strlen(json_str),
                                             pdMS_TO_TICKS(2000));
    ws_count_tx(ret);
    free(json_str);
  }
}

static bool ws_json_type_matches(const cJSON *type) {
  if (!cJSON_IsString(type) || type->valuestring == NULL) {
    return false;
  }
  return strcmp(type->valuestring, "ota") == 0 ||
         strcmp(type->valuestring, "ota_gateway") == 0 ||
         strcmp(type->valuestring, "gateway_ota") == 0;
}

static const char *ws_json_get_ota_url(cJSON *root) {
  static const char *keys[] = {"url", "ota_url", "firmware_url", "line"};
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    cJSON *item = cJSON_GetObjectItem(root, keys[i]);
    if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
      return item->valuestring;
    }
  }
  return NULL;
}

static void ws_handle_ota_command(cJSON *root) {
  const char *url = ws_json_get_ota_url(root);
  if (url == NULL) {
    ESP_LOGW(TAG_WEBSOCKET, "OTA command ignored: missing URL");
    return;
  }

  esp_err_t ret = fota_start_gateway_ota(url);
  if (ret == ESP_OK) {
    ESP_LOGW(TAG_WEBSOCKET, "Gateway OTA started: %s", url);
  } else {
    ESP_LOGE(TAG_WEBSOCKET, "Gateway OTA start failed: %s",
             esp_err_to_name(ret));
  }
  ws_send_ota_gateway_progress();
}

static const char *resolve_local_ws_url(const char *url) {
  static const char ws_prefix[] = "ws://";
  static const char local_suffix[] = ".local";

  if (url == NULL || strncmp(url, ws_prefix, sizeof(ws_prefix) - 1) != 0) {
    return url;
  }

  const char *host_begin = url + sizeof(ws_prefix) - 1;
  const char *suffix = strstr(host_begin, local_suffix);
  if (suffix == NULL) {
    return url;
  }

  const char *tail = suffix + sizeof(local_suffix) - 1;
  if (*tail != '\0' && *tail != ':' && *tail != '/') {
    return url;
  }

  const size_t hostname_len = (size_t)(suffix - host_begin);
  if (hostname_len == 0 || hostname_len >= 64) {
    ESP_LOGE(TAG_WEBSOCKET, "Invalid mDNS hostname in URL: %s", url);
    return NULL;
  }

  char hostname[64];
  memcpy(hostname, host_begin, hostname_len);
  hostname[hostname_len] = '\0';

  esp_ip4_addr_t address = {0};
  esp_err_t err =
      mdns_query_a(hostname, WS_MDNS_QUERY_TIMEOUT_MS, &address);
  if (err != ESP_OK) {
    ESP_LOGW(TAG_WEBSOCKET, "mDNS lookup failed for %s.local: %s", hostname,
             esp_err_to_name(err));
    return NULL;
  }

  int written = snprintf(ws_resolved_url, sizeof(ws_resolved_url),
                         "ws://" IPSTR "%s", IP2STR(&address), tail);
  if (written < 0 || (size_t)written >= sizeof(ws_resolved_url)) {
    ESP_LOGE(TAG_WEBSOCKET, "Resolved WebSocket URL is too long");
    return NULL;
  }

  ESP_LOGI(TAG_WEBSOCKET, "mDNS resolved %s.local to " IPSTR, hostname,
           IP2STR(&address));
  return ws_resolved_url;
}

static void websocket_client_destroy_current(void) {
  if (client == NULL) {
    return;
  }

  esp_err_t err = esp_websocket_client_destroy(client);
  if (err != ESP_OK) {
    ESP_LOGW(TAG_WEBSOCKET, "WebSocket destroy failed: %s",
             esp_err_to_name(err));
  }
  client = NULL;
}

static const char *gateway_power_source_str(uint32_t pack_mv,
                                            bool have_voltage) {
  if (!have_voltage || pack_mv < 2000U) {
    return "unknown";
  }
  /* Heuristic: nguồn ngoài/USB thường đẩy điện áp gói cao hơn khi đang nạp —
   * tinh chỉnh theo mạch thực tế. */
  if (pack_mv >= 4180U) {
    return "AC_or_USB";
  }
  return "battery";
}

static void ws_send_gateway_status(ws_handler_ctx_t *ctx) {
  if (client == NULL || !esp_websocket_client_is_connected(client) ||
      ctx == NULL || ctx->metrics == NULL) {
    return;
  }

  ui_metrics_t m = {0};
  if (ctx->metrics->mutex == NULL ||
      xSemaphoreTake(ctx->metrics->mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return;
  }
  m = ctx->metrics->value;
  xSemaphoreGive(ctx->metrics->mutex);

  /*
   * --- Payload WebSocket: gateway_status (JSON text, gửi định kỳ ~5s khi
   * WS+STA OK) --- Đồng bộ schema phía server/UI: number = JSON number, string
   * = string, null = không có dữ liệu.
   *
   * {
   *   "type":               string, luôn "gateway_status".
   *   "clientType":         string, luôn "esp32".
   *
   *   "cpu_load_percent":   number, tải CPU 0..100 (%), suy ra từ
   * cpu_load_permille / 10 (SystemMonitor). "ram_used_kb":        number, RAM
   * đã dùng (kB, heap internal). "ram_used_percent":   number, % RAM đã dùng
   * 0..100 (= 100 - ram_free_percent từ metrics).
   *
   *   "battery_voltage_v":  number, điện áp gói pin ước lượng (V), từ ADC chia
   * áp (0 nếu chưa đo). "battery_percent":    number, % pin 0..100.
   *   "power_source":       string, "unknown" | "AC_or_USB" | "battery"
   * (heuristic, gateway_power_source_str).
   *
   *   "chip_temp_c":        number | null, °C nếu chip_temp_valid; else null.
   *   "chip_temp_internal_supported": bool, true chỉ khi SoC có driver TSENS
   * trong IDF (CONFIG_SOC_TEMP_SENSOR_SUPPORTED). ESP32 classic = false nên
   * chip_temp_c luôn null dù firmware đúng.
   *
   *   "uptime_s":           number, giây từ boot/reset (SystemMonitor /
   * esp_timer).
   *
   *   "wifi_ssid":          string, SSID AP đang bám; rỗng nếu không đọc được.
   *   "wifi_rssi":          number, RSSI dBm (âm), từ esp_wifi_sta_get_ap_info;
   * 0 nếu không có AP. "sta_ip":             string, IPv4 STA "x.x.x.x"; rỗng
   * nếu chưa có. "sta_gateway":        string, gateway IPv4 STA; rỗng nếu không
   * đọc được.
   * }
   */
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return;
  }

  cJSON_AddStringToObject(root, "type", "gateway_status");
  cJSON_AddStringToObject(root, "clientType", "esp32");
  cJSON_AddNumberToObject(root, "cpu_load_percent",
                          (double)m.cpu_load_permille / 10.0);

  cJSON_AddNumberToObject(root, "ram_used_kb", (double)m.ram_used_kb);
  {
    uint32_t ram_used_pct = 0U;
    if (m.ram_free_percent <= 100U) {
      ram_used_pct = 100U - m.ram_free_percent;
    }
    cJSON_AddNumberToObject(root, "ram_used_percent", (double)ram_used_pct);
  }

  const bool have_v = (m.battery_pack_mv > 0U);
  const double bat_v = have_v ? (double)m.battery_pack_mv / 1000.0 : 0.0;
  cJSON_AddNumberToObject(root, "battery_voltage_v", bat_v);
  cJSON_AddNumberToObject(root, "battery_percent", (double)m.battery_pct);
  cJSON_AddStringToObject(root, "power_source",
                          gateway_power_source_str(m.battery_pack_mv, have_v));

  cJSON_AddBoolToObject(root, "chip_temp_internal_supported",
                        m.chip_temp_internal_supported ? 1 : 0);
  if (m.chip_temp_valid) {
    cJSON_AddNumberToObject(root, "chip_temp_c", (double)m.chip_temp_c);
  } else {
    cJSON_AddNullToObject(root, "chip_temp_c");
  }

  cJSON_AddNumberToObject(root, "uptime_s", (double)m.uptime_s);

  wifi_ap_record_t ap = {0};
  const bool ap_ok = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);
  if (ap_ok) {
    cJSON_AddStringToObject(root, "wifi_ssid", (const char *)ap.ssid);
    cJSON_AddNumberToObject(root, "wifi_rssi", (double)ap.rssi);
  } else {
    cJSON_AddStringToObject(root, "wifi_ssid", "");
    cJSON_AddNumberToObject(root, "wifi_rssi", 0.0);
  }

  char ip_buf[20] = "";
  char gw_buf[20] = "";
  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta_netif != NULL) {
    esp_netif_ip_info_t ipi;
    if (esp_netif_get_ip_info(sta_netif, &ipi) == ESP_OK) {
      snprintf(ip_buf, sizeof(ip_buf), IPSTR, IP2STR(&ipi.ip));
      snprintf(gw_buf, sizeof(gw_buf), IPSTR, IP2STR(&ipi.gw));
    }
  }
  cJSON_AddStringToObject(root, "sta_ip", ip_buf);
  cJSON_AddStringToObject(root, "sta_gateway", gw_buf);

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json_str != NULL) {
    // ESP_LOGI(TAG_WEBSOCKET, "gateway_status send: %s", json_str);
    int ret = esp_websocket_client_send_text(client, json_str, strlen(json_str),
                                             pdMS_TO_TICKS(2000));
    if (ret >= 0 && ctx->telemetry) {
      ctx->telemetry->tx_packet_count++;
      ctx->telemetry->tx_byte_count += strlen(json_str);
    }
    free(json_str);
  }
}

void websocket_attach_state(dm_ws_t *ws_state) { s_ws_state = ws_state; }

/** Chỉ RAM — không ghi NVS/flash; mất khi reset. */
esp_err_t save_ws_url(const char *url) {
  if (url == NULL || url[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }

  strncpy(ws_url, url, sizeof(ws_url) - 1);
  ws_url[sizeof(ws_url) - 1] = '\0';
  s_ws_selected_target = WEBSOCKET_TARGET_CUSTOM;
  ESP_LOGI(TAG_WEBSOCKET, "WebSocket URL (RAM): %s", ws_url);

  s_ws_restart_requested = true;

  if (s_ws_state) {
    strncpy(s_ws_state->url_cached, ws_url, sizeof(s_ws_state->url_cached) - 1);
    s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
  }

  return ESP_OK;
}

static void load_ws_target_from_nvs(void) {
  static bool s_loaded = false;
  if (s_loaded) {
    return;
  }
  s_loaded = true;

  nvs_handle_t nvs;
  if (nvs_open("ws_cfg", NVS_READONLY, &nvs) == ESP_OK) {
    uint8_t target = 0;
    if (nvs_get_u8(nvs, "target", &target) == ESP_OK) {
      if (target == WEBSOCKET_TARGET_LOCAL || target == WEBSOCKET_TARGET_SERVER ||
          target == WEBSOCKET_TARGET_CUSTOM) {
        s_ws_selected_target = (websocket_target_t)target;
        ESP_LOGI(TAG_WEBSOCKET, "Loaded WebSocket target from NVS: %s",
                 target == WEBSOCKET_TARGET_LOCAL
                     ? "local"
                     : (target == WEBSOCKET_TARGET_SERVER ? "server" : "custom"));
      }
    }
    nvs_close(nvs);
  }
}

static void save_ws_target_to_nvs(websocket_target_t target) {
  nvs_handle_t nvs;
  if (nvs_open("ws_cfg", NVS_READWRITE, &nvs) == ESP_OK) {
    nvs_set_u8(nvs, "target", (uint8_t)target);
    nvs_commit(nvs);
    nvs_close(nvs);
  }
}

esp_err_t websocket_select_target(websocket_target_t target) {
  if (target != WEBSOCKET_TARGET_CUSTOM && target != WEBSOCKET_TARGET_LOCAL &&
      target != WEBSOCKET_TARGET_SERVER) {
    return ESP_ERR_INVALID_ARG;
  }

  s_ws_selected_target = target;
  s_ws_restart_requested = true;

  save_ws_target_to_nvs(target);

  ESP_LOGI(TAG_WEBSOCKET, "WebSocket target selected and saved to NVS: %s",
           target == WEBSOCKET_TARGET_LOCAL
               ? "local"
               : (target == WEBSOCKET_TARGET_SERVER ? "server" : "custom"));

  if (s_ws_state) {
    const char *url = get_ws_url();
    strncpy(s_ws_state->url_cached, url, sizeof(s_ws_state->url_cached) - 1);
    s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
  }

  return ESP_OK;
}

websocket_target_t websocket_get_selected_target(void) {
  load_ws_target_from_nvs();
  return s_ws_selected_target;
}

const char *get_ws_url(void) {
  load_ws_target_from_nvs();
  if (s_ws_selected_target == WEBSOCKET_TARGET_LOCAL) {
#if defined(CONFIG_WS_LOCAL_URL)
    return CONFIG_WS_LOCAL_URL;
#else
    return "ws://systemmsems.local:9090/ws";
#endif
  }

  if (s_ws_selected_target == WEBSOCKET_TARGET_SERVER) {
#if defined(CONFIG_WS_SERVER_URL)
    return CONFIG_WS_SERVER_URL;
#else
    return "wss://systemmsems.msems.click/ws";
#endif
  }

  if (ws_url[0] != '\0') {
    return ws_url;
  }
#if defined(CONFIG_WS_TARGET_LOCAL) && defined(CONFIG_WS_LOCAL_URL)
  return CONFIG_WS_LOCAL_URL;
#elif defined(CONFIG_WS_TARGET_SERVER) && defined(CONFIG_WS_SERVER_URL)
  return CONFIG_WS_SERVER_URL;
#elif defined(CONFIG_WS_URL)
  /* Backward compatibility with an existing sdkconfig. */
  return CONFIG_WS_URL;
#else
  return "wss://systemmsems.msems.click/ws";
#endif
}

/*
 * Hàm ws_handle_sync_time: Xử lý bản tin đồng bộ thời gian từ Server
 */
static void ws_handle_sync_time(cJSON *root) {
  if (root == NULL) {
    return;
  }

  time_t sec = 0;
  bool have_time = false;

  cJSON *j_ts = cJSON_GetObjectItem(root, "timestamp");
  if (!j_ts) j_ts = cJSON_GetObjectItem(root, "timestamp_ms");
  if (!j_ts) j_ts = cJSON_GetObjectItem(root, "time");
  if (!j_ts) j_ts = cJSON_GetObjectItem(root, "epoch");
  if (!j_ts) j_ts = cJSON_GetObjectItem(root, "timer");

  if (cJSON_IsNumber(j_ts)) {
    double val = j_ts->valuedouble;
    if (val > 1e11) {
      sec = (time_t)(val / 1000.0);
    } else {
      sec = (time_t)val;
    }
    have_time = true;
  } else if (cJSON_IsString(j_ts) && j_ts->valuestring) {
    sec = (time_t)atoll(j_ts->valuestring);
    if (sec > 0) {
      have_time = true;
    }
  }

  cJSON *j_year = cJSON_GetObjectItem(root, "year");
  cJSON *j_month = cJSON_GetObjectItem(root, "month");
  cJSON *j_day = cJSON_GetObjectItem(root, "day");
  cJSON *j_hour = cJSON_GetObjectItem(root, "hour");
  cJSON *j_min = cJSON_GetObjectItem(root, "min");
  cJSON *j_sec = cJSON_GetObjectItem(root, "sec");

  struct tm tm_info = {0};

  if (!have_time && cJSON_IsNumber(j_year) && cJSON_IsNumber(j_month) && cJSON_IsNumber(j_day)) {
    tm_info.tm_year = j_year->valueint - 1900;
    tm_info.tm_mon = j_month->valueint - 1;
    tm_info.tm_mday = j_day->valueint;
    tm_info.tm_hour = cJSON_IsNumber(j_hour) ? j_hour->valueint : 0;
    tm_info.tm_min = cJSON_IsNumber(j_min) ? j_min->valueint : 0;
    tm_info.tm_sec = cJSON_IsNumber(j_sec) ? j_sec->valueint : 0;
    sec = mktime(&tm_info);
    have_time = true;
  }

  if (!have_time || sec <= 1000000000) {
    ESP_LOGW(TAG_WEBSOCKET, "Time sync payload received but timestamp is invalid");
    return;
  }

  // Cập nhật System Time ESP32
  struct timeval tv = {.tv_sec = sec, .tv_usec = 0};
  settimeofday(&tv, NULL);

  localtime_r(&sec, &tm_info);

  char time_buf[64];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);
  ESP_LOGI(TAG_WEBSOCKET, "System time synchronized via WebSocket: %s (timestamp: %ld)",
           time_buf, (long)sec);

  // Cập nhật chip RTC DS3231 nếu có phần cứng sẵn sàng
  if (s_ws_ctx && s_ws_ctx->hw && s_ws_ctx->hw->rtc_ready) {
    esp_err_t err = ds3231_set_time(&s_ws_ctx->hw->rtc_dev, &tm_info);
    if (err == ESP_OK) {
      ESP_LOGI(TAG_WEBSOCKET, "DS3231 RTC hardware updated from WebSocket time sync");
    } else {
      ESP_LOGW(TAG_WEBSOCKET, "Failed to update DS3231 RTC: %s", esp_err_to_name(err));
    }
  }
}

void SendSignalRegister(void) {
  if (client == NULL) {
    return;
  }
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "type", "register");
  cJSON_AddStringToObject(data, "clientType", "esp32");

  char *json_str = cJSON_PrintUnformatted(data);
  if (json_str) {
    int ret = esp_websocket_client_send_text(client, json_str, strlen(json_str),
                                             portMAX_DELAY);
    if (ret >= 0 && s_ws_telemetry) {
      s_ws_telemetry->tx_packet_count++;
      s_ws_telemetry->tx_byte_count += strlen(json_str);
    }
    free(json_str);
  }
  cJSON_Delete(data);

  // Gửi bản tin yêu cầu Server đồng bộ thời gian (Time Sync Request)
  cJSON *time_req = cJSON_CreateObject();
  cJSON_AddStringToObject(time_req, "type", "request_time_sync");
  cJSON_AddStringToObject(time_req, "clientType", "esp32");
  char *time_req_str = cJSON_PrintUnformatted(time_req);
  if (time_req_str) {
    int ret = esp_websocket_client_send_text(client, time_req_str, strlen(time_req_str),
                                             pdMS_TO_TICKS(2000));
    if (ret >= 0 && s_ws_telemetry) {
      s_ws_telemetry->tx_packet_count++;
      s_ws_telemetry->tx_byte_count += strlen(time_req_str);
    }
    free(time_req_str);
  }
  cJSON_Delete(time_req);
}

static void websocket_event_handler(void *arg, esp_event_base_t base,
                                    int32_t event_id, void *event_data) {
  (void)arg;
  (void)base;
  esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

  switch (event_id) {

  case WEBSOCKET_EVENT_CONNECTED:
    ESP_LOGI(TAG_WEBSOCKET, "Connected to server");
    if (s_ws_connected_once && s_ws_reconnect_pending) {
      s_ws_reconnect_count++;
    }
    s_ws_connected_once = true;
    s_ws_reconnect_pending = false;
    if (s_ws_state) {
      s_ws_state->connected = true;
    }
    SendSignalRegister();
    break;

  case WEBSOCKET_EVENT_DISCONNECTED:
    ESP_LOGW(TAG_WEBSOCKET, "Disconnected from server");
    if (s_ws_connected_once) {
      s_ws_reconnect_pending = true;
    }
    if (s_ws_state) {
      s_ws_state->connected = false;
    }
    break;

  case WEBSOCKET_EVENT_DATA:
    ESP_LOGI(TAG_WEBSOCKET, "WEBSOCKET_EVENT_DATA");
    if (data && data->data_ptr && data->data_len > 0) {
      size_t len = (size_t)data->data_len;
      char *json_buf = (char *)malloc(len + 1);
      if (json_buf) {
        memcpy(json_buf, data->data_ptr, len);
        json_buf[len] = '\0';

        cJSON *root = cJSON_Parse(json_buf);
        if (root) {
          const cJSON *j_type = cJSON_GetObjectItem(root, "type");
          const cJSON *j_version = cJSON_GetObjectItem(root, "version");
          if (ws_json_type_matches(j_type)) {
            ws_handle_ota_command(root);
          }
          if (cJSON_IsString(j_type)) {
            const char *t_str = j_type->valuestring;
            strncpy(ws_last_type, t_str, sizeof(ws_last_type) - 1);
            ws_last_type[sizeof(ws_last_type) - 1] = '\0';

            if (strcmp(t_str, "sync_time") == 0 ||
                strcmp(t_str, "time_sync") == 0 ||
                strcmp(t_str, "time") == 0 ||
                strcmp(t_str, "timer") == 0 ||
                strcmp(t_str, "set_time") == 0) {
              ws_handle_sync_time(root);
            }
          }
          if (cJSON_IsString(j_version)) {
            strncpy(ws_last_version, j_version->valuestring,
                    sizeof(ws_last_version) - 1);
            ws_last_version[sizeof(ws_last_version) - 1] = '\0';
            ESP_LOGI(TAG_WEBSOCKET,
                     "Server version: %s (FOTA stub — không gọi OTA)",
                     ws_last_version);
          }
          cJSON_Delete(root);
        }
        free(json_buf);
      }
    }
    break;

  case WEBSOCKET_EVENT_ERROR:
    ESP_LOGE(TAG_WEBSOCKET, "WebSocket error occurred");
    if (s_ws_connected_once) {
      s_ws_reconnect_pending = true;
    }
    if (s_ws_state) {
      s_ws_state->connected = false;
    }
}

static void websocket_app_start(void) {
  const char *configured_url = get_ws_url();
  const char *url_to_use = resolve_local_ws_url(configured_url);
  if (url_to_use == NULL) {
    return;
  }
  const bool use_tls = strncmp(url_to_use, "wss://", 6) == 0;

  esp_websocket_client_config_t websocket_cfg = {
      .uri = url_to_use,
      .reconnect_timeout_ms = WS_RECONNECT_TIMEOUT_MS,
      .network_timeout_ms = WS_NETWORK_TIMEOUT_MS,
      .task_stack = 6144,
      .buffer_size = 2048,
      .keep_alive_enable = true,
      .crt_bundle_attach = use_tls ? esp_crt_bundle_attach : NULL,
  };

  ESP_LOGI(TAG_WEBSOCKET, "Starting WebSocket (%s) with URL: %s",
           use_tls ? "TLS" : "plain", url_to_use);

  if (s_ws_state) {
    strncpy(s_ws_state->url_cached, url_to_use,
            sizeof(s_ws_state->url_cached) - 1);
    s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
  }

  client = esp_websocket_client_init(&websocket_cfg);
  if (client == NULL) {
    ESP_LOGE(TAG_WEBSOCKET, "esp_websocket_client_init failed");
    return;
  }
  esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY,
                                websocket_event_handler, NULL);
  esp_websocket_client_start(client);
  ESP_LOGI(TAG_WEBSOCKET, "WebSocket client start issued");
}

/** Gửi một dòng UART đã ghép hoàn chỉnh lên WS. */
static void ws_send_uart_rx_payload(const char *payload, size_t payload_len) {
  if (!client || !payload || payload_len == 0) {
    return;
  }
  if (!esp_websocket_client_is_connected(client)) {
    return;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "type", "uart_rx");
  cJSON_AddNumberToObject(root, "len", (double)payload_len);
  cJSON_AddStringToObject(root, "payload", payload);

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json_str) {
    int ret = esp_websocket_client_send_text(client, json_str, strlen(json_str),
                                             pdMS_TO_TICKS(2000));
    if (ret >= 0 && s_ws_telemetry) {
      s_ws_telemetry->tx_packet_count++;
      s_ws_telemetry->tx_byte_count += strlen(json_str);
    }
    // ESP_LOGI(TAG_WEBSOCKET, "WebSocket send UART RX item: %s", json_str);
    free(json_str);
  }
}

/** Nhận chunk UART từ queue, ghép theo '\n' rồi mới gửi WS để tránh cắt bản
 * tin. */
static void ws_send_uart_rx_item(const uart_node_rx_item_t *item) {
  enum { WS_UART_LINE_BUF_SZ = 2048 };
  static char s_ws_uart_line[WS_UART_LINE_BUF_SZ];
  static size_t s_ws_uart_line_len = 0;

  if (!item || item->len == 0) {
    return;
  }

  for (size_t i = 0; i < item->len; i++) {
    unsigned char c = item->data[i];

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (s_ws_uart_line_len > 0) {
        s_ws_uart_line[s_ws_uart_line_len] = '\0';
        ws_send_uart_rx_payload(s_ws_uart_line, s_ws_uart_line_len);
        s_ws_uart_line_len = 0;
      }
      continue;
    }

    if (!isprint(c) && c != '\t') {
      c = '.';
    }

    if (s_ws_uart_line_len >= (WS_UART_LINE_BUF_SZ - 1U)) {
      s_ws_uart_line[s_ws_uart_line_len] = '\0';
      ESP_LOGW(TAG_WEBSOCKET, "WS UART line too long, send truncated payload");
      ws_send_uart_rx_payload(s_ws_uart_line, s_ws_uart_line_len);
      s_ws_uart_line_len = 0;
    }


  if (s_ws_state) {
    strncpy(s_ws_state->url_cached, url_to_use,
            sizeof(s_ws_state->url_cached) - 1);
    s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
  }

  client = esp_websocket_client_init(&websocket_cfg);
  if (client == NULL) {
    ESP_LOGE(TAG_WEBSOCKET, "esp_websocket_client_init failed");
    return;
  }
  esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY,
                                websocket_event_handler, NULL);
  esp_websocket_client_start(client);
  ESP_LOGI(TAG_WEBSOCKET, "WebSocket client start issued");
}

/** Gửi một dòng UART đã ghép hoàn chỉnh lên WS. */
static void ws_send_uart_rx_payload(const char *payload, size_t payload_len) {
  if (!client || !payload || payload_len == 0) {
    return;
  }
  if (!esp_websocket_client_is_connected(client)) {
    return;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "type", "uart_rx");
  cJSON_AddNumberToObject(root, "len", (double)payload_len);
  cJSON_AddStringToObject(root, "payload", payload);

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json_str) {
    int ret = esp_websocket_client_send_text(client, json_str, strlen(json_str),
                                             pdMS_TO_TICKS(2000));
    if (ret >= 0 && s_ws_telemetry) {
      s_ws_telemetry->tx_packet_count++;
      s_ws_telemetry->tx_byte_count += strlen(json_str);
    }
    // ESP_LOGI(TAG_WEBSOCKET, "WebSocket send UART RX item: %s", json_str);
    free(json_str);
  }
}

/** Nhận chunk UART từ queue, ghép theo '\n' rồi mới gửi WS để tránh cắt bản
 * tin. */
static void ws_send_uart_rx_item(const uart_node_rx_item_t *item) {
  enum { WS_UART_LINE_BUF_SZ = 2048 };
  static char s_ws_uart_line[WS_UART_LINE_BUF_SZ];
  static size_t s_ws_uart_line_len = 0;

  if (!item || item->len == 0) {
    return;
  }

  for (size_t i = 0; i < item->len; i++) {
    unsigned char c = item->data[i];

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (s_ws_uart_line_len > 0) {
        s_ws_uart_line[s_ws_uart_line_len] = '\0';
        ws_send_uart_rx_payload(s_ws_uart_line, s_ws_uart_line_len);
        s_ws_uart_line_len = 0;
      }
      continue;
    }

    if (!isprint(c) && c != '\t') {
      c = '.';
    }

    if (s_ws_uart_line_len >= (WS_UART_LINE_BUF_SZ - 1U)) {
      s_ws_uart_line[s_ws_uart_line_len] = '\0';
      ESP_LOGW(TAG_WEBSOCKET, "WS UART line too long, send truncated payload");
      ws_send_uart_rx_payload(s_ws_uart_line, s_ws_uart_line_len);
      s_ws_uart_line_len = 0;
    }

    s_ws_uart_line[s_ws_uart_line_len++] = (char)c;
  }
}

void WebSocket_Handler(void *pvParameter) {
  ws_handler_ctx_t *ctx = (ws_handler_ctx_t *)pvParameter;
  s_ws_ctx = ctx;
  if (ctx && ctx->ws) {
    s_ws_state = ctx->ws;
  }
  if (ctx && ctx->telemetry) {
    s_ws_telemetry = ctx->telemetry;
  }

  TickType_t last_wifi_tick = xTaskGetTickCount();
  TickType_t last_gateway_tick = xTaskGetTickCount();
  TickType_t last_ota_progress_tick = 0;
  uint8_t last_ota_percent_sent = 255;
  bool last_ota_running_sent = false;

  for (;;) {
    uart_node_rx_item_t uart_rx;

    if (!fota_is_running() && ctx && ctx->uart && ctx->uart->uplink_queue) {
      TickType_t qwait = pdMS_TO_TICKS(100);
      if (xQueueReceive(ctx->uart->uplink_queue, &uart_rx, qwait) == pdTRUE) {
        do {
          if (is_wifi_connected()) {
            ws_send_uart_rx_item(&uart_rx);
          }
        } while (xQueueReceive(ctx->uart->uplink_queue, &uart_rx, 0) == pdTRUE);
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    bool ota_running = fota_is_running();
    uint8_t ota_percent = fota_get_progress_percent();
    if ((ota_running || last_ota_running_sent ||
         ota_percent != last_ota_percent_sent) &&
        (xTaskGetTickCount() - last_ota_progress_tick) >=
            pdMS_TO_TICKS(WS_OTA_PROGRESS_INTERVAL_MS)) {
      last_ota_progress_tick = xTaskGetTickCount();
      ws_send_ota_gateway_progress();
      last_ota_percent_sent = ota_percent;
      last_ota_running_sent = ota_running;
    }

    if ((xTaskGetTickCount() - last_wifi_tick) >= pdMS_TO_TICKS(1000)) {
      last_wifi_tick = xTaskGetTickCount();

      if (s_ws_restart_requested) {
        s_ws_restart_requested = false;
        if (client != NULL) {
          ESP_LOGI(TAG_WEBSOCKET,
                   "Restarting WebSocket client to apply new URL");
          if (s_ws_connected_once) {
            s_ws_reconnect_pending = true;
          }
          websocket_client_destroy_current();
        }
        if (s_ws_state) {
          s_ws_state->connected = false;
        }
      }

      if (is_wifi_connected()) {
        if (client == NULL) {
          websocket_app_start();
        }
      } else {
        if (client != NULL) {
          ESP_LOGW(TAG_WEBSOCKET, "WiFi not connected — stop WebSocket");
          websocket_client_destroy_current();
          if (s_ws_state) {
            s_ws_state->connected = false;
          }
        }
      }
    }

    if (ctx != NULL && (xTaskGetTickCount() - last_gateway_tick) >=
                           pdMS_TO_TICKS(GATEWAY_STATUS_INTERVAL_MS)) {
      last_gateway_tick = xTaskGetTickCount();
      if (!fota_is_running() && client != NULL && esp_websocket_client_is_connected(client) &&
          is_wifi_connected()) {
        ws_send_gateway_status(ctx);
      }
    }
  }
}

bool websocket_is_connected(void) {
  return s_ws_state != NULL && s_ws_state->connected;
}

uint32_t websocket_get_reconnect_count(void) {
  return s_ws_reconnect_count;
}
