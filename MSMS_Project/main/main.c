#include "main.h"
#include "app_test.h"
#include "BatteryManager.h"
#include "DataManager.h"
#include "FOTAManager.h"
#include "UartToGateWay.h"
#include "FunctionManager.h"
#include "InternetManager.h"
#include "MeshManager.h"
#include "PinManager.h"
#include "PowerManager.h"
#include "SD_Card.h"
#include "StorageManager.h"
#include "SystemPerfomance.h"
#include "TimeManager.h"
#include "esp_app_desc.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "pcf8574.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include <time.h>


/*
static i2c_dev_t s_pcf8574;

#define PCF8574_ADDR 0x20
#define PCF8574_P4_MASK (1U << 4)
#define PCF8574_P6_MASK (1U << 6)
*/

static esp_err_t init_spiffs(void) {
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false,
  };

  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG_MAIN, "Failed to mount SPIFFS: %s", esp_err_to_name(ret));
    return ret;
  }
  return ESP_OK;
}

#if !CONFIG_MSMS_ENABLE_TESTS
static bool app_sensor_bus_lock(uint32_t timeout_ms, void *context) {
  (void)context;
  if (!ScreenManager_IsReady()) {
    return true;
  }
  return ScreenManager_Lock(pdMS_TO_TICKS(timeout_ms));
}

static void app_sensor_bus_unlock(void *context) {
  (void)context;
  if (ScreenManager_IsReady()) {
    ScreenManager_Unlock();
  }
}

static void app_fota_uart_sender(const char *data, size_t len) {
  if (data != NULL && len > 0) {
    UartToGateWay_Send(data, len);
  }
}

static void app_uart_ota_handler(DataManager_t *data, const char *json_cmd,
                                 size_t len, void *context) {
  (void)context;
  FOTAManager_HandleOtaCommand(data, json_cmd, len);
}
#endif

void app_main(void) {
  // Sync firmware version from PROJECT_VER in CMakeLists.txt
  const esp_app_desc_t *app_desc = esp_app_get_description();
  if (app_desc != NULL) {
    int maj = 0, min = 0, pat = 0;
    if (sscanf(app_desc->version, "%d.%d.%d", &maj, &min, &pat) >= 1) {
      DataManager.version[0] = (uint8_t)maj;
      DataManager.version[1] = (uint8_t)min;
      DataManager.version[2] = (uint8_t)pat;
    }
    ESP_LOGI(TAG_MAIN, "Firmware version: %s (Parsed: %u.%u.%u)",
             app_desc->version, DataManager.version[0],
             DataManager.version[1], DataManager.version[2]);
  }

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Initialize and mount SPIFFS globally
  ESP_ERROR_CHECK(init_spiffs());

  ButtonManagerInit();
  ESP_ERROR_CHECK(PowerManager_Init());
  ESP_ERROR_CHECK(i2cInitDevCommon());
  // ESP_ERROR_CHECK(init_pcf8574_outputs());
  PowerManager_SetState(POWER_CHANNEL_3V3, true);
  PowerManager_SetState(POWER_CHANNEL_5V, true);

  system_err_t screen_ret = ScreenManager_Init();
  if (screen_ret != MRS_OK) {
    ESP_LOGE(TAG_MAIN, "ScreenManager_Init failed: %s", system_err_to_name(screen_ret));
    ErrorCodes_PushError(DataManager.error_code, DATA_MANAGER_ERROR_CAPACITY,
                         screen_ret);
  }

#if CONFIG_MSMS_ENABLE_TESTS
  ESP_LOGI(TAG_MAIN, "=== CONFIG_MSMS_ENABLE_TESTS: Running Architectural Test Suite ===");
  int failures = app_run_all_tests();
  ESP_LOGI(TAG_MAIN, "=== Test suite complete. Result: %d failure(s). Halting production boot. ===", failures);
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
#else
  ESP_ERROR_CHECK(FunctionManager_Init(&DataManager));
  FunctionManager_SetSensorBusGuard(app_sensor_bus_lock,
                                    app_sensor_bus_unlock, NULL);
  FOTAManager_Init(app_fota_uart_sender);
  UartToGateWay_SetOtaHandler(app_uart_ota_handler, NULL);
  if (ScreenManager_IsReady()) {
    MenuSystemInit(&DataManager);
  }
  MeshManager_RegisterDownstreamCallback(FOTAManager_MeshDownstreamHandler);

  SystemPerformance_Init();

  // Time and Storage Manager Init (Only on ESP32)
#if defined(CONFIG_IDF_TARGET_ESP32)
  if (TimeManager_Init() != ESP_OK) {
    ErrorCodes_PushError(DataManager.error_code, DATA_MANAGER_ERROR_CAPACITY,
                         MRS_ERR_DS3231_INIT_FAILED);
  }

  if (StorageManager_Init() == ESP_OK) {
    StorageManager_StartTask(&DataManager);
  } else {
    ErrorCodes_PushError(DataManager.error_code, DATA_MANAGER_ERROR_CAPACITY,
                         MRS_ERR_SDCARD_INIT_FAILED);
  }
#endif

  if (ScreenManager_IsReady()) {
    ret = xTaskCreate(MenuNavigation_Task, "MenuNavigation_Task", 4096,
                      &DataManager, 5, NULL);
    if (ret != pdPASS) {
      ESP_LOGE(TAG_MAIN, "Failed to create MenuNavigation_Task: %s",
               esp_err_to_name(ret));
    } else {
      ESP_LOGI(TAG_MAIN, "MenuNavigation_Task created successfully");
    }

    ret = xTaskCreate(MenuRender_Task, "MenuRender_Task", 4096, &DataManager, 5,
                      NULL);
    if (ret != pdPASS) {
      ESP_LOGE(TAG_MAIN, "Failed to create MenuRender_Task: %s",
               esp_err_to_name(ret));
    } else {
      ESP_LOGI(TAG_MAIN, "MenuRender_Task created successfully");
    }
  }

  while (1) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
#endif
}
