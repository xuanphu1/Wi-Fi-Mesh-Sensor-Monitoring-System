#ifndef SYSTEM_METRICS_H
#define SYSTEM_METRICS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

typedef enum
{
    STATUS_OK = 0,
    STATUS_ERROR = 1,
    STATUS_WARNING = 2,
    STATUS_INFO = 3,
    STATUS_DEBUG = 4,
    STATUS_TRACE = 5,
    STATUS_FATAL = 6,
    STATUS_UNKNOWN = 7,
} Status_t;

typedef struct
{
    bool rtc_ok;
    struct tm rtc_time;
    int battery_pct;
    /** Ước lượng điện áp gói pin (mV) từ ADC chân chia áp — dùng cho telemetry/WS. */
    uint32_t battery_pack_mv;
    uint32_t sd_total_kb;
    uint32_t sd_free_kb;
    uint32_t sd_used_percent;
    uint32_t ram_free_percent;
    uint32_t ram_free_permille; // 0..1000
    uint32_t ram_free_kb;
    uint32_t ram_used_kb;
    uint32_t cpu_idle_permille; // 0..1000
    uint32_t cpu_load_permille; // 0..1000
    uint32_t fps;
    /** Thời gian chạy từ lúc reset/cấp nguồn (esp_timer, giây). */
    uint32_t uptime_s;
    /** Nhiệt độ chip °C — chỉ hợp lệ khi chip_temp_valid (SoC có driver cảm biến nội bộ). */
    float chip_temp_c;
    bool chip_temp_valid;
    /** True nếu IDF/SoC có driver nhiệt nội bộ (CONFIG_SOC_TEMP_SENSOR_SUPPORTED). ESP32 classic = false → chip_temp_c thường null. */
    bool chip_temp_internal_supported;
} ui_metrics_t;

typedef struct
{
    // CPU runtime stats state
    uint64_t prev_total_runtime;
    uint64_t prev_idle_runtime;
    TaskHandle_t idle_task_0;
    TaskHandle_t idle_task_1;
} dm_cpu_t;

typedef struct
{
    // FPS helper (incremented by LVGL task)
    volatile uint32_t loop_counter;
    uint32_t prev_loop_counter;
} dm_lvgl_t;

typedef struct
{
    // Shared metrics for UI
    ui_metrics_t value;
    SemaphoreHandle_t mutex;
} dm_metrics_t;

#endif // SYSTEM_METRICS_H
