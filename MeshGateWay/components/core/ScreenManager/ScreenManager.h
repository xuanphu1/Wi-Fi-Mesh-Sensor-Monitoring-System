#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "Datamanager.h"
#include <stdbool.h>
#include <stdint.h>

void screen_manager_start(dm_metrics_t *metrics, dm_lvgl_t *lvgl, dm_telemetry_t *telemetry, dm_hw_t *hw, UBaseType_t priority, BaseType_t core_id);

/**
 * Cập nhật tiến độ và hiển thị PanelOTA trên màn hình LVGL.
 * @param active: true để mở PanelOTA, false để ẩn
 * @param percent: Phần trăm tiến độ 0 - 100% (cập nhật Arc1 và LabelPercentOTA)
 * @param target: Tên đối tượng (ValueTarget: "Gateway", "Root", "Node", ...)
 * @param version: Phiên bản firmware (ValueVersion: "0.0.2", ...)
 * @param detail: Chi tiết phạm vi hoặc trạng thái (ValueVersion1: "root", "leaf", "Downloading", "Flashing", MAC...)
 */
void screen_manager_set_ota_progress(bool active, uint8_t percent, const char *target, const char *version, const char *detail);

#endif // SCREEN_MANAGER_H
