#ifndef WS_HANDLE_H
#define WS_HANDLE_H

#include "esp_err.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WS_HANDLE_URL_MAX_LEN 128
#define WS_HANDLE_DEFAULT_NVS_NAMESPACE "websocket"
#define WS_HANDLE_DEFAULT_NVS_KEY "ws_url"

typedef enum {
  WS_HANDLE_EVENT_CONNECTED = 0,
  WS_HANDLE_EVENT_DISCONNECTED,
  WS_HANDLE_EVENT_DATA,
  WS_HANDLE_EVENT_ERROR,
} ws_handle_event_t;

typedef struct {
  ws_handle_event_t event;
  const uint8_t *data;
  size_t data_len;
  bool is_binary;
  bool fin;
  uint8_t op_code;
  int payload_len;
  int payload_offset;
  const esp_websocket_error_codes_t *error;
} ws_handle_event_data_t;

typedef void (*ws_handle_event_cb_t)(const ws_handle_event_data_t *event,
                                     void *user_ctx);
typedef void (*ws_handle_rx_cb_t)(const uint8_t *data, size_t len,
                                  bool is_binary, void *user_ctx);

typedef struct {
  const char *uri;
  const char *headers;
  int network_timeout_ms;
  int reconnect_timeout_ms;
  size_t ping_interval_sec;
  bool auto_start;
  ws_handle_event_cb_t event_cb;
  ws_handle_rx_cb_t rx_cb;
  void *user_ctx;
} ws_handle_config_t;

esp_err_t WSHandle_Init(const ws_handle_config_t *config);
esp_err_t WSHandle_SetCallbacks(ws_handle_event_cb_t event_cb,
                                ws_handle_rx_cb_t rx_cb, void *user_ctx);
esp_err_t WSHandle_Start(void);
esp_err_t WSHandle_Stop(void);
esp_err_t WSHandle_Deinit(void);

bool WSHandle_IsStarted(void);
bool WSHandle_IsConnected(void);
esp_websocket_client_handle_t WSHandle_GetClient(void);

esp_err_t WSHandle_SetUrl(const char *url);
const char *WSHandle_GetUrl(void);
esp_err_t WSHandle_SaveUrl(const char *url);
esp_err_t WSHandle_LoadUrl(char *url, size_t len);

esp_err_t WSHandle_SendText(const char *text, TickType_t timeout_ticks);
esp_err_t WSHandle_SendBinary(const void *data, size_t len,
                              TickType_t timeout_ticks);

/* Compatibility wrappers used by the existing WiFi config page. */
esp_err_t save_ws_url(const char *url);
esp_err_t load_ws_url(char *url, size_t len);
const char *get_ws_url(void);

/* Optional FreeRTOS entry point: init/start once, then delete itself. */
void WebSocket_Handler(void *pvParameter);

#ifdef __cplusplus
}
#endif

#endif /* WS_HANDLE_H */
