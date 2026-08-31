#include "TimeManager.h"
#include "esp_log.h"
#include "ds3231.h"
#include "PinManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG_TIME = "TIME_MANAGER";
static i2c_dev_t rtc;
static bool is_rtc_initialized = false;

esp_err_t TimeManager_Init(void) {
    memset(&rtc, 0, sizeof(rtc));
    
    esp_err_t ret = ds3231_init_desc(&rtc, PIN_I2C_PORT_NUM, PIN_I2C_SDA, PIN_I2C_SCL);
    if (ret == ESP_OK) {
        struct tm timeinfo;
        if (ds3231_get_time(&rtc, &timeinfo) == ESP_OK) {
            is_rtc_initialized = true;
            ESP_LOGI(TAG_TIME, "DS3231 RTC initialized successfully");
        } else {
            ESP_LOGE(TAG_TIME, "DS3231 RTC not found or not responding");
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG_TIME, "DS3231 RTC initialization failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t TimeManager_WaitForSync(uint32_t timeout_ms) {
    if (is_rtc_initialized) return ESP_OK;
    time_t now = time(NULL);
    if (now > 1600000000) return ESP_OK;
    return ESP_FAIL;
}

esp_err_t TimeManager_GetTimestampStr(char *buf, size_t max_len) {
    if (buf == NULL || max_len == 0) return ESP_ERR_INVALID_ARG;
    
    if (is_rtc_initialized) {
        struct tm now = {0};
        if (ds3231_get_time(&rtc, &now) == ESP_OK) {
            ds3231_get_time_str(&now, buf, max_len);
            return ESP_OK;
        }
    }
    
    // Fallback to ESP32 system time
    time_t now = time(NULL);
    if (now > 1600000000) {
        struct tm local_time;
        if (localtime_r(&now, &local_time) != NULL) {
            snprintf(buf, max_len, "%04d-%02d-%02dT%02d:%02d:%02d",
                     local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday,
                     local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
            return ESP_OK;
        }
    }
    
    strncpy(buf, "NO_TIME", max_len);
    return ESP_FAIL;
}

uint32_t TimeManager_GetEpochTime(void) {
    if (is_rtc_initialized) {
        struct tm now = {0};
        if (ds3231_get_time(&rtc, &now) == ESP_OK) {
            return (uint32_t)mktime(&now);
        }
    }
    
    // Fallback to ESP32 system time
    time_t now = time(NULL);
    if (now > 1600000000) {
        return (uint32_t)now;
    }
    return 0;
}

esp_err_t TimeManager_GetTime(struct tm *timeinfo) {
    if (timeinfo == NULL) return ESP_FAIL;
    
    if (is_rtc_initialized) {
        if (ds3231_get_time(&rtc, timeinfo) == ESP_OK) {
            return ESP_OK;
        }
    }
    
    // Fallback to ESP32 system time
    time_t now = time(NULL);
    if (now > 1600000000) {
        if (localtime_r(&now, timeinfo) != NULL) {
            timeinfo->tm_year += 1900; // Match DS3231 full year convention if expected by caller
            timeinfo->tm_mon += 1;
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t TimeManager_SetDateTime(int year, int month, int day, int hour, int min, int sec, uint32_t timestamp) {
    struct tm timeinfo = {0};
    timeinfo.tm_year = year; // DS3231 driver expects full year (e.g. 2026)
    timeinfo.tm_mon = month - 1; // 0-11
    timeinfo.tm_mday = day; // 1-31
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = min;
    timeinfo.tm_sec = sec;
    timeinfo.tm_isdst = 0;

    // Calculate weekday using standard struct tm (tm_year - 1900)
    struct tm std_tm = timeinfo;
    std_tm.tm_year = (year >= 1900) ? (year - 1900) : year;
    time_t epoch = mktime(&std_tm);
    if (epoch != (time_t)-1) {
        timeinfo.tm_wday = std_tm.tm_wday;
    }

    // 1. Cập nhật phần cứng DS3231 RTC (nếu có phát hiện)
    if (is_rtc_initialized) {
        esp_err_t rtc_err = ds3231_set_time(&rtc, &timeinfo);
        if (rtc_err == ESP_OK) {
            ESP_LOGI(TAG_TIME, "DS3231 RTC time updated: %04d-%02d-%02d %02d:%02d:%02d",
                     year, month, day, hour, min, sec);
        } else {
            ESP_LOGW(TAG_TIME, "Failed to set DS3231 time: %s", esp_err_to_name(rtc_err));
        }
    }

    // 2. Luôn cập nhật thời gian hệ thống ESP32
    time_t sec_to_set = (timestamp > 0) ? (time_t)timestamp : epoch;
    if (sec_to_set > 0) {
        struct timeval tv = {
            .tv_sec = sec_to_set,
            .tv_usec = 0
        };
        settimeofday(&tv, NULL);
        ESP_LOGI(TAG_TIME, "ESP32 system time updated: %04d-%02d-%02d %02d:%02d:%02d (ts: %lld)",
                 year, month, day, hour, min, sec, (long long)sec_to_set);
    }

    return ESP_OK;
}

esp_err_t TimeManager_SetEpochTime(uint32_t timestamp) {
    if (timestamp == 0) return ESP_ERR_INVALID_ARG;

    time_t raw_time = (time_t)timestamp;
    struct tm timeinfo;
    if (localtime_r(&raw_time, &timeinfo) != NULL) {
        int year = timeinfo.tm_year + 1900;
        int month = timeinfo.tm_mon + 1;
        int day = timeinfo.tm_mday;
        int hour = timeinfo.tm_hour;
        int min = timeinfo.tm_min;
        int sec = timeinfo.tm_sec;
        return TimeManager_SetDateTime(year, month, day, hour, min, sec, timestamp);
    }
    return ESP_FAIL;
}

