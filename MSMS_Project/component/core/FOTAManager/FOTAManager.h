#ifndef FOTA_MANAGER_H
#define FOTA_MANAGER_H

#include "DataManager.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char url[256];
  char job_id[64];
  char version[32];
  char target[32];
  char target_detail[64];
  char ssid[33];
  char password[65];
  uint32_t size;
  char md5[64];
} fota_job_info_t;

/** Callback to receive formatted JSON ota_progress line for sending upstream (UART / WS) */
typedef void (*fota_raw_send_fn)(const char *data, size_t len);

/** Detailed progress callback */
typedef void (*fota_progress_cb_t)(const char *job_id, const char *status,
                                   uint8_t percent, uint32_t bytes_read,
                                   uint32_t total_bytes,
                                   const char *running_part,
                                   const char *target_part,
                                   const char *msg);

/**
 * @brief Initialize FOTAManager and register raw send callback.
 */
void FOTAManager_Init(fota_raw_send_fn send_fn);

/**
 * @brief Register callback for progress events.
 */
void FOTAManager_RegisterProgressCallback(fota_progress_cb_t cb);

/**
 * @brief Handle incoming JSON OTA command from Gateway (or Mesh TCP).
 *
 * Checks if target matches this device. If matched, switches mode to Wi-Fi,
 * connects to Wi-Fi, and executes HTTPS OTA download and flash.
 *
 * @param data DataManager pointer
 * @param json_str JSON string containing "ota_start"
 * @param len Length of JSON string
 * @return ESP_OK if handled, ESP_ERR_INVALID_ARG if invalid, or error code.
 */
esp_err_t FOTAManager_HandleOtaCommand(DataManager_t *data, const char *json_str, size_t len);

/**
 * @brief Start OTA task directly with fota_job_info_t.
 */
esp_err_t FOTAManager_StartJob(DataManager_t *data, const fota_job_info_t *job);

/**
 * @brief Check if OTA task is currently in progress.
 */
bool FOTAManager_IsRunning(void);

/**
 * @brief Get current OTA download/flash progress percentage (0-100).
 */
uint8_t FOTAManager_GetProgressPercent(void);

/**
 * @brief Downstream callback invoked on child nodes when Root broadcasts a message.
 */
void FOTAManager_MeshDownstreamHandler(DataManager_t *data, const char *json_str, size_t len);

#ifdef __cplusplus
}
#endif

#endif // FOTA_MANAGER_H
