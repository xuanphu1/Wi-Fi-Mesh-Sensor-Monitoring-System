/**
 * @file PowerManager.h
 * @brief Power control module for peripheral 3V3 (GPIO 12) and 5V (GPIO 14) rails.
 */
#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Power rail channels bitmask.
 */
typedef enum {
    POWER_CHANNEL_NONE = 0,
    POWER_CHANNEL_3V3  = (1 << 0), ///< 3.3V Peripheral Rail (GPIO 12, High Active)
    POWER_CHANNEL_5V   = (1 << 1), ///< 5.0V Peripheral Rail (GPIO 14, High Active)
    POWER_CHANNEL_ALL  = (POWER_CHANNEL_3V3 | POWER_CHANNEL_5V) ///< All power channels
} power_channel_t;

/**
 * @brief Initialize PowerManager GPIOs and timer resources.
 *
 * Configures power control pins as outputs (default LOW/OFF with internal pull-down).
 *
 * @return ESP_OK on success, or error code on failure.
 */
esp_err_t PowerManager_Init(void);

/**
 * @brief Turn ON or OFF a specific power rail.
 *
 * @param channel Target channel (POWER_CHANNEL_3V3, POWER_CHANNEL_5V, or POWER_CHANNEL_ALL).
 * @param enable true to turn ON (HIGH), false to turn OFF (LOW).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if invalid channel.
 */
esp_err_t PowerManager_SetState(power_channel_t channel, bool enable);

/**
 * @brief Get the current ON/OFF state of a power channel.
 *
 * @param channel Single channel (POWER_CHANNEL_3V3 or POWER_CHANNEL_5V).
 * @return true if channel is ON, false if OFF or invalid channel.
 */
bool PowerManager_GetState(power_channel_t channel);

/**
 * @brief Toggle the state of a power channel.
 *
 * @param channel Target channel.
 * @return ESP_OK on success, or error code.
 */
esp_err_t PowerManager_Toggle(power_channel_t channel);

/**
 * @brief Turn ON a power rail and automatically turn it OFF after a specified duration.
 *
 * Useful for powering sensors for a brief measurement window and then turning them off.
 *
 * @param channel Target channel (POWER_CHANNEL_3V3, POWER_CHANNEL_5V, or POWER_CHANNEL_ALL).
 * @param duration_ms Time in milliseconds to keep the power rail ON before automatically turning OFF.
 * @return ESP_OK on success, or error code.
 */
esp_err_t PowerManager_EnableForDuration(power_channel_t channel, uint32_t duration_ms);

/**
 * @brief Schedule a power state change after a specified delay.
 *
 * @param channel Target channel.
 * @param target_state Target state to set when the delay expires (true for ON, false for OFF).
 * @param delay_ms Delay in milliseconds before setting the state.
 * @return ESP_OK on success, or error code.
 */
esp_err_t PowerManager_ScheduleAction(power_channel_t channel, bool target_state, uint32_t delay_ms);

/**
 * @brief Cancel any pending timer / scheduled action on a channel.
 *
 * @param channel Target channel.
 * @return ESP_OK on success, or error code.
 */
esp_err_t PowerManager_CancelTimer(power_channel_t channel);

/**
 * @brief Get remaining time in milliseconds before a pending timer fires.
 *
 * @param channel Target channel (POWER_CHANNEL_3V3 or POWER_CHANNEL_5V).
 * @return Remaining milliseconds, or 0 if no timer is active.
 */
uint32_t PowerManager_GetRemainingTimeMs(power_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MANAGER_H */
