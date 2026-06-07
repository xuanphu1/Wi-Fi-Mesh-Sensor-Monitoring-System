/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WS_HANDLE_H
#define WS_HANDLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_websocket_client.h"
#include "Datamanager.h"

typedef struct
{
    dm_ws_t *ws;
    dm_telemetry_t *telemetry;
    dm_uart_t *uart;
    dm_metrics_t *metrics;
} ws_handler_ctx_t;

/** Gắn state WS để cập nhật URL cache + trạng thái kết nối. */
void websocket_attach_state(dm_ws_t *ws_state);

void SendSignalRegister(void);
void DataToSever(dm_telemetry_t *telemetry);
void WebSocket_Handler(void *pvParameter);

/** URL chỉ trong RAM (không flash). */
esp_err_t save_ws_url(const char *url);
const char *get_ws_url(void);

#endif /* WS_HANDLE_H */
