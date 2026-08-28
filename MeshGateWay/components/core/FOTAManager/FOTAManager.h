#ifndef FOTA_MANAGER_H
#define FOTA_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define KEY_OTA 0xAB

enum
{
  OTA_MODE_COMMAND = 0xFE,
  NORMAL_MODE_COMMAND = 0xFF,
};

#define END_LINE_DATA 0xEE

#ifndef OTA_DATA_TYPE_DEFINED
#define OTA_DATA_TYPE_DEFINED
typedef struct
{
  char *type;
  uint16_t index;
  uint8_t percent;
  uint16_t length;
  char *line;
} OTAData;
#endif

typedef struct
{
  uint8_t Flag_OTA;

  uint8_t Flag_Request_OTA;

} Flag_Signal_Control_t;

/**
 * OTA task (placeholder).
 * Hiện tại project chưa có luồng gọi OTA, nhưng component cần build/compile được.
 */
void FOTA_task(void *pvParameters);

/**
 * OTA bằng HTTPS theo URL.
 */
esp_err_t do_manual_http_ota(const char *url);
esp_err_t fota_start_gateway_ota(const char *url);
uint8_t fota_get_progress_percent(void);
bool fota_is_running(void);
esp_err_t fota_get_last_result(void);

#ifdef __cplusplus
}
#endif

#endif // FOTA_MANAGER_H
