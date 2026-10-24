#ifndef UART_TO_GATEWAY_H
#define UART_TO_GATEWAY_H

#include <stddef.h>
#include "DataManager.h"
#include "ErrorCodes.h"

/**
 * @file UartToGateWay.h
 * @brief UART tới gateway: 2 task (RX lệnh / TX dữ liệu lên gateway) và gửi raw bytes.
 */

typedef enum {
  UART_STATE_UNINITIALIZED = 0,
  UART_STATE_INITIALIZING,
  UART_STATE_RUNNING,
  UART_STATE_PAUSED,
  UART_STATE_STOPPING,
} uart_state_t;

uart_state_t UartToGateWay_GetState(void);

system_err_t UartToGateWay_Init(DataManager_t *data);

/** Application-level role request hook. Keeps UART command parsing independent
 * from FunctionManager while allowing both UART and UI to use the same use
 * case implementation. */
typedef system_err_t (*uart_gateway_role_request_fn)(mesh_role_t role,
                                                     void *context);
void UartToGateWay_SetRoleRequestHandler(
    uart_gateway_role_request_fn handler, void *context);

/** Application-level OTA command hook. Keeps UART driver independent from FOTAManager. */
typedef void (*uart_gateway_ota_handler_fn)(DataManager_t *data,
                                            const char *json_cmd, size_t len,
                                            void *context);
void UartToGateWay_SetOtaHandler(uart_gateway_ota_handler_fn handler,
                                void *context);

/**
 * @brief Deinitialize UART gateway subsystem and clean up resources/tasks.
 * @return MRS_OK on success.
 */
system_err_t UartToGateWay_Deinit(void);

/**
 * @brief Gửi raw bytes tới gateway (UART TX). Có thể gọi từ bất kỳ task nào; driver UART serialize.
 *
 * @param data Pointer to bytes.
 * @param len Number of bytes.
 * @return Number of bytes queued by UART driver, negative on error.
 */
int UartToGateWay_Send(const void *data, size_t len);

#if CONFIG_MSMS_ENABLE_TESTS
void UartToGateWay_TestInjectInitFault(int stage);
#endif

#endif /* UART_TO_GATEWAY_H */
