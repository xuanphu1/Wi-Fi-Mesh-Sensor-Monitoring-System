/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WS_INTERNAL_H
#define WS_INTERNAL_H

#include "WSHandle.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "cJSON.h"

#define TAG_WEBSOCKET "WebSocket Handler"
#define GATEWAY_STATUS_INTERVAL_MS 5000
#define WS_RECONNECT_TIMEOUT_MS 15000
#define WS_NETWORK_TIMEOUT_MS 15000
#define WS_MDNS_QUERY_TIMEOUT_MS 3000
#define WS_OTA_PROGRESS_INTERVAL_MS 1000
#define WS_NODE_OTA_TIMEOUT_MS 10000

extern ws_handler_ctx_t *s_ws_ctx;
extern dm_ws_t *s_ws_state;
extern dm_telemetry_t *s_ws_telemetry;
extern esp_websocket_client_handle_t ws_internal_client;
extern SemaphoreHandle_t s_ws_send_mutex;
extern volatile bool s_ws_restart_requested;
extern websocket_target_t s_ws_selected_target;
extern volatile uint32_t s_ws_reconnect_count;
extern volatile bool s_ws_connected_once;
extern volatile bool s_ws_reconnect_pending;
extern volatile bool s_gateway_ota_stop_ws;
extern char ws_url[128];
extern char ws_last_type[32];
extern char ws_last_version[32];

// ScreenManager weak declaration
extern void screen_manager_set_ota_progress(bool active, uint8_t percent,
                                            const char *target, const char *version,
                                            const char *detail) __attribute__((weak));

#endif // WS_INTERNAL_H
