#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "Datamanager.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void screen_manager_start(dm_metrics_t *metrics, dm_lvgl_t *lvgl, dm_telemetry_t *telemetry, dm_hw_t *hw, UBaseType_t priority, BaseType_t core_id);

/**
 * Cập nhật tiến độ và kích hoạt màn hình OTA Popup trên TFT thuần.
 * @param active: true để hiển thị popup OTA, false để ẩn
 * @param percent: Phần trăm tiến độ 0 - 100%
 * @param target: Tên đối tượng ("Gateway", "Root", "Node", ...)
 * @param version: Phiên bản firmware ("0.0.2", ...)
 * @param detail: Chi tiết ("Downloading 45%", "Flashing...", "Success! Rebooting...", ...)
 */
void screen_manager_set_ota_progress(bool active, uint8_t percent, const char *target, const char *version, const char *detail);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_MANAGER_H
