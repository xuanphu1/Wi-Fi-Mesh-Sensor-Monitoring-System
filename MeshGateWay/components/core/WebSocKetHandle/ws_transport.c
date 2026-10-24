/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ws_transport.h"
#include "ws_protocol.h"
#include "ws_time_sync.h"
#include "ws_ota.h"
#include "WifiManager.h"
#include "FOTAManager.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mdns.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int ws_send_text_locked(const char *text, size_t len, TickType_t timeout) {
  if (text == NULL || len == 0 || ws_internal_client == NULL || !esp_websocket_client_is_connected(ws_internal_client) || !is_wifi_connected()) {
    return -1;
  }
  if (s_ws_send_mutex == NULL) {
    s_ws_send_mutex = xSemaphoreCreateMutex();
  }
  if (s_ws_send_mutex == NULL || xSemaphoreTake(s_ws_send_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return -1;
  }
  int ret = -1;
  if (ws_internal_client != NULL && esp_websocket_client_is_connected(ws_internal_client) && is_wifi_connected()) {
    ret = esp_websocket_client_send_text(ws_internal_client, text, (int)len, timeout);
    if (ret >= 0 && s_ws_telemetry) {
      s_ws_telemetry->tx_packet_count++;
      s_ws_telemetry->tx_byte_count += (uint32_t)len;
    }
  }
  xSemaphoreGive(s_ws_send_mutex);
  return ret;
}

bool ws_client_can_send(void) {
  return ws_internal_client != NULL && esp_websocket_client_is_connected(ws_internal_client) &&
         is_wifi_connected();
}

const char *ws_transport_get_url_path(const char *url) {
  if (!url) return "/";
  if (url[0] == '/') return url;
  const char *p = strstr(url, "://");
  if (p) {
    p += 3;
    const char *slash = strchr(p, '/');
    if (slash) return slash;
  }
  return "/";
}

const char *ws_transport_resolve_local_url(const char *url) {
  if (url == NULL || url[0] == '\0') {
    return url;
  }

  const char *scheme = NULL;
  size_t scheme_len = 0;
  if (strncmp(url, "ws://", 5) == 0) {
    scheme = "ws://";
    scheme_len = 5;
  } else if (strncmp(url, "wss://", 6) == 0) {
    scheme = "wss://";
    scheme_len = 6;
  } else if (strncmp(url, "http://", 7) == 0) {
    scheme = "http://";
    scheme_len = 7;
  } else if (strncmp(url, "https://", 8) == 0) {
    scheme = "https://";
    scheme_len = 8;
  } else {
    return url;
  }

  const char *local_suffix = ".local";
  const char *host_begin = url + scheme_len;
  const char *suffix = strstr(host_begin, local_suffix);
  if (suffix == NULL) {
    return url;
  }

  const char *tail = suffix + strlen(local_suffix);
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
  esp_err_t err = mdns_query_a(hostname, WS_MDNS_QUERY_TIMEOUT_MS, &address);
  if (err != ESP_OK) {
    ESP_LOGW(TAG_WEBSOCKET, "mDNS lookup failed for %s.local: %s", hostname,
             esp_err_to_name(err));
    return NULL;
  }

  static char s_resolved_url[256];
  int written = snprintf(s_resolved_url, sizeof(s_resolved_url),
                         "%s" IPSTR "%s", scheme, IP2STR(&address), tail);
  if (written < 0 || (size_t)written >= sizeof(s_resolved_url)) {
    ESP_LOGE(TAG_WEBSOCKET, "Resolved URL is too long");
    return NULL;
  }

  ESP_LOGI(TAG_WEBSOCKET, "mDNS resolved %s.local -> " IPSTR " (URL: %s)",
           hostname, IP2STR(&address), s_resolved_url);
  return s_resolved_url;
}

void ws_transport_destroy_current(void) {
  if (ws_internal_client == NULL) {
    return;
  }
  if (s_ws_send_mutex != NULL) {
    xSemaphoreTake(s_ws_send_mutex, pdMS_TO_TICKS(500));
  }
  esp_websocket_client_handle_t temp_client = ws_internal_client;
  ws_internal_client = NULL;
  if (s_ws_send_mutex != NULL) {
    xSemaphoreGive(s_ws_send_mutex);
  }
  esp_err_t err = esp_websocket_client_destroy(temp_client);
  if (err != ESP_OK) {
    ESP_LOGW(TAG_WEBSOCKET, "WebSocket destroy failed: %s",
             esp_err_to_name(err));
  }
}

void ws_transport_event_handler(void *arg, esp_event_base_t base,
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
                strcmp(t_str, "time_sync") == 0 || strcmp(t_str, "time") == 0 ||
                strcmp(t_str, "timer") == 0 || strcmp(t_str, "set_time") == 0 ||
                strcmp(t_str, "get_time") == 0 ||
                strcmp(t_str, "time_response") == 0 ||
                strcmp(t_str, "request_time_sync") == 0) {
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
    break;
  }
}

void ws_transport_start(void) {
  const char *configured_url = get_ws_url();
  const char *url_to_use = ws_transport_resolve_local_url(configured_url);
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

  fota_register_progress_callback(ws_ota_progress_callback);

  ws_internal_client = esp_websocket_client_init(&websocket_cfg);
  if (ws_internal_client == NULL) {
    ESP_LOGE(TAG_WEBSOCKET, "esp_websocket_client_init failed");
    return;
  }
  esp_websocket_register_events(ws_internal_client, WEBSOCKET_EVENT_ANY,
                                ws_transport_event_handler, NULL);
  esp_websocket_client_start(ws_internal_client);
  ESP_LOGI(TAG_WEBSOCKET, "WebSocket client start issued");
}
