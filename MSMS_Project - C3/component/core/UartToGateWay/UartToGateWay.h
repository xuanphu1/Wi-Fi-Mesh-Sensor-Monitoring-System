#ifndef UART_TO_GATEWAY_H
#define UART_TO_GATEWAY_H

#include <stddef.h>
#include "DataManager.h"
#include "ErrorCodes.h"

/**
 * @file UartToGateWay.h
 * @brief UART tới gateway: 2 task (RX lệnh / TX dữ liệu lên gateway) và gửi raw bytes.
 *
 * ---------------------------------------------------------------------------
 * Định dạng (ASCII, UTF-8, không nhị phân có cấu trúc riêng)
 * ---------------------------------------------------------------------------
 *
 * **Gateway → ESP (RX, qua dòng kết thúc \\r hoặc \\n):**
 * - Mỗi lệnh một dòng; khoảng trắng đầu/cuối bị bỏ qua; so khớp không phân biệt hoa thường.
 * - `Connected` — heartbeat khi ESP ở chế độ root + watchdog gateway.
 * - `Start Root` — yêu cầu chuyển mesh root (cần mesh đã khởi tạo xong).
 * - Dòng khác — log cảnh báo “Unknown UART cmd”.
 *
 * **ESP → Gateway (TX):**
 * - `UartToGateWay_Send()` gửi **raw bytes** tới UART (không thêm header/length); caller tự định nghĩa payload.
 * - Phản hồi lệnh: chuỗi ASCII có sẵn trong code (ví dụ `Mesh not ready`, `Switching to root mode...`).
 * - Khi ESP là **mesh root**, task TX chuyển tiếp telemetry từ `meshIo.gateway_rx_queue`: mỗi gói gửi **payload JSON** rồi thêm **một byte `\\n`** (một dòng một JSON).
 * - Heartbeat khi không có node: dòng ASCII `No node\\n` (8 byte + newline trong code).
 *
 * Không có frame kiểu [length][payload] hay CRC; nếu cần giao thức nặng hơn nên bọc thêm lớp trên `UartToGateWay_Send`.
 */

/**
 * @brief Khởi tạo UART, queue sự kiện RX, và 2 task: `uart_gateway_rx` + `uart_gateway_tx`.
 *
 * @param data Global application context.
 * @return MRS_OK on success, otherwise MRS_ERR_* .
 */
system_err_t UartToGateWay_Init(DataManager_t *data);

/**
 * @brief Gửi raw bytes tới gateway (UART TX). Có thể gọi từ bất kỳ task nào; driver UART serialize.
 *
 * @param data Pointer to bytes.
 * @param len Number of bytes.
 * @return Number of bytes queued by UART driver, negative on error.
 */
int UartToGateWay_Send(const void *data, size_t len);

#endif /* UART_TO_GATEWAY_H */
