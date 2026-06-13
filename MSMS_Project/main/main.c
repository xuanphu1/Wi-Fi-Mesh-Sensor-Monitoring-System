#include "main.h"
#include "PinManager.h"
#include "DataManager.h"
#include "nvs_flash.h"
#include "BatteryManager.h"
#include "driver/uart.h"
#include "sdkconfig.h"
#include "ds3231.h"
#include "SD_Card.h"
#include "UartToGateWay.h"
#include "InternetManager.h"
#include "pcf8574.h"
#include "esp_spiffs.h"
#include <string.h>
#include <time.h>

static i2c_dev_t s_pcf8574;

#define PCF8574_ADDR 0x20
#define PCF8574_P4_MASK (1U << 4)
#define PCF8574_P6_MASK (1U << 6)

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

static esp_err_t init_pcf8574_outputs(void) {
  memset(&s_pcf8574, 0, sizeof(s_pcf8574));

  esp_err_t ret = pcf8574_init_desc(&s_pcf8574, PCF8574_ADDR, PIN_I2C_PORT_NUM,
                                    PIN_I2C_SDA, PIN_I2C_SCL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG_MAIN, "PCF8574 init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  uint8_t port_value = 0xFF;
  port_value &= (uint8_t)~PCF8574_P4_MASK;
  port_value |= PCF8574_P6_MASK;

  ret = pcf8574_port_write(&s_pcf8574, port_value);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG_MAIN, "PCF8574 write failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG_MAIN, "PCF8574 initialized: P4=LOW, P6=HIGH");
  return ESP_OK;
}

// static void rtc_sd_log_task(void *pvParameters) {
//   (void)pvParameters;
//   esp_err_t ret;

//   // Init DS3231 descriptor on shared I2C bus
//   i2c_dev_t rtc;
//   memset(&rtc, 0, sizeof(rtc));
//   ret = ds3231_init_desc(&rtc, PIN_I2C_PORT_NUM, PIN_I2C_SDA, PIN_I2C_SCL);
//   if (ret != ESP_OK) {
//     ESP_LOGE(TAG_MAIN, "DS3231 init failed: %s", esp_err_to_name(ret));
//     vTaskDelete(NULL);
//   }

//   // Init SD card
//   ret = initSDCard();
//   if (ret != ESP_OK) {
//     ESP_LOGE(TAG_MAIN, "SD card init failed: %s", esp_err_to_name(ret));
//     vTaskDelete(NULL);
//   }

//   while (1) {
//     struct tm now = {0};
//     char time_str[40];
//     char line[80];

//     ret = ds3231_get_time(&rtc, &now);
//     if (ret == ESP_OK) {
//       ds3231_get_time_str(&now, time_str, sizeof(time_str));
//       snprintf(line, sizeof(line), "%s", time_str);

//       // Append one line to log file on SD
//       esp_err_t wret = writeFinalFileSD_Card("rtc_log.txt", line);
//       if (wret != ESP_OK) {
//         ESP_LOGW(TAG_MAIN, "Failed to write RTC log to SD: %s",
//                  esp_err_to_name(wret));
//       }

//       // Print log to UART for debug
//       ESP_LOGI(TAG_MAIN, "RTC time: %s", time_str);
//     } else {
//       ESP_LOGW(TAG_MAIN, "DS3231 read failed: %s", esp_err_to_name(ret));
//     }

//     vTaskDelay(pdMS_TO_TICKS(5000)); // write every 5s for testing
//   }
// }

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
    vTaskDelay(portMAX_DELAY);
  }
  ScreenManagerInit(&MainScreen);
  /* No sensor selected yet -> menu shows plain "Port 1/2/3", not "Port 1 - BME280" from zero-init */
  for (int i = 0; i < NUM_PORTS; i++) {
    DataManager.selectedSensor[i] = SENSOR_NONE;
  }
  MenuSystemInit(&DataManager);
  UartToGateWay_Init(&DataManager);

  // Battery Manager init
  // esp_err_t battery_ret = BatteryManager_Init();
  // if (battery_ret != ESP_OK) {
  //   ESP_LOGW(TAG_MAIN, "Failed to initialize Battery Manager: %s", esp_err_to_name(battery_ret));
  // } else {
  //   // Start FreeRTOS task for periodic battery reads
  //   BatteryManager_StartTask(&DataManager);
  // }

  /* Default boot mode: join mesh as node (không chạy flow connect WiFi STA mặc định). */
  InternetManager_Init(&DataManager, INTERNET_MODE_MESH);

  ret = xTaskCreate(MenuNavigation_Task, "MenuNavigation_Task", 4096,
              &DataManager, 5, NULL);
  if (ret != pdPASS) {
    ESP_LOGE(TAG_MAIN, "Failed to create MenuNavigation_Task: %s", esp_err_to_name(ret));
    vTaskDelay(portMAX_DELAY);
  } else {
    ESP_LOGI(TAG_MAIN, "MenuNavigation_Task created successfully");
  }



  // Test task: read DS3231 time and log to SD card
  //xTaskCreate(rtc_sd_log_task, "rtc_sd_log_task", 4096, NULL, 5, NULL);
  while (1) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
