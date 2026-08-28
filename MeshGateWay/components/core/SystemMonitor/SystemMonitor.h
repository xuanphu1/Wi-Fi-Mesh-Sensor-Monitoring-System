#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include "Datamanager.h"

void system_monitor_start(dm_hw_t *hw,
                          dm_cpu_t *cpu,
                          dm_lvgl_t *lvgl,
                          dm_metrics_t *metrics,
                          dm_telemetry_t *telemetry,
                          UBaseType_t priority,
                          BaseType_t core_id);

#endif // SYSTEM_MONITOR_H
