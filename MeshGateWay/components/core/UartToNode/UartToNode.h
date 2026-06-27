/*
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "Datamanager.h"

#ifdef __cplusplus
extern "C" {
#endif

void uart_to_node_attach_uplink_queue(QueueHandle_t uplink_queue);
void uart_to_node_attach_ws_state(dm_ws_t *ws_state);
void uart_to_node_attach_telemetry(dm_telemetry_t *telemetry);

/** Lấy độ dài dữ liệu đang nằm trong hardware rx buffer / ringbuffer */
size_t uart_to_node_get_buffered_len(void);

void uart_to_node_get_queue_status(uint32_t *used, uint32_t *total);

esp_err_t uart_to_node_start(void);

#ifdef __cplusplus
}
#endif
