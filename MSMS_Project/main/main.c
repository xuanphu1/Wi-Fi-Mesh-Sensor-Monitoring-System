#include "main.h"
#include "BatteryManager.h"
#include "DataManager.h"
#include "InternetManager.h"
#include "PinManager.h"
#include "SD_Card.h"
#include "StorageManager.h"
#include "SystemPerfomance.h"
#include "TimeManager.h"
#include "UartToGateWay.h"
#include "driver/uart.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "pcf8574.h"
#include "sdkconfig.h"
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

void app_main(void) {

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
  ESP_ERROR_CHECK(i2cInitDevCommon());
  // ESP_ERROR_CHECK(init_pcf8574_outputs());
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  MainScreen = ssd1306_create(I2C_NUM_0, SSD1306_I2C_ADDRESS);
#pragma GCC diagnostic pop
  if (MainScreen == NULL) {
    ESP_LOGE(TAG_MAIN, "Failed to create SSD1306 handle");
    ErrorCodes_PushError(DataManager.error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_ERR_SSD1306_INIT_FAILED);
  } else {
    ScreenManagerInit(&MainScreen);
    /* No sensor selected yet -> menu shows plain "Port 1/2/3", not "Port 1 -
     * BME280" from zero-init */
    for (int i = 0; i < NUM_PORTS; i++) {
      DataManager.selectedSensor[i] = SENSOR_NONE;
    }
    MenuSystemInit(&DataManager);
  }
  UartToGateWay_Init(&DataManager);

  // Battery Manager init
  esp_err_t battery_ret = BatteryManager_Init();
  if (battery_ret != ESP_OK) {
    ESP_LOGW(TAG_MAIN, "Failed to initialize Battery Manager: %s",
             esp_err_to_name(battery_ret));
  } else {
    // Start FreeRTOS task for periodic battery reads
    BatteryManager_StartTask(&DataManager);
  }

  SystemPerformance_Init();

  /* Default boot mode: join mesh as node (không chạy flow connect WiFi STA mặc
   * định). */
  InternetManager_Init(&DataManager, INTERNET_MODE_MESH);

  // Time and Storage Manager Init (Only on ESP32)
#if defined(CONFIG_IDF_TARGET_ESP32)
  if (TimeManager_Init() != ESP_OK) {
    ErrorCodes_PushError(DataManager.error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_ERR_DS3231_INIT_FAILED);
  }

  if (StorageManager_Init() == ESP_OK) {
    StorageManager_StartTask(&DataManager);
  } else {
    ErrorCodes_PushError(DataManager.error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_ERR_SDCARD_INIT_FAILED);
  }
#endif


  if (MainScreen != NULL) {
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

  // Test task: read DS3231 time and log to SD card
  // xTaskCreate(rtc_sd_log_task, "rtc_sd_log_task", 4096, NULL, 5, NULL);
  while (1) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
