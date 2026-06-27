#include "StorageManager.h"
#include "TimeManager.h"
#include "SD_Card.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG_SD = "STORAGE_MANAGER";
static bool is_mounted = false;

esp_err_t StorageManager_Init(void) {
    esp_err_t ret = initSDCard();
    if (ret == ESP_OK) {
        is_mounted = true;
        ESP_LOGI(TAG_SD, "Storage Manager initialized successfully");
    } else {
        ESP_LOGE(TAG_SD, "Storage Manager failed to initialize SD Card");
    }
    return ret;
}

esp_err_t StorageManager_AppendToFile(const char *filepath, const char *data) {
    if (!is_mounted) return ESP_FAIL;
    return writeFinalFileSD_Card(filepath, data);
}

static void storage_task(void *pvParameters) {
    DataManager_t *data = (DataManager_t *)pvParameters;
    char timestamp[32];
    char log_buffer[128];
    
    ESP_LOGI(TAG_SD, "Storage task started.");
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // Log every 10 seconds
        
        if (TimeManager_GetTimestampStr(timestamp, sizeof(timestamp)) != ESP_OK) {
            strncpy(timestamp, "UNKNOWN_TIME", sizeof(timestamp));
        }
        
        if (data != NULL) {
            // Build the log string
            snprintf(log_buffer, sizeof(log_buffer), "[%s] BATT: %d%%, CPU: %d%%", 
                     timestamp, 
                     data->objectInfo.batteryInfo.levelPercent,
                     // Example data
                     data->objectInfo.wifiInfo.wifiStatus); 
            
            // Append to file using SD_Card wrapper
            StorageManager_AppendToFile("datalog.txt", log_buffer);
            ESP_LOGI(TAG_SD, "Logged: %s", log_buffer);
        }
    }
}

void StorageManager_StartTask(DataManager_t *data) {
    if (!is_mounted) {
        ESP_LOGW(TAG_SD, "SD card not mounted, task not started.");
        return;
    }
    xTaskCreate(storage_task, "storage_task", 4096, data, 4, NULL);
    ESP_LOGI(TAG_SD, "Storage task created");
}
