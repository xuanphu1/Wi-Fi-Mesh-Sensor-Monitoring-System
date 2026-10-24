#include "BatteryManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/adc.h"
#include <string.h>

#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_0 // GPIO 36
#define BATTERY_ADC_WIDTH ADC_WIDTH_BIT_12
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_12
#define ADC_MAX 4095.0f
#define VREF 3.3f
#define R1 100000.0f
#define R2 100000.0f
#define BATTERY_FULL_VOLTAGE 4.2f
#define BATTERY_EMPTY_VOLTAGE 3.0f
#define BATTERY_READ_INTERVAL_MS 5000

static bool adc_initialized = false;
static bool s_battery_initialized = false;
static volatile bool s_battery_stop_requested = false;
static TaskHandle_t s_battery_task_handle = NULL;
static SemaphoreHandle_t s_battery_stopped_sem = NULL;

typedef enum {
  BATTERY_TASK_STATE_STOPPED = 0,
  BATTERY_TASK_STATE_STARTING,
  BATTERY_TASK_STATE_RUNNING,
  BATTERY_TASK_STATE_STOPPING,
} battery_task_state_t;

static volatile battery_task_state_t s_battery_task_state = BATTERY_TASK_STATE_STOPPED;

static float current_voltage_legacy = 0.0f;
static uint8_t current_level_legacy = 0;
static uint8_t current_level_index_legacy = 0;
static char battery_text_legacy[32] = "0% (0.00V)";

static esp_err_t battery_adc_init(void) {
  if (adc_initialized) {
    return ESP_OK;
  }
  esp_err_t ret = adc1_config_width(BATTERY_ADC_WIDTH);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG_BATTERY_MANAGER, "Failed to config ADC width: %s",
             esp_err_to_name(ret));
    return ret;
  }
  ret = adc1_config_channel_atten(BATTERY_ADC_CHANNEL, BATTERY_ADC_ATTEN);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG_BATTERY_MANAGER,
             "Failed to config ADC channel attenuation: %s",
             esp_err_to_name(ret));
    return ret;
  }
  vTaskDelay(pdMS_TO_TICKS(100));
  int dummy_sum = 0;
  for (int i = 0; i < 10; i++) {
    dummy_sum += adc1_get_raw(BATTERY_ADC_CHANNEL);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  ESP_LOGI(TAG_BATTERY_MANAGER, "ADC warm-up readings: avg = %.0f",
           dummy_sum / 10.0f);
  ESP_LOGI(TAG_BATTERY_MANAGER,
           "ADC initialized for battery monitoring on GPIO 36 (DB_12, 12-bit)");
  adc_initialized = true;
  return ESP_OK;
}

static float battery_read_voltage(void) {
  if (!adc_initialized) {
    ESP_LOGW(TAG_BATTERY_MANAGER, "ADC not initialized, cannot read voltage");
    return 0.0f;
  }
  int adc_sum = 0;
  int valid_samples = 0;
  const int samples = 20;
  for (int i = 0; i < samples; i++) {
    int adc_reading = adc1_get_raw(BATTERY_ADC_CHANNEL);
    if (adc_reading >= 0) {
      adc_sum += adc_reading;
      valid_samples++;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  if (valid_samples == 0) {
    return 0.0f;
  }
  float adc_avg = (float)adc_sum / (float)valid_samples;
  float vAdc = (adc_avg / ADC_MAX) * VREF;
  float vBat = vAdc * ((R1 + R2) / R2);
  return vBat + 0.37f;
}

static uint8_t battery_calculate_level(float voltage) {
  if (voltage >= BATTERY_FULL_VOLTAGE) {
    return 100;
  }
  if (voltage <= BATTERY_EMPTY_VOLTAGE) {
    return 0;
  }
  float percentage = ((voltage - BATTERY_EMPTY_VOLTAGE) /
                      (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE)) *
                     100.0f;
  if (percentage > 100.0f) {
    percentage = 100.0f;
  }
  if (percentage < 0.0f) {
    percentage = 0.0f;
  }
  return (uint8_t)percentage;
}

static uint8_t battery_map_level_to_index(uint8_t level) {
  if (level == 0) {
    return 0;
  }
  if (level <= 17) {
    return 1;
  }
  if (level <= 33) {
    return 2;
  }
  if (level <= 50) {
    return 3;
  }
  if (level <= 67) {
    return 4;
  }
  if (level <= 83) {
    return 5;
  }
  return 6;
}

static void battery_read_task(void *pvParameters) {
  DataManager_t *data = (DataManager_t *)pvParameters;
  ESP_LOGI(TAG_BATTERY_MANAGER, "Battery read task started");
  while (!s_battery_stop_requested) {
    float voltage = battery_read_voltage();
    uint8_t level = battery_calculate_level(voltage);
    uint8_t level_index = battery_map_level_to_index(level);
    bool is_charging = (voltage > 4.15f);

    if (is_charging) {
      level_index = 7; // Index for charging icon
    }

    current_voltage_legacy = voltage;
    current_level_legacy = level;
    current_level_index_legacy = level_index;
    snprintf(battery_text_legacy, sizeof(battery_text_legacy), "%d%% (%.2fV)%s",
             level, voltage, is_charging ? " CHRG" : "");
    ESP_LOGI(TAG_BATTERY_MANAGER, "Battery: %d%% (index: %d), Voltage: %.2fV%s",
             level, level_index, voltage, is_charging ? " [CHARGING]" : "");
    if (data != NULL) {
      data->objectInfo.batteryInfo.batteryLevel = level_index;
      data->objectInfo.batteryInfo.levelPercent = level;
      data->objectInfo.batteryInfo.batteryName = battery_text_legacy;
      data->objectInfo.batteryInfo.is_charging = is_charging;
    }

    for (int elapsed = 0; elapsed < BATTERY_READ_INTERVAL_MS && !s_battery_stop_requested; elapsed += 100) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

  s_battery_task_state = BATTERY_TASK_STATE_STOPPING;
  SemaphoreHandle_t sem = s_battery_stopped_sem;
  if (sem != NULL) {
    xSemaphoreGive(sem);
  }
  ESP_LOGI(TAG_BATTERY_MANAGER, "Battery read task exited cleanly");
  vTaskDelete(NULL);
}

esp_err_t BatteryManager_Init(void) {
  if (s_battery_initialized) {
    return ESP_OK;
  }
  esp_err_t ret = battery_adc_init();
  if (ret != ESP_OK) {
    return ret;
  }
  if (s_battery_stopped_sem == NULL) {
    s_battery_stopped_sem = xSemaphoreCreateBinary();
    if (s_battery_stopped_sem == NULL) {
      adc_initialized = false;
      return ESP_ERR_NO_MEM;
    }
  }
  s_battery_initialized = true;
  s_battery_stop_requested = false;
  ESP_LOGI(TAG_BATTERY_MANAGER, "BatteryManager initialized");
  return ESP_OK;
}

esp_err_t BatteryManager_StartTask(DataManager_t *data) {
  if (!s_battery_initialized) {
    return ESP_ERR_INVALID_STATE;
  }
  if (s_battery_task_handle != NULL) {
    return ESP_OK; // already running
  }
  s_battery_stop_requested = false;
  if (s_battery_stopped_sem != NULL) {
    (void)xSemaphoreTake(s_battery_stopped_sem, 0);
  }
  s_battery_task_state = BATTERY_TASK_STATE_STARTING;
  BaseType_t ret = xTaskCreate(battery_read_task, "battery_read_task", 4096, data, 5, &s_battery_task_handle);
  if (ret != pdPASS) {
    s_battery_task_handle = NULL;
    s_battery_task_state = BATTERY_TASK_STATE_STOPPED;
    ESP_LOGE(TAG_BATTERY_MANAGER, "Failed to create battery read task");
    return ESP_FAIL;
  }
  s_battery_task_state = BATTERY_TASK_STATE_RUNNING;
  ESP_LOGI(TAG_BATTERY_MANAGER, "Battery read task created");
  return ESP_OK;
}

esp_err_t BatteryManager_StopTask(TickType_t timeout_ticks) {
  if (s_battery_task_state == BATTERY_TASK_STATE_STOPPED && s_battery_task_handle == NULL) {
    return ESP_OK;
  }
  s_battery_stop_requested = true;
  if (s_battery_stopped_sem == NULL ||
      xSemaphoreTake(s_battery_stopped_sem, timeout_ticks) != pdTRUE) {
    ESP_LOGE(TAG_BATTERY_MANAGER, "Timeout waiting for battery read task to exit");
    return ESP_ERR_TIMEOUT;
  }
  s_battery_stop_requested = false;
  s_battery_task_handle = NULL;
  s_battery_task_state = BATTERY_TASK_STATE_STOPPED;
  return ESP_OK;
}

esp_err_t BatteryManager_Deinit(void) {
  if (!s_battery_initialized && s_battery_task_handle == NULL) {
    return ESP_OK;
  }
  esp_err_t err = BatteryManager_StopTask(pdMS_TO_TICKS(2000));
  if (err != ESP_OK) {
    ESP_LOGE(TAG_BATTERY_MANAGER, "BatteryManager_Deinit: StopTask failed: %s; preserving resources", esp_err_to_name(err));
    return err;
  }
  if (s_battery_stopped_sem != NULL) {
    SemaphoreHandle_t sem = s_battery_stopped_sem;
    s_battery_stopped_sem = NULL;
    vSemaphoreDelete(sem);
  }
  adc_initialized = false;
  s_battery_initialized = false;
  s_battery_stop_requested = false;
  return ESP_OK;
}

bool BatteryManager_IsRunning(void) {
  return s_battery_task_handle != NULL;
}

float BatteryManager_GetVoltage(void) { return current_voltage_legacy; }

uint8_t BatteryManager_GetLevel(void) { return current_level_legacy; }

uint8_t BatteryManager_GetLevelIndex(void) {
  return current_level_index_legacy;
}

void BatteryManager_UpdateInfo(BatteryInfo_t *batteryInfo) {
  if (batteryInfo == NULL) {
    return;
  }
  batteryInfo->batteryLevel = current_level_index_legacy;
  batteryInfo->levelPercent = current_level_legacy;
  batteryInfo->batteryName = battery_text_legacy;
  batteryInfo->is_charging = (current_voltage_legacy > 4.15f);
}
