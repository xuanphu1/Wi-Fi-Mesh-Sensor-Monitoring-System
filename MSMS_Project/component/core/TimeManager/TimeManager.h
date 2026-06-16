#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize TimeManager (SNTP).
 *        Sets up the SNTP client to sync time via Wi-Fi.
 *        Requires Wi-Fi to be connected for actual sync.
 */
esp_err_t TimeManager_Init(void);

/**
 * @brief Wait for time to be synchronized from NTP server.
 * @param timeout_ms Maximum time to wait in milliseconds.
 * @return ESP_OK if synced, ESP_ERR_TIMEOUT if timed out.
 */
esp_err_t TimeManager_WaitForSync(uint32_t timeout_ms);

/**
 * @brief Get the current time as a formatted string (e.g., "YYYY-MM-DD HH:MM:SS").
 * @param buf Buffer to store the string.
 * @param max_len Size of the buffer.
 * @return ESP_OK on success.
 */
esp_err_t TimeManager_GetTimestampStr(char *buf, size_t max_len);

/**
 * @brief Get current Unix epoch time in seconds.
 * @return Epoch time in seconds, or 0 if time is not set.
 */
uint32_t TimeManager_GetEpochTime(void);

/**
 * @brief Get current time as struct tm.
 * @param timeinfo Pointer to struct tm to fill.
 * @return ESP_OK on success.
 */
esp_err_t TimeManager_GetTime(struct tm *timeinfo);

#ifdef __cplusplus
}
#endif

#endif // TIME_MANAGER_H
