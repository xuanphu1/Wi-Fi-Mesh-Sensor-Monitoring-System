#ifndef __BATTERY_MANAGER_H__
#define __BATTERY_MANAGER_H__

#include "DataManager.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdbool.h>

#define TAG_BATTERY_MANAGER "BATTERY_MANAGER"

/**
 * @file BatteryManager.h
 * @brief Battery read API (legacy ADC stub or real driver per build).
 */

/**
 * @brief Initialize battery subsystem (ADC or stub).
 *
 * @return ESP_OK on success, or an ESP-IDF error code on failure.
 */
esp_err_t BatteryManager_Init(void);

/**
 * @brief Start a FreeRTOS task that periodically updates battery fields in DataManager.
 *
 * @param data DataManager pointer receiving batteryLevel / batteryName updates.
 */
void BatteryManager_StartTask(DataManager_t *data);

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

#endif /* __BATTERY_MANAGER_H__ */
