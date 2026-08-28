#include "FOTAManager.h"

#include <string.h>

#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "FOTAManager";
static volatile uint8_t s_fota_progress_percent = 0;
static volatile bool s_fota_running = false;
static volatile esp_err_t s_fota_last_result = ESP_OK;
static TaskHandle_t s_fota_task_handle = NULL;
static char s_fota_url[256] = {0};

void __attribute__((weak)) suspendAllTask(void) {}
void __attribute__((weak)) resumeAllTask(void) {}

uint8_t fota_get_progress_percent(void)
{
    return s_fota_progress_percent;
}

bool fota_is_running(void)
{
    return s_fota_running;
}

esp_err_t fota_get_last_result(void)
{
    return s_fota_last_result;
}

static void fota_update_progress(esp_https_ota_handle_t handle)
{
    int image_size = esp_https_ota_get_image_size(handle);
    int image_read = esp_https_ota_get_image_len_read(handle);

    if (image_size > 0 && image_read >= 0) {
        uint32_t percent = ((uint32_t)image_read * 100U) / (uint32_t)image_size;
        if (percent > 100U) {
            percent = 100U;
        }
        s_fota_progress_percent = (uint8_t)percent;
    }
}

esp_err_t do_manual_http_ota(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    /* Dùng esp_https_ota để ghi ảnh OTA vào partition. */
    esp_http_client_config_t http_config = {
        .url = url,
        .timeout_ms = 60000,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    s_fota_running = true;
    s_fota_progress_percent = 0;
    s_fota_last_result = ESP_ERR_INVALID_STATE;

    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(ret));
        s_fota_last_result = ret;
        s_fota_running = false;
        return ret;
    }

    do {
        ret = esp_https_ota_perform(ota_handle);
        fota_update_progress(ota_handle);
        ESP_LOGI(TAG, "OTA progress: %u%%", (unsigned)s_fota_progress_percent);
    } while (ret == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_perform failed: %s", esp_err_to_name(ret));
        esp_https_ota_abort(ota_handle);
        s_fota_last_result = ret;
        s_fota_running = false;
        return ret;
    }

    if (!esp_https_ota_is_complete_data_received(ota_handle)) {
        ESP_LOGE(TAG, "Complete OTA data was not received");
        esp_https_ota_abort(ota_handle);
        s_fota_last_result = ESP_FAIL;
        s_fota_running = false;
        return ESP_FAIL;
    }

    ret = esp_https_ota_finish(ota_handle);
    if (ret == ESP_OK) {
        s_fota_progress_percent = 100;
    } else {
        ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(ret));
    }

    s_fota_last_result = ret;
    s_fota_running = false;
    return ret;
}

static void fota_gateway_task(void *arg)
{
    (void)arg;

    suspendAllTask();
    esp_err_t ret = do_manual_http_ota(s_fota_url);
    resumeAllTask();

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Gateway OTA successful, restarting...");
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_restart();
    }

    ESP_LOGE(TAG, "Gateway OTA failed: %s", esp_err_to_name(ret));
    s_fota_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t fota_start_gateway_ota(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_fota_task_handle != NULL || s_fota_running) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(s_fota_url, url, sizeof(s_fota_url) - 1);
    s_fota_url[sizeof(s_fota_url) - 1] = '\0';
    s_fota_running = true;
    s_fota_progress_percent = 0;
    s_fota_last_result = ESP_ERR_INVALID_STATE;

    BaseType_t ok = xTaskCreate(fota_gateway_task, "fota_gateway", 8192, NULL,
                                6, &s_fota_task_handle);
    if (ok != pdPASS) {
        s_fota_task_handle = NULL;
        s_fota_running = false;
        s_fota_last_result = ESP_ERR_NO_MEM;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void FOTA_task(void *pvParameters)
{
    (void)pvParameters;

    /* Placeholder: luồng OTA thật cần phải được kích hoạt từ WebSocket / HTTP command. */
    ESP_LOGW(TAG, "FOTA_task is running, but no OTA trigger is wired yet.");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
