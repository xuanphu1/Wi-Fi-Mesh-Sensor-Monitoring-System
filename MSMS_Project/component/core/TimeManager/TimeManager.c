#include "TimeManager.h"
#include "esp_log.h"
#include "ds3231.h"
#include "PinManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG_TIME = "TIME_MANAGER";
static i2c_dev_t rtc;
static bool is_rtc_initialized = false;

esp_err_t TimeManager_Init(void) {
    memset(&rtc, 0, sizeof(rtc));
    
    esp_err_t ret = ds3231_init_desc(&rtc, PIN_I2C_PORT_NUM, PIN_I2C_SDA, PIN_I2C_SCL);
    if (ret == ESP_OK) {
        is_rtc_initialized = true;
        ESP_LOGI(TAG_TIME, "DS3231 RTC initialized successfully");
    } else {
        ESP_LOGE(TAG_TIME, "DS3231 RTC initialization failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t TimeManager_WaitForSync(uint32_t timeout_ms) {
    // For hardware RTC, time is already synced if battery is good.
    if (!is_rtc_initialized) return ESP_FAIL;
    return ESP_OK;
}

esp_err_t TimeManager_GetTimestampStr(char *buf, size_t max_len) {
    if (buf == NULL || max_len == 0) return ESP_ERR_INVALID_ARG;
    
    if (!is_rtc_initialized) {
        strncpy(buf, "NO_RTC_TIME", max_len);
        return ESP_FAIL;
    }
    
    struct tm now = {0};
    esp_err_t ret = ds3231_get_time(&rtc, &now);
    if (ret == ESP_OK) {
        ds3231_get_time_str(&now, buf, max_len);
        return ESP_OK;
    } else {
        ESP_LOGW(TAG_TIME, "Failed to read RTC time");
        strncpy(buf, "RTC_READ_ERR", max_len);
        return ret;
    }
}

uint32_t TimeManager_GetEpochTime(void) {
    if (!is_rtc_initialized) return 0;
    
    struct tm now = {0};
    if (ds3231_get_time(&rtc, &now) == ESP_OK) {
        return (uint32_t)mktime(&now);
    }
    return 0;
}

esp_err_t TimeManager_GetTime(struct tm *timeinfo) {
    if (!is_rtc_initialized || timeinfo == NULL) return ESP_FAIL;
    return ds3231_get_time(&rtc, timeinfo);
}
