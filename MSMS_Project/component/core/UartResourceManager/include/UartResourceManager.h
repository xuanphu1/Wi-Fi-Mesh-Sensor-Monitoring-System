#ifndef UART_RESOURCE_MANAGER_H
#define UART_RESOURCE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ErrorCodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file UartResourceManager.h
 * @brief Transactional UART bus ownership and resource manager.
 *
 * Provides serialized, leased access to the shared UART bus between
 * the Gateway subsystem and UART-based environmental sensors (e.g. MH-Z14A, PMS7003).
 */

/* ============================================================================
 * STATE MACHINE & OWNERSHIP ENUMS
 * ============================================================================ */

typedef enum {
  UART_RES_STATE_UNINITIALIZED = 0,
  UART_RES_STATE_GATEWAY_ACTIVE,
  UART_RES_STATE_QUIESCE_REQUESTED,
  UART_RES_STATE_SENSOR_PROBE_ACTIVE,
  UART_RES_STATE_SENSOR_ASSIGNED,
  UART_RES_STATE_GATEWAY_RESUMING,
  UART_RES_STATE_STOPPING,
  UART_RES_STATE_CLEANUP_REQUIRED,
  UART_RES_STATE_FAILED,
} uart_resource_state_t;

typedef enum {
  UART_OWNER_NONE = 0,
  UART_OWNER_GATEWAY,
  UART_OWNER_SENSOR_SCANNER,
  UART_OWNER_SENSOR_ACTIVE,
} uart_owner_t;

/* ============================================================================
 * LEASE & CONFIGURATION STRUCTURES
 * ============================================================================ */

typedef struct {
  uart_owner_t owner;
  uint32_t token;
  uint32_t generation;
  uart_port_t port;
} uart_lease_t;

typedef struct {
  uart_config_t uart_config;
  int rx_buffer_size;
  int tx_buffer_size;
  int queue_size;
  QueueHandle_t *out_queue;
  int intr_alloc_flags;
  bool enable_intr;
  uart_intr_config_t intr_config;
} uart_resource_config_t;

typedef struct {
  uart_resource_state_t state;
  uart_owner_t current_owner;
  uint32_t current_generation;
  uint32_t active_token;
  uart_port_t port;
} uart_resource_snapshot_t;

/* ============================================================================
 * GATEWAY QUIESCE / RESUME CALLBACKS
 * ============================================================================ */

typedef system_err_t (*uart_gateway_quiesce_fn)(void *context, TickType_t timeout_ticks);
typedef system_err_t (*uart_gateway_resume_fn)(void *context, uart_port_t port, QueueHandle_t queue);

/* ============================================================================
 * PUBLIC APIS
 * ============================================================================ */

/**
 * @brief Initialize the UART resource manager (idempotent for same port/pins).
 *
 * @param port Hardware UART port to manage (e.g. UART_NUM_1 / UART_NUM_2).
 * @param tx_pin TX GPIO pin.
 * @param rx_pin RX GPIO pin.
 * @return MRS_OK on success, or error code.
 */
system_err_t UartResource_Init(uart_port_t port, int tx_pin, int rx_pin);

/**
 * @brief Deinitialize the UART resource manager and release any held driver.
 * @return MRS_OK on success.
 */
system_err_t UartResource_Deinit(void);

/**
 * @brief Register Gateway quiesce and resume callbacks.
 *
 * Only registers hooks and context; does NOT alter ownership or state.
 * @return MRS_OK on success, MRS_ERR_CORE_NOT_INITIALIZED or invalid params.
 */
system_err_t UartResource_RegisterGatewayCallbacks(uart_gateway_quiesce_fn quiesce_cb,
                                                   uart_gateway_resume_fn resume_cb,
                                                   void *context);

/**
 * @brief Acquire a transactional lease for the UART bus.
 *
 * If the bus is currently owned by Gateway, this initiates quiesce on Gateway,
 * waits for ACK from both RX and TX tasks, uninstalls Gateway driver, and installs
 * the requested configuration.
 *
 * @param owner The requesting owner.
 * @param config UART configuration and buffer sizes.
 * @param timeout_ticks Timeout waiting for quiesce and lock.
 * @param out_lease Pointer to store the issued lease.
 * @return MRS_OK on success, MRS_ERR_CORE_TIMEOUT on quiesce timeout,
 *         or MRS_ERR_CORE_INVALID_STATE if another sensor already owns the bus.
 */
system_err_t UartResource_Acquire(uart_owner_t owner,
                                  const uart_resource_config_t *config,
                                  TickType_t timeout_ticks,
                                  uart_lease_t *out_lease);

/**
 * @brief Release a transactional lease for the UART bus.
 *
 * Validates the lease generation and token to prevent stale or double releases.
 * If released by the active sensor or scanner, restores Gateway if no active
 * UART sensor remains assigned.
 *
 * @param lease Pointer to the lease to release. Invalidated upon return.
 * @return MRS_OK on success.
 */
system_err_t UartResource_Release(uart_lease_t *lease);

/**
 * @brief Check if a lease is currently valid.
 */
bool UartResource_IsLeaseValid(const uart_lease_t *lease);

/**
 * @brief Retrieve a snapshot of the current UART resource state.
 */
system_err_t UartResource_GetSnapshot(uart_resource_snapshot_t *out_snapshot);

/**
 * @brief Return string representation of UART resource state.
 */
const char *UartResource_StateToString(uart_resource_state_t state);

/**
 * @brief Return string representation of UART owner.
 */
const char *UartResource_OwnerToString(uart_owner_t owner);

/* ============================================================================
 * TEST HOOKS
 * ============================================================================ */
#if CONFIG_MSMS_ENABLE_TESTS
void UartResource_TestInjectQuiesceTimeout(bool inject);
void UartResource_TestInjectResumeFault(bool inject);
void UartResource_TestInjectDriverInstallFault(bool inject);
void UartResource_TestReset(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* UART_RESOURCE_MANAGER_H */
