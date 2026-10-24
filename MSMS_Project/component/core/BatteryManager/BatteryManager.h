#ifndef __BATTERY_MANAGER_H__
#define __BATTERY_MANAGER_H__

#include "DataManager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TAG_BATTERY_MANAGER "BATTERY_MANAGER"

/**
 * @file BatteryManager.h
 * @brief Battery read API and background task lifecycle management.
 */

/**
 * @brief Initialize battery subsystem (ADC hardware and synchronization).
 *
 * @return ESP_OK on success, or an ESP-IDF error code on failure.
 */
esp_err_t BatteryManager_Init(void);

/**
 * @brief Start a FreeRTOS task that periodically updates battery fields in DataManager.
 *
 * @param data DataManager pointer receiving batteryLevel / batteryName updates.
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized, ESP_FAIL on task create error.
 */
esp_err_t BatteryManager_StartTask(DataManager_t *data);

/**
 * @brief Cooperatively request and wait for battery read task to exit.
 *
 * @param timeout_ticks Timeout waiting for task confirmation.
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on failure (task and resources preserved).
 */
esp_err_t BatteryManager_StopTask(TickType_t timeout_ticks);

/**
 * @brief Deinitialize battery manager and free all synchronization resources.
 *
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if task failed to stop.
 */
esp_err_t BatteryManager_Deinit(void);

/**
 * @brief Check whether the battery monitoring task is currently running.
 *
 * @return true if running, false otherwise.
 */
bool BatteryManager_IsRunning(void);

/**
 * @brief Last measured battery voltage from the monitoring path.
 *
 * @return Voltage in volts (stub may return 0).
 */
float BatteryManager_GetVoltage(void);

/**
 * @brief Estimated state of charge as a percentage.
 *
 * @return Value from 0 to 100 (stub may return 0).
 */
uint8_t BatteryManager_GetLevel(void);

/**
 * @brief Bucket index 0–6 for battery icon / label tables in the UI.
 *
 * @return Discrete level index for ScreenManager bitmaps.
 */
uint8_t BatteryManager_GetLevelIndex(void);

/**
 * @brief Copy the latest battery snapshot into a BatteryInfo_t for display.
 *
 * @param batteryInfo Destination; must not be NULL.
 */
void BatteryManager_UpdateInfo(BatteryInfo_t *batteryInfo);

#ifdef __cplusplus
}
#endif

#endif /* __BATTERY_MANAGER_H__ */
