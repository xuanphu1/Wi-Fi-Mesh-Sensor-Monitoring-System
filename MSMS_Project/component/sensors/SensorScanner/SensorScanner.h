#ifndef __SENSOR_SCANNER_H__
#define __SENSOR_SCANNER_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "DataManager.h"
#include "SensorTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*sensor_scanner_permission_fn)(void);
typedef void (*sensor_scanner_assignment_cb_t)(void *context, PortId_t port,
                                               SensorType_t type);

/**
 * @brief Initialize sensor scanner subsystem.
 * @param data Pointer to global DataManager.
 * @return ESP_OK on success.
 */
esp_err_t SensorScanner_Init(DataManager_t *data);

/**
 * @brief Deinitialize sensor scanner subsystem.
 * @return ESP_OK on success.
 */
esp_err_t SensorScanner_Deinit(void);

/**
 * @brief Start background auto-scan task (runs only when permitted by policy).
 * @param data Pointer to global DataManager.
 * @return ESP_OK on success.
 */
esp_err_t SensorScanner_StartTask(DataManager_t *data);

/**
 * @brief Stop background auto-scan task.
 * @return ESP_OK on success.
 */
esp_err_t SensorScanner_StopTask(void);

/**
 * @brief Inject policy callback to determine if local scanning is permitted.
 */
void SensorScanner_SetPermissionPolicy(sensor_scanner_permission_fn policy_fn);

/**
 * @brief Inject assignment callback triggered when a sensor is automatically assigned.
 */
void SensorScanner_SetAssignmentCallback(sensor_scanner_assignment_cb_t callback,
                                         void *context);

/**
 * @brief Run a one-time probe across all supported sensors.
 *        Does NOT run if device is ROOT node or policy disallows.
 * @param data Pointer to global DataManager.
 * @param found_sensors Output array to store detected sensor types.
 * @param max_sensors Maximum capacity of found_sensors.
 * @return Number of sensors detected.
 */
int SensorScanner_Scan(DataManager_t *data, SensorType_t *found_sensors, size_t max_sensors);

/**
 * @brief Automatically assign detected sensors to the remaining empty ports.
 *        Ports that already have a sensor are left untouched.
 * @param data Pointer to global DataManager.
 * @param found_sensors Array of detected sensor types.
 * @param count Number of detected sensors.
 * @return Number of ports newly assigned.
 */
int SensorScanner_AutoAssign(DataManager_t *data, const SensorType_t *found_sensors, size_t count);

/** Return whether current policy permits local sensor scanning. */
bool SensorScanner_IsAllowed(void);

/**
 * @brief Enable or disable background continuous auto-scan.
 * @param enable true to enable, false to disable.
 */
void SensorScanner_SetAutoScanEnabled(bool enable);

/**
 * @brief Check if background auto-scan is currently enabled.
 * @return true if enabled.
 */
bool SensorScanner_IsAutoScanEnabled(void);

/**
 * @brief Immediately trigger an auto-scan cycle if background auto-scan task is running.
 */
void SensorScanner_TriggerScan(void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_SCANNER_H__ */
