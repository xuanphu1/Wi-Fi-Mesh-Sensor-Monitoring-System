#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PowerManager.h"
#include "SD_Card.h"
#include "SystemMonitor.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#if defined(CONFIG_SOC_TEMP_SENSOR_SUPPORTED) &&                               \
    CONFIG_SOC_TEMP_SENSOR_SUPPORTED
#include "driver/temperature_sensor.h"
#endif

typedef struct {
  dm_hw_t *hw;
  dm_cpu_t *cpu;
  dm_lvgl_t *lvgl;
  dm_metrics_t *metrics;
  dm_telemetry_t *telemetry;
} system_monitor_ctx_t;

static const char *TAG = "system_monitor";

static uint32_t cpu_idle_permille_runtime_stats(dm_cpu_t *cpu) {
  UBaseType_t task_count = uxTaskGetNumberOfTasks();
  if (task_count == 0)
    return 0;

  TaskStatus_t *task_status =
      (TaskStatus_t *)malloc(sizeof(TaskStatus_t) * (task_count + 4));
  if (task_status == NULL)
    return 0;

  uint32_t total_runtime = 0;
  UBaseType_t got =
      uxTaskGetSystemState(task_status, task_count + 4, &total_runtime);
  if (got == 0) {
    free(task_status);
    return 0;
  }

  uint64_t idle_runtime = 0;
  for (UBaseType_t i = 0; i < got; i++) {
    if (task_status[i].xHandle == cpu->idle_task_0 ||
        task_status[i].xHandle == cpu->idle_task_1) {
      idle_runtime += (uint64_t)task_status[i].ulRunTimeCounter;
    }
  }

  free(task_status);

  if (cpu->prev_total_runtime == 0) {
    cpu->prev_total_runtime = total_runtime;
    cpu->prev_idle_runtime = idle_runtime;
    return 1000;
  }

  uint64_t total_delta = (uint64_t)total_runtime - cpu->prev_total_runtime;
  uint64_t idle_delta = idle_runtime - cpu->prev_idle_runtime;
  cpu->prev_total_runtime = total_runtime;
  cpu->prev_idle_runtime = idle_runtime;

  if (total_delta == 0)
    return 0;

  uint32_t core_count = 1;
#if configNUM_CORES > 1
  core_count = 2;
#endif
  uint64_t idle_avg_delta = idle_delta / core_count;

  uint32_t idle_permille = (uint32_t)((idle_avg_delta * 1000ULL) / total_delta);
  if (idle_permille > 1000)
    idle_permille = 1000;
  return idle_permille;
}

#if defined(CONFIG_SOC_TEMP_SENSOR_SUPPORTED) &&                               \
    CONFIG_SOC_TEMP_SENSOR_SUPPORTED
static temperature_sensor_handle_t s_sys_tsens;
static bool s_sys_tsens_ready;

static void system_monitor_tsens_ensure(void) {
  if (s_sys_tsens_ready) {
    return;
  }
  temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  if (temperature_sensor_install(&cfg, &s_sys_tsens) != ESP_OK) {
    return;
  }
  if (temperature_sensor_enable(s_sys_tsens) != ESP_OK) {
    temperature_sensor_uninstall(s_sys_tsens);
    s_sys_tsens = NULL;
    return;
  }
  s_sys_tsens_ready = true;
}
#endif

static void system_monitor_task(void *arg) {
  system_monitor_ctx_t *ctx = (system_monitor_ctx_t *)arg;
  uint32_t last_log_ms = 0;
#if !defined(CONFIG_SOC_TEMP_SENSOR_SUPPORTED) ||                              \
    !CONFIG_SOC_TEMP_SENSOR_SUPPORTED
  bool logged_no_tsens = false;
#endif

  while (1) {
    ui_metrics_t m = {0};
    uint32_t bat_raw_avg = 0;
    uint32_t bat_adc_mv = 0;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

    if (ctx->hw->rtc_ready &&
        ds3231_get_time(&ctx->hw->rtc_dev, &m.rtc_time) == ESP_OK)
      m.rtc_ok = true;

    m.battery_pct =
        power_manager_battery_get_percent(ctx->hw, &bat_raw_avg, &bat_adc_mv);
    if (ctx->hw->adc_ready && bat_adc_mv > 0) {
      m.battery_pack_mv =
          (uint32_t)((uint64_t)bat_adc_mv *
                     (BAT_R_TOP_OHMS + BAT_R_BOTTOM_OHMS) / BAT_R_BOTTOM_OHMS);
    } else {
      m.battery_pack_mv = 0;
    }

    if (getSD_CardSpaceKB(&m.sd_total_kb, &m.sd_free_kb) != ESP_OK) {
      m.sd_total_kb = 0;
      m.sd_free_kb = 0;
      m.sd_used_percent = 0;
    } else if (m.sd_total_kb > 0) {
      m.sd_used_percent =
          (uint32_t)(((m.sd_total_kb - m.sd_free_kb) * 100ULL) / m.sd_total_kb);
    }

    {
      size_t total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
      size_t free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      size_t used = (total >= free) ? (total - free) : 0;

      m.ram_free_percent =
          (total > 0) ? (uint32_t)((free * 100ULL) / total) : 0;
      m.ram_free_permille =
          (total > 0) ? (uint32_t)((free * 1000ULL) / total) : 0;
      m.ram_free_kb = (uint32_t)(free / 1024ULL);
      m.ram_used_kb = (uint32_t)(used / 1024ULL);
    }

    m.cpu_idle_permille = cpu_idle_permille_runtime_stats(ctx->cpu);
    m.cpu_load_permille = 1000U - m.cpu_idle_permille;

    {
      uint32_t loop_now = ctx->lvgl->loop_counter;
      uint32_t loop_delta = loop_now - ctx->lvgl->prev_loop_counter;
      ctx->lvgl->prev_loop_counter = loop_now;
      m.fps = loop_delta;
    }

    m.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    m.chip_temp_valid = false;
#if defined(CONFIG_SOC_TEMP_SENSOR_SUPPORTED) &&                               \
    CONFIG_SOC_TEMP_SENSOR_SUPPORTED
    m.chip_temp_internal_supported = true;
    system_monitor_tsens_ensure();
    if (s_sys_tsens_ready) {
      float tc = 0.0f;
      if (temperature_sensor_get_celsius(s_sys_tsens, &tc) == ESP_OK) {
        m.chip_temp_c = tc;
        m.chip_temp_valid = true;
      }
    }
#else
    m.chip_temp_internal_supported = false;
    if (!logged_no_tsens) {
      logged_no_tsens = true;
      ESP_LOGW(TAG, "Nhiệt chip nội bộ: target không có SOC_TEMP_SENSOR trong "
                    "IDF (vd. ESP32); "
                    "gateway_status gửi chip_temp_c=null — dùng C3/S3/C6… hoặc "
                    "cảm biến ngoài.");
    }
#endif

    if (m.rtc_ok) {
      (void)snprintf(
          ctx->telemetry->timestamp, sizeof(ctx->telemetry->timestamp),
          "%04d-%02d-%02dT%02d:%02d:%02d", (int)(m.rtc_time.tm_year + 1900),
          (int)(m.rtc_time.tm_mon + 1), (int)m.rtc_time.tm_mday,
          (int)m.rtc_time.tm_hour, (int)m.rtc_time.tm_min,
          (int)m.rtc_time.tm_sec);
    } else {
      ctx->telemetry->timestamp[0] = '\0';
    }

    if (ctx->metrics->mutex &&
        xSemaphoreTake(ctx->metrics->mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      ctx->metrics->value = m;
      xSemaphoreGive(ctx->metrics->mutex);
    }

    // if ((uint32_t)(now_ms - last_log_ms) >= 10000)
    // {
    //     last_log_ms = now_ms;

    //     UBaseType_t task_count = uxTaskGetNumberOfTasks();
    //     TaskStatus_t *task_status = (TaskStatus_t
    //     *)malloc(sizeof(TaskStatus_t) * (task_count + 4)); if (task_status !=
    //     NULL) {
    //         UBaseType_t got = uxTaskGetSystemState(task_status, task_count +
    //         4, NULL); ESP_LOGI(TAG, "--- TASK STACK HIGH WATER MARK (Bytes
    //         Free) ---"); for (UBaseType_t i = 0; i < got; i++) {
    //             // Càng gần 0 thì task càng có nguy cơ tràn RAM (Stack
    //             Overflow) ESP_LOGI(TAG, "%-16s : %lu bytes free",
    //             task_status[i].pcTaskName, (unsigned
    //             long)task_status[i].usStackHighWaterMark);
    //         }
    //         free(task_status);
    //     }
    // }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void system_monitor_start(dm_hw_t *hw, dm_cpu_t *cpu, dm_lvgl_t *lvgl,
                          dm_metrics_t *metrics, dm_telemetry_t *telemetry,
                          UBaseType_t priority, BaseType_t core_id) {
  if (hw == NULL || cpu == NULL || lvgl == NULL || metrics == NULL ||
      telemetry == NULL)
    return;

  cpu->idle_task_0 = xTaskGetIdleTaskHandleForCPU(0);
#if configNUM_CORES > 1
  cpu->idle_task_1 = xTaskGetIdleTaskHandleForCPU(1);
#endif

  static system_monitor_ctx_t s_ctx;
  s_ctx.hw = hw;
  s_ctx.cpu = cpu;
  s_ctx.lvgl = lvgl;
  s_ctx.metrics = metrics;
  s_ctx.telemetry = telemetry;
  xTaskCreatePinnedToCore(system_monitor_task, "system_monitor", 3072, &s_ctx,
                          priority, NULL, core_id);
}
