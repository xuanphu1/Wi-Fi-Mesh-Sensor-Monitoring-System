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

typedef struct {
  char url[256];
  char job_id[64];
  char version[32];
  char target[32];
  char target_detail[64];
  uint32_t size;
  char md5[64];
} fota_job_info_t;

typedef void (*fota_progress_cb_t)(const char *job_id, const char *status,
                                   uint8_t percent, uint32_t bytes_read,
                                   uint32_t total_bytes,
                                   const char *running_part,
                                   const char *target_part,
                                   const char *msg);

/** Đăng ký callback để FOTAManager thông báo tiến trình OTA lên WS */
void fota_register_progress_callback(fota_progress_cb_t cb);

/**
 * Khởi tạo FOTAManager và tự động xác nhận phân vùng hợp lệ (huỷ rollback)
 * khi boot từ firmware mới để ESP-IDF luân chuyển phân vùng ota_0 <-> ota_1.
 */
esp_err_t fota_manager_init(void);

/**
 * OTA task (placeholder).
 * Hiện tại project chưa có luồng gọi OTA, nhưng component cần build/compile được.
 */
void FOTA_task(void *pvParameters);

/**
 * OTA bằng HTTP / HTTPS theo URL hoặc job_info đầy đủ.
 */
esp_err_t do_manual_http_ota(const char *url);
esp_err_t fota_start_gateway_ota(const char *url);
esp_err_t fota_start_gateway_ota_with_info(const fota_job_info_t *info);
uint8_t fota_get_progress_percent(void);
bool fota_is_running(void);
esp_err_t fota_get_last_result(void);

#ifdef __cplusplus
}
#endif

#endif // FOTA_MANAGER_H
