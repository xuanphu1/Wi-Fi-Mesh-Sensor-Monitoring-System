#ifndef ACTUATOR_MANAGER_H
#define ACTUATOR_MANAGER_H

#include "ErrorCodes.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize actuator manager subsystem.
 */
system_err_t ActuatorManager_Init(void);

/**
 * @brief Deinitialize actuator manager subsystem.
 */
system_err_t ActuatorManager_Deinit(void);

/**
 * @brief Set the output level of an actuator GPIO.
 * @param gpio_num GPIO pin number.
 * @param enabled true for HIGH (1), false for LOW (0).
 * @return MRS_OK on success, otherwise error.
 */
system_err_t ActuatorManager_SetLevel(int gpio_num, bool enabled);

/**
 * @brief Read the current level of an actuator GPIO.
 * @param gpio_num GPIO pin number.
 * @param out_enabled Pointer to receive status.
 * @return MRS_OK on success, otherwise error.
 */
system_err_t ActuatorManager_GetLevel(int gpio_num, bool *out_enabled);

#ifdef __cplusplus
}
#endif

#endif /* ACTUATOR_MANAGER_H */
