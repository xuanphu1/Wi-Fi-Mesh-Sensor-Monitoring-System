/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WS_PROTOCOL_H
#define WS_PROTOCOL_H

#include "ws_internal.h"
#include "UartToNode.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

const char *gateway_power_source_str(uint32_t pack_mv, bool have_voltage);
void ws_send_gateway_status(ws_handler_ctx_t *ctx);
void SendSignalRegister(void);
void ws_send_uart_rx_payload(const char *payload, size_t payload_len);
void ws_send_uart_rx_item(const uart_node_rx_item_t *item);

#endif // WS_PROTOCOL_H
