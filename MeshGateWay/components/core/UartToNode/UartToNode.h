/*
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#include "Datamanager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Gắn queue uplink để đẩy RX UART và chuyển lên server. Gọi trước `uart_to_node_start`. */
void uart_to_node_attach_uplink_queue(QueueHandle_t uplink_queue);

void uart_to_node_attach_ws_state(dm_ws_t *ws_state);

/**
 * @brief Khởi động task UART handshake với node.
 *
 * Luồng:
 * - Gửi "Start Root" định kỳ cho đến khi RX khớp **nguyên vẹn** (byte-for-byte) chuỗi wire
 *   `Switching to root mode...\r\n` (cùng nội dung với `UartToGateWay_Send` phía node).
 * - Sau đó gửi "Connected" định kỳ.
 * - Nếu sau khi đã connected mà quá lâu không RX từ node -> quay lại gửi "Start Root".
 */
esp_err_t uart_to_node_start(void);

#ifdef __cplusplus
}
#endif

