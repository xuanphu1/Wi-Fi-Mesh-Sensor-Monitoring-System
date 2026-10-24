#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef struct
{
    /**
     * Hàng đợi RX từ node (UartToNode đẩy vào) — WebSocket_Handler đọc và gửi lên server.
     * Khởi tạo trong app_main (`xQueueCreate`). Mỗi phần tử ≤ UART_NODE_RX_DATA_MAX byte.
     */
    QueueHandle_t uplink_queue;
} dm_uart_t;

/** Payload một lần đọc UART (chia nhỏ nếu chunk > UART_NODE_RX_DATA_MAX). */
#define UART_NODE_RX_DATA_MAX 512
#define UART_NODE_RX_QUEUE_DEPTH 32

typedef struct {
    size_t len;
    uint8_t data[UART_NODE_RX_DATA_MAX];
} uart_node_rx_item_t;

// ===== UART ↔ Node (handshake wire + context task UartToNode) =====

/** Bitmask ngắt RX UART (ESP32 HAL); dùng với `uart_intr_config`. */
#ifndef UART_TO_NODE_INTR_RXFIFO_FULL
#define UART_TO_NODE_INTR_RXFIFO_FULL (1U << 0)
#define UART_TO_NODE_INTR_RXFIFO_TOUT (1U << 8)
#endif

/** Chuỗi wire handshake đầy đủ — khớp byte-for-byte với node (`Switching...` + \r\n). */
#define UART_TO_NODE_HS_WIRE       "Switching to root mode...\r\n"
#define UART_TO_NODE_HS_LEN        (sizeof(UART_TO_NODE_HS_WIRE) - 1)

/** Buffer trượt khớp handshake (phải ≥ UART_TO_NODE_HS_LEN). */
#define UART_TO_NODE_HS_ROLL_BYTES 48

/** Dòng UART / giá trị JSON `type` (“No Node”); macro có \n cho đúng wire; khi so JSON dùng phần chữ trước \n. */
#define UART_NODE_MSG_TYPE_NO_NODE "No Node\n"

typedef enum {
    UART_TO_NODE_STATE_START_ROOT = 0,
    UART_TO_NODE_STATE_CONNECTED = 1,
} uart_to_node_state_t;

typedef struct {
    uart_to_node_state_t state;
    TickType_t last_rx_tick;
    TickType_t next_tx_tick;
    uint8_t hs_roll[UART_TO_NODE_HS_ROLL_BYTES];
    size_t hs_fill;
} uart_to_node_ctx_t;

_Static_assert(UART_TO_NODE_HS_LEN <= UART_TO_NODE_HS_ROLL_BYTES,
               "UART handshake wire longer than hs_roll buffer");

#endif // UART_PROTOCOL_H
