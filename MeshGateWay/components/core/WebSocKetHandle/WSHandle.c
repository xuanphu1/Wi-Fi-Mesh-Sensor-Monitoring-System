/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "WSHandle.h"
#include "ws_internal.h"
#include "ws_transport.h"
#include "ws_protocol.h"
#include "ws_time_sync.h"
#include "ws_ota.h"

#include "FOTAManager.h"
#include "WifiManager.h"
#include "UartToNode.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

esp_websocket_client_handle_t ws_internal_client = NULL;

ws_handler_ctx_t *s_ws_ctx = NULL;
dm_ws_t *s_ws_state = NULL;
dm_telemetry_t *s_ws_telemetry = NULL;
char ws_last_type[32] = {0};
char ws_last_version[32] = {0};
char ws_url[128] = {0};
volatile bool s_ws_restart_requested = false;

#if defined(CONFIG_WS_TARGET_SERVER)
websocket_target_t s_ws_selected_target = WEBSOCKET_TARGET_SERVER;
#elif defined(CONFIG_WS_TARGET_LOCAL)
websocket_target_t s_ws_selected_target = WEBSOCKET_TARGET_LOCAL;
#else
websocket_target_t s_ws_selected_target = WEBSOCKET_TARGET_LOCAL;
#endif

volatile uint32_t s_ws_reconnect_count = 0;
volatile bool s_ws_connected_once = false;
volatile bool s_ws_reconnect_pending = false;
volatile bool s_gateway_ota_stop_ws = false;

SemaphoreHandle_t s_ws_send_mutex = NULL;

void websocket_attach_state(dm_ws_t *ws_state) {
  s_ws_state = ws_state;
}

esp_err_t save_ws_url(const char *url) {
  if (url == NULL || url[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }

  strncpy(ws_url, url, sizeof(ws_url) - 1);
  ws_url[sizeof(ws_url) - 1] = '\0';

  if (strstr(url, "msems.click") != NULL || strncmp(url, "wss://", 6) == 0) {
    s_ws_selected_target = WEBSOCKET_TARGET_SERVER;
  } else if (strstr(url, "systemmsems.local") != NULL) {
    s_ws_selected_target = WEBSOCKET_TARGET_LOCAL;
  } else {
    s_ws_selected_target = WEBSOCKET_TARGET_CUSTOM;
  }

  ESP_LOGI(TAG_WEBSOCKET, "WebSocket URL (RAM): %s (target: %s)", ws_url,
           s_ws_selected_target == WEBSOCKET_TARGET_LOCAL
               ? "local"
               : (s_ws_selected_target == WEBSOCKET_TARGET_SERVER ? "server" : "custom"));

  s_ws_restart_requested = true;

  if (s_ws_state) {
    strncpy(s_ws_state->url_cached, ws_url, sizeof(s_ws_state->url_cached) - 1);
    s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
  }

  return ESP_OK;
}

esp_err_t websocket_select_target(websocket_target_t target) {
  if (target != WEBSOCKET_TARGET_CUSTOM && target != WEBSOCKET_TARGET_LOCAL &&
      target != WEBSOCKET_TARGET_SERVER) {
    return ESP_ERR_INVALID_ARG;
  }

  s_ws_selected_target = target;
  s_ws_restart_requested = true;

  ESP_LOGI(TAG_WEBSOCKET, "WebSocket target selected (RAM): %s",
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
  return s_ws_selected_target;
}

const char *get_ws_url(void) {
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

bool websocket_is_connected(void) {
  return s_ws_state != NULL && s_ws_state->connected;
}

uint32_t websocket_get_reconnect_count(void) {
  return s_ws_reconnect_count;
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
  TickType_t last_time_sync_tick = 0;
  TickType_t last_ota_progress_tick = 0;
  uint8_t last_ota_percent_sent = 255;
  bool last_ota_running_sent = false;

  for (;;) {
    if (s_gateway_ota_stop_ws) {
      s_gateway_ota_stop_ws = false;
      if (ws_internal_client != NULL) {
        ESP_LOGI(TAG_WEBSOCKET, "Safely stopping WebSocket client to free TLS RAM for Gateway OTA");
        ws_transport_destroy_current();
        if (s_ws_state) {
          s_ws_state->connected = false;
        }
      }
    }

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
        if (ws_internal_client != NULL) {
          ESP_LOGI(TAG_WEBSOCKET,
                   "Restarting WebSocket client to apply new URL");
          if (s_ws_connected_once) {
            s_ws_reconnect_pending = true;
          }
          ws_transport_destroy_current();
        }
        if (s_ws_state) {
          s_ws_state->connected = false;
        }
      }

      if (is_wifi_connected()) {
        if (ws_internal_client == NULL) {
          ws_transport_start();
        }
      } else {
        if (ws_internal_client != NULL) {
          ESP_LOGW(TAG_WEBSOCKET, "WiFi not connected — stop WebSocket");
          ws_transport_destroy_current();
          if (s_ws_state) {
            s_ws_state->connected = false;
          }
        }
      }
    }

    if (ctx != NULL && (xTaskGetTickCount() - last_gateway_tick) >=
                           pdMS_TO_TICKS(GATEWAY_STATUS_INTERVAL_MS)) {
      last_gateway_tick = xTaskGetTickCount();
      if (!fota_is_running() && ws_internal_client != NULL &&
          esp_websocket_client_is_connected(ws_internal_client) && is_wifi_connected()) {
        ws_send_gateway_status(ctx);
      }
    }

    // Kiểm tra timeout 10s cho Node/Root OTA nếu không nhận được phản hồi tiến trình
    ws_ota_check_node_timeout();

    // Định kỳ gửi lại yêu cầu đồng bộ thời gian (30s nếu chưa sync, 1h nếu đã sync)
    uint32_t sync_interval_ms = websocket_is_time_synced() ? (3600 * 1000) : 30000;
    if ((xTaskGetTickCount() - last_time_sync_tick) >= pdMS_TO_TICKS(sync_interval_ms)) {
      last_time_sync_tick = xTaskGetTickCount();
      if (!fota_is_running() && ws_internal_client != NULL &&
          esp_websocket_client_is_connected(ws_internal_client) && is_wifi_connected()) {
        ws_send_time_sync_request();
      }
    }
  }
}
