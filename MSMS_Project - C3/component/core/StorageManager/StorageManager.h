#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "DataManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize StorageManager.
 *        Mounts the SD Card using SPI interface.
 * @return ESP_OK on success.
 */
esp_err_t StorageManager_Init(void);

/**
 * @brief Append a string of data to a specific file on the SD card.
 * @param filepath Path to the file (e.g., "/sdcard/log.txt").
 * @param data String to append.
 * @return ESP_OK on success.
 */
esp_err_t StorageManager_AppendToFile(const char *filepath, const char *data);

/**
 * @brief Starts the background task for writing real-time data to SD card.
 *        The task will periodically check DataManager and write logs.
 * @param data Pointer to the DataManager instance.
 */
void StorageManager_StartTask(DataManager_t *data);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_MANAGER_H
