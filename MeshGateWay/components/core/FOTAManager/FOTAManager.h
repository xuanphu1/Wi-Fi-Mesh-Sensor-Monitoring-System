#ifndef FOTA_MANAGER_H
#define FOTA_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

/**
 * OTA task (placeholder).
 * Hiện tại project chưa có luồng gọi OTA, nhưng component cần build/compile được.
 */
void FOTA_task(void *pvParameters);

/**
 * OTA bằng HTTPS theo URL.
 */
esp_err_t do_manual_http_ota(const char *url);

#ifdef __cplusplus
}
#endif

#endif // FOTA_MANAGER_H