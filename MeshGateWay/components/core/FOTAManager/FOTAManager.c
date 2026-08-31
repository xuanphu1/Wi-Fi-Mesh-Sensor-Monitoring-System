#include "FOTAManager.h"

#include <inttypes.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "FOTAManager";
static volatile uint8_t s_fota_progress_percent = 0;
static volatile bool s_fota_running = false;
static volatile esp_err_t s_fota_last_result = ESP_OK;
static TaskHandle_t s_fota_task_handle = NULL;
static fota_job_info_t s_fota_job = {0};
static fota_progress_cb_t s_progress_cb = NULL;

void __attribute__((weak)) suspendAllTask(void) {}
void __attribute__((weak)) resumeAllTask(void) {}

esp_err_t fota_manager_init(void) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running == NULL) {
    ESP_LOGW(TAG, "Cannot determine running partition");
    return ESP_FAIL;
  }

  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      ESP_LOGI(TAG, "First boot from [%s], marking app valid and cancelling rollback",
               running->label);
      esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mark app valid: %s", esp_err_to_name(err));
        return err;
      }
    } else {
      ESP_LOGI(TAG, "Running partition [%s] state: 0x%x (verified)",
               running->label, (unsigned)ota_state);
    }
  } else {
    ESP_LOGI(TAG, "Running partition [%s] (standard/factory boot)", running->label);
  }

  const esp_partition_t *next_update = esp_ota_get_next_update_partition(NULL);
  ESP_LOGI(TAG, "OTA Ready: Running [%s] -> Next update partition [%s]",
           running->label, next_update ? next_update->label : "none");
  return ESP_OK;
}

void fota_register_progress_callback(fota_progress_cb_t cb) {
  s_progress_cb = cb;
}

uint8_t fota_get_progress_percent(void) {
  return s_fota_progress_percent;
}

bool fota_is_running(void) {
  return s_fota_running;
}

esp_err_t fota_get_last_result(void) {
  return s_fota_last_result;
}

static void fota_notify(const char *status, uint8_t percent, uint32_t bytes_read,
                        uint32_t total_bytes, const char *running_part,
                        const char *target_part, const char *msg) {
  if (s_progress_cb) {
    s_progress_cb(s_fota_job.job_id, status, percent, bytes_read, total_bytes,
                  running_part, target_part, msg);
  }
}

esp_err_t do_manual_http_ota(const char *url) {
  fota_job_info_t info = {0};
  if (url) {
    strncpy(info.url, url, sizeof(info.url) - 1);
  }
  return fota_start_gateway_ota_with_info(&info);
}

static esp_err_t fota_perform_download_and_flash(const fota_job_info_t *job) {
  if (job == NULL || job->url[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }

  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
  const char *running_label = running ? running->label : "unknown";
  const char *target_label = update ? update->label : "unknown";

  ESP_LOGI(TAG, "Starting Gateway OTA from URL: %s", job->url);
  ESP_LOGI(TAG,
           "Running partition: %s (0x%08" PRIx32 "), Target partition: %s (0x%08" PRIx32 ", size: %" PRIu32 " KB)",
           running_label, running ? (uint32_t)running->address : 0,
           target_label, update ? (uint32_t)update->address : 0,
           update ? (uint32_t)(update->size / 1024) : 0);

  esp_http_client_config_t http_config = {
      .url = job->url,
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

  // 1. Thông báo bắt đầu quá trình tải (Downloading 0%)
  fota_notify("Downloading", 0, 0, job->size, running_label, target_label,
              "Connecting to firmware server...");

  esp_https_ota_handle_t ota_handle = NULL;
  esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(ret));
    s_fota_last_result = ret;
    s_fota_running = false;
    fota_notify("Failed", 0, 0, job->size, running_label, target_label,
                esp_err_to_name(ret));
    return ret;
  }

  uint8_t last_reported_percent = 0;
  int total_image_size = esp_https_ota_get_image_size(ota_handle);
  if (total_image_size <= 0 && job->size > 0) {
    total_image_size = (int)job->size;
  }

  // 2. Vòng lặp tải và nạp flash theo từng chunk
  do {
    ret = esp_https_ota_perform(ota_handle);
    int read_len = esp_https_ota_get_image_len_read(ota_handle);
    if (total_image_size > 0 && read_len >= 0) {
      uint32_t pct = ((uint32_t)read_len * 100U) / (uint32_t)total_image_size;
      if (pct > 100U)
        pct = 100U;
      s_fota_progress_percent = (uint8_t)pct;

      if (s_fota_progress_percent >= last_reported_percent + 1 ||
          s_fota_progress_percent == 100) {
        last_reported_percent = s_fota_progress_percent;
        ESP_LOGI(TAG, "OTA progress: %u%% (%d / %d bytes) -> [%s]",
                 (unsigned)s_fota_progress_percent, read_len, total_image_size,
                 target_label);
        fota_notify("Downloading", s_fota_progress_percent, (uint32_t)read_len,
                    (uint32_t)total_image_size, running_label, target_label,
                    "Downloading and flashing...");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  } while (ret == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_https_ota_perform failed: %s", esp_err_to_name(ret));
    esp_https_ota_abort(ota_handle);
    s_fota_last_result = ret;
    s_fota_running = false;
    fota_notify("Failed", s_fota_progress_percent, 0, (uint32_t)total_image_size,
                running_label, target_label, "Download or flash write interrupted");
    return ret;
  }

  if (!esp_https_ota_is_complete_data_received(ota_handle)) {
    ESP_LOGE(TAG, "Complete OTA data was not received");
    esp_https_ota_abort(ota_handle);
    s_fota_last_result = ESP_FAIL;
    s_fota_running = false;
    fota_notify("Failed", s_fota_progress_percent, 0, (uint32_t)total_image_size,
                running_label, target_label, "Incomplete image data");
    return ESP_FAIL;
  }

  // 3. Flashing / Validating
  fota_notify("Flashing", 100, (uint32_t)total_image_size,
              (uint32_t)total_image_size, running_label, target_label,
              "Validating firmware image...");

  ret = esp_https_ota_finish(ota_handle);
  if (ret == ESP_OK) {
    s_fota_progress_percent = 100;
    s_fota_last_result = ESP_OK;
    ESP_LOGI(TAG, "OTA finish OK! Target partition [%s] is verified and ready to boot",
             target_label);
    // 4. Success
    fota_notify("Success", 100, (uint32_t)total_image_size,
                (uint32_t)total_image_size, running_label, target_label,
                "Firmware flashed successfully! Rebooting Gateway...");
  } else {
    ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(ret));
    s_fota_last_result = ret;
    fota_notify("Failed", 100, 0, (uint32_t)total_image_size, running_label,
                target_label, "Image validation failed");
  }

  s_fota_running = false;
  return ret;
}

static void fota_gateway_task(void *arg) {
  (void)arg;

  suspendAllTask();
  esp_err_t ret = fota_perform_download_and_flash(&s_fota_job);
  resumeAllTask();

  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Gateway OTA successful, restarting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
  }

  ESP_LOGE(TAG, "Gateway OTA failed: %s", esp_err_to_name(ret));
  s_fota_task_handle = NULL;
  vTaskDelete(NULL);
}

esp_err_t fota_start_gateway_ota_with_info(const fota_job_info_t *info) {
  if (info == NULL || info->url[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_fota_task_handle != NULL || s_fota_running) {
    ESP_LOGW(TAG, "OTA task is already running");
    return ESP_ERR_INVALID_STATE;
  }

  memcpy(&s_fota_job, info, sizeof(s_fota_job));
  s_fota_running = true;
  s_fota_progress_percent = 0;
  s_fota_last_result = ESP_ERR_INVALID_STATE;

  BaseType_t ok = xTaskCreatePinnedToCore(fota_gateway_task, "fota_gateway", 8192,
                                          NULL, 6, &s_fota_task_handle, 0);
  if (ok != pdPASS) {
    s_fota_task_handle = NULL;
    s_fota_running = false;
    s_fota_last_result = ESP_ERR_NO_MEM;
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

esp_err_t fota_start_gateway_ota(const char *url) {
  fota_job_info_t info = {0};
  if (url) {
    strncpy(info.url, url, sizeof(info.url) - 1);
  }
  return fota_start_gateway_ota_with_info(&info);
}

void FOTA_task(void *pvParameters) {
  (void)pvParameters;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
