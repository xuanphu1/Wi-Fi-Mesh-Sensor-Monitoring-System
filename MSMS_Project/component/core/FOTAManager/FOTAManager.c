#include "FOTAManager.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "InternetManager.h"
#include "MeshManager.h"
#include "ScreenManager.h"
#include "WifiManager.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_mesh_lite.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "FOTAManager";

static volatile uint8_t s_fota_progress_percent = 0;
static volatile bool s_fota_running = false;
static TaskHandle_t s_fota_task_handle = NULL;
static TimerHandle_t s_root_lan_ota_timeout_timer = NULL;
static fota_job_info_t s_fota_job = {0};
static DataManager_t *s_data_mgr = NULL;
static fota_raw_send_fn s_raw_send_fn = NULL;
static fota_progress_cb_t s_progress_cb = NULL;

static esp_ota_handle_t s_mesh_lan_ota_handle = 0;
static const esp_partition_t *s_mesh_lan_next_partition = NULL;
static uint32_t s_mesh_lan_last_pct = 0;
static uint32_t s_root_lan_last_pct = 0;

static void fota_send_progress_json(const char *status, uint8_t percent,
                                    uint32_t bytes_read, uint32_t total_bytes,
                                    const char *running_part,
                                    const char *target_part, const char *msg);

static void root_lan_ota_timer_cb(TimerHandle_t xTimer) {
  (void)xTimer;
  if (s_fota_running && (MeshManager_GetRole() == MESH_ROLE_ROOT || esp_mesh_lite_get_level() <= 1)) {
    ESP_LOGI(TAG, "Root LAN OTA activity idle timeout -> restoring normal mesh operations");
    ScreenManager_SetSuspended(false);
    MeshManager_SetTelemetryPaused(false);
    s_fota_running = false;
  }
}

static esp_err_t provide_file_cb(esp_mesh_lite_lan_ota_file_transfer_param_t *param) {
  if (param == NULL || param->data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running == NULL) {
    ESP_LOGE(TAG, "provide_file_cb: No running partition found");
    return ESP_FAIL;
  }

  if (s_root_lan_ota_timeout_timer != NULL) {
    xTimerReset(s_root_lan_ota_timeout_timer, 0);
  }

  if (!s_fota_running) {
    s_fota_running = true;
    s_root_lan_last_pct = 0;
    ScreenManager_SetSuspended(true);
    MeshManager_SetTelemetryPaused(true);
    ESP_LOGI(TAG, "Root started serving Mesh-Lite LAN OTA (Total: %d bytes, Ver: %s)",
             param->filesize, param->fw_version ? param->fw_version : "unknown");
    ScreenShowOtaProgress("Broadcasting...", 0, 0, (uint32_t)param->filesize, param->fw_version);
    fota_send_progress_json("Downloading", 0, 0, (uint32_t)param->filesize,
                            running->label, "node_ota", "Mesh-Lite LAN OTA broadcasting to mesh...");
  }

  uint32_t bytes_sent = param->offset + param->data_size;
  uint32_t total_size = (uint32_t)param->filesize;
  uint32_t pct = (total_size > 0) ? (bytes_sent * 100U) / total_size : 0;
  if (pct > 100U) pct = 100U;

  if (pct >= s_root_lan_last_pct + 5 || pct == 100) {
    s_root_lan_last_pct = pct;
    s_fota_progress_percent = (uint8_t)pct;
    ESP_LOGI(TAG, "Root LAN OTA TX progress: %" PRIu32 "%% (%" PRIu32 " / %" PRIu32 " bytes)",
             pct, bytes_sent, total_size);
    ScreenShowOtaProgress("Broadcasting...", (uint8_t)pct, bytes_sent, total_size,
                          param->fw_version);
    fota_send_progress_json("Downloading", (uint8_t)pct, bytes_sent, total_size,
                            running->label, "node_ota",
                            "Mesh-Lite LAN OTA broadcasting to mesh...");
  }

  // Handle padding when Mesh-Lite 64KB alignment extends beyond partition size
  if (param->offset >= running->size) {
    memset(param->data, 0xFF, param->data_size);
  } else {
    size_t to_read = param->data_size;
    if (param->offset + to_read > running->size) {
      to_read = running->size - param->offset;
      memset((uint8_t *)param->data + to_read, 0xFF, param->data_size - to_read);
    }

    esp_err_t err = esp_partition_read(running, param->offset, param->data, to_read);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_partition_read offset=0x%x len=%d failed: %s",
               (unsigned int)param->offset, (int)to_read, esp_err_to_name(err));
      return err;
    }
  }

  if (pct == 100) {
    ESP_LOGI(TAG, "Root completed broadcasting LAN OTA firmware!");
    ScreenShowOtaProgress("Broadcast Done", 100, total_size, total_size, param->fw_version);
    fota_send_progress_json("Success", 100, total_size, total_size,
                            running->label, "node_ota",
                            "Mesh-Lite LAN OTA broadcast completed!");
    if (s_root_lan_ota_timeout_timer != NULL) {
      xTimerStop(s_root_lan_ota_timeout_timer, 0);
    }
    ScreenManager_SetSuspended(false);
    MeshManager_SetTelemetryPaused(false);
    s_fota_running = false;
  }

  return ESP_OK;
}

static esp_err_t get_file_cb(esp_mesh_lite_lan_ota_file_transfer_param_t *param) {
  if (param == NULL || param->data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (s_mesh_lan_ota_handle == 0) {
    s_fota_running = true;
    s_mesh_lan_last_pct = 0;
    ScreenManager_SetSuspended(true);
    MeshManager_SetTelemetryPaused(true);
    s_mesh_lan_next_partition = esp_ota_get_next_update_partition(NULL);
    if (s_mesh_lan_next_partition == NULL) {
      ESP_LOGE(TAG, "No OTA partition available for update!");
      return ESP_FAIL;
    }
    esp_err_t err = esp_ota_begin(s_mesh_lan_next_partition, OTA_WITH_SEQUENTIAL_WRITES,
                                  &s_mesh_lan_ota_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
      ScreenManager_SetSuspended(false);
      MeshManager_SetTelemetryPaused(false);
      s_fota_running = false;
      return err;
    }
    ESP_LOGI(TAG, "Mesh-Lite LAN OTA started: Target partition [%s], Total size %" PRIu32,
             s_mesh_lan_next_partition->label, (uint32_t)param->filesize);
    ScreenShowOtaProgress("Receiving...", 0, 0, (uint32_t)param->filesize, param->fw_version);
  }

  if (s_mesh_lan_next_partition && param->offset < s_mesh_lan_next_partition->size) {
    size_t to_write = param->data_size;
    if (param->offset + to_write > s_mesh_lan_next_partition->size) {
      to_write = s_mesh_lan_next_partition->size - param->offset;
    }
    esp_err_t write_err = esp_ota_write(s_mesh_lan_ota_handle, (const void *)param->data,
                                        to_write);
    if (write_err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(write_err));
      return write_err;
    }
  }

  uint32_t bytes_read = param->offset + param->data_size;
  uint32_t total_size = (uint32_t)param->filesize;
  uint32_t pct = (total_size > 0) ? (bytes_read * 100U) / total_size : 0;
  if (pct > 100U) pct = 100U;

  if (pct >= s_mesh_lan_last_pct + 5 || pct == 100) {
    s_mesh_lan_last_pct = pct;
    s_fota_progress_percent = (uint8_t)pct;
    ESP_LOGI(TAG, "Mesh-Lite OTA progress: %" PRIu32 "%% (%" PRIu32 " / %" PRIu32 " bytes)",
             pct, bytes_read, total_size);
    ScreenShowOtaProgress("Flashing...", (uint8_t)pct, bytes_read, total_size,
                          param->fw_version);
    fota_send_progress_json("Downloading", (uint8_t)pct, bytes_read, total_size,
                            esp_ota_get_running_partition() ? esp_ota_get_running_partition()->label : "ota_0",
                            s_mesh_lan_next_partition ? s_mesh_lan_next_partition->label : "ota_1",
                            "Mesh-Lite LAN OTA downloading...");
  }

  return ESP_OK;
}

static esp_err_t get_file_done(void) {
  ESP_LOGI(TAG, "Mesh-Lite LAN OTA transfer complete!");
  if (s_mesh_lan_ota_handle != 0) {
    esp_err_t end_err = esp_ota_end(s_mesh_lan_ota_handle);
    s_mesh_lan_ota_handle = 0;
    if (end_err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(end_err));
      ScreenShowOtaProgress("Error!", 100, 0, 0, "Validation Fail");
      ScreenManager_SetSuspended(false);
      MeshManager_SetTelemetryPaused(false);
      s_fota_running = false;
      return end_err;
    }
  }

  if (s_mesh_lan_next_partition != NULL) {
    esp_err_t set_err = esp_ota_set_boot_partition(s_mesh_lan_next_partition);
    if (set_err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(set_err));
      ScreenManager_SetSuspended(false);
      MeshManager_SetTelemetryPaused(false);
      s_fota_running = false;
      return set_err;
    }
  }

  ScreenShowOtaProgress("100% Rebooting", 100, 0, 0, "OTA Success");
  fota_send_progress_json("Success", 100, 0, 0,
                          esp_ota_get_running_partition() ? esp_ota_get_running_partition()->label : "ota_0",
                          s_mesh_lan_next_partition ? s_mesh_lan_next_partition->label : "ota_1",
                          "Mesh-Lite LAN OTA completed! Restarting...");
  return ESP_OK;
}

static esp_mesh_lite_lan_ota_file_transfer_cb_t s_lan_ota_cb = {
    .provide_file_cb = provide_file_cb,
    .get_file_cb = get_file_cb,
    .get_file_done = get_file_done,
};

void FOTAManager_Init(fota_raw_send_fn send_fn) {
  s_raw_send_fn = send_fn;

  // Confirm running partition is valid to cancel rollback and enable ping-pong OTA
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      ESP_LOGI(TAG, "First boot from [%s], marking app valid and cancelling rollback",
               running ? running->label : "unknown");
      esp_ota_mark_app_valid_cancel_rollback();
    }
  }

  // Register ESP-Mesh-Lite LAN OTA callbacks
  esp_mesh_lite_ota_register_file_transfer_cb(&s_lan_ota_cb);

  if (s_root_lan_ota_timeout_timer == NULL) {
    s_root_lan_ota_timeout_timer = xTimerCreate("fota_root_tmr", pdMS_TO_TICKS(60000), pdFALSE,
                                                NULL, root_lan_ota_timer_cb);
  }

  ESP_LOGI(TAG, "FOTAManager initialized (running partition: %s, LAN OTA enabled)",
           running ? running->label : "unknown");
}

void FOTAManager_RegisterProgressCallback(fota_progress_cb_t cb) {
  s_progress_cb = cb;
}

uint8_t FOTAManager_GetProgressPercent(void) {
  return s_fota_progress_percent;
}

bool FOTAManager_IsRunning(void) {
  return s_fota_running;
}

static void get_self_mac_str(char *mac_str, size_t max_len) {
  if (mac_str == NULL || max_len < 18) {
    return;
  }
  uint8_t mac[6] = {0};
  wifi_manager_get_mac(mac);
  snprintf(mac_str, max_len, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool FOTAManager_IsTargetMatch(const char *target, const char *target_detail) {
  if (target_detail == NULL || target_detail[0] == '\0') {
    return false;
  }

  // If explicitly targeted at gateway self, ignore on node/root
  if (target != NULL && strcasecmp(target, "gateway") == 0 &&
      strcasecmp(target_detail, "gateway") == 0) {
    return false;
  }

  bool is_root = (MeshManager_GetRole() == MESH_ROLE_ROOT || esp_mesh_lite_get_level() <= 1);
  int self_level = esp_mesh_lite_get_level();

  if (strcasecmp(target_detail, "all") == 0) {
    return true;
  }
  if (strcasecmp(target_detail, "root") == 0) {
    return is_root;
  }
  if (strcasecmp(target_detail, "leaf") == 0) {
    return !is_root;
  }

  if (strncasecmp(target_detail, "level:", 6) == 0) {
    int target_level = atoi(target_detail + 6);
    return (self_level == target_level);
  }

  char self_mac[18] = {0};
  get_self_mac_str(self_mac, sizeof(self_mac));
  if (strcasecmp(target_detail, self_mac) == 0) {
    return true;
  }

  return false;
}

void FOTAManager_MeshDownstreamHandler(DataManager_t *data, const char *json_str, size_t len) {
  if (json_str == NULL || len == 0) {
    return;
  }
  cJSON *root = cJSON_Parse(json_str);
  if (root == NULL) {
    return;
  }
  cJSON *type_item = cJSON_GetObjectItem(root, "type");
  if (type_item != NULL && cJSON_IsString(type_item) &&
      strcmp(type_item->valuestring, "ota_start") == 0) {
    ESP_LOGI(TAG, "Downstream ota_start command received: %s", json_str);
    FOTAManager_HandleOtaCommand(data, json_str, len);
  }
  cJSON_Delete(root);
}

static void fota_send_progress_json(const char *status, uint8_t percent,
                                    uint32_t bytes_read, uint32_t total_bytes,
                                    const char *running_part,
                                    const char *target_part, const char *msg) {
  char self_mac[18] = {0};
  get_self_mac_str(self_mac, sizeof(self_mac));

  bool is_root = (MeshManager_GetRole() == MESH_ROLE_ROOT || esp_mesh_lite_get_level() <= 1);

  // If designated targetDetail is set, prioritize it (fallback to self_mac)
  const char *target_detail = (s_fota_job.target_detail[0] != '\0') ? s_fota_job.target_detail : self_mac;
  const char *target_type = (s_fota_job.target[0] != '\0') ? s_fota_job.target : "node";

  char json_buf[512];
  int len = snprintf(
      json_buf, sizeof(json_buf),
      "{\"type\":\"ota_progress\",\"target\":\"%s\",\"targetDetail\":\"%s\","
      "\"jobId\":\"%s\",\"status\":\"%s\",\"percent\":%u,\"bytes_read\":%" PRIu32
      ",\"total_bytes\":%" PRIu32
      ",\"running_partition\":\"%s\",\"target_partition\":\"%s\",\"mac\":\"%s\",\"message\":"
      "\"%s\"}\n",
      target_type, target_detail, s_fota_job.job_id, status, (unsigned)percent, bytes_read,
      total_bytes, running_part ? running_part : "unknown",
      target_part ? target_part : "unknown", self_mac, msg ? msg : "");

  if (len > 0) {
    ESP_LOGI(TAG, "TX ota_progress: %.*s", len - 1, json_buf);
    if (is_root) {
      if (s_raw_send_fn != NULL) {
        s_raw_send_fn(json_buf, (size_t)len);
      } else {
        ESP_LOGW(TAG, "s_raw_send_fn is NULL! Progress not sent via UART");
      }
    } else {
      // Child node sends progress upstream to Root via Mesh TCP
      esp_err_t send_ret = MeshManager_SendFrameToRoot(json_buf, (size_t)len);
      if (send_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send ota_progress to Root: %s", esp_err_to_name(send_ret));
      }
    }
  }

  if (s_progress_cb != NULL) {
    s_progress_cb(s_fota_job.job_id, status, percent, bytes_read, total_bytes,
                  running_part, target_part, msg);
  }
}

static void fota_worker_task(void *pvParameters) {
  (void)pvParameters;

  s_fota_running = true;
  s_fota_progress_percent = 0;
  ScreenManager_SetSuspended(true);
  MeshManager_SetTelemetryPaused(true);

  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
  const char *running_label = running ? running->label : "unknown";
  const char *target_label = update ? update->label : "unknown";

  ESP_LOGI(TAG, "=== Starting FOTA Process ===");
  ESP_LOGI(TAG, "URL: %s", s_fota_job.url);
  ESP_LOGI(TAG, "Job ID: %s | Target: %s (%s) | Ver: %s | Size: %" PRIu32,
           s_fota_job.job_id, s_fota_job.target, s_fota_job.target_detail,
           s_fota_job.version, s_fota_job.size);
  ESP_LOGI(TAG, "Running: [%s] -> Target: [%s]", running_label, target_label);

  // 1. Notify switching mode
  ScreenShowOtaProgress("Switching Wi-Fi", 0, 0, s_fota_job.size, s_fota_job.version);
  fota_send_progress_json("Downloading", 0, 0, s_fota_job.size, running_label,
                          target_label, "Switching to Wi-Fi mode for OTA download...");

  // 2. Clean Mesh stack and connect Wi-Fi via WifiManager driver
  ESP_LOGI(TAG, "Stopping Mesh and connecting Wi-Fi via network driver (SSID='%s')...",
           s_fota_job.ssid[0] != '\0' ? s_fota_job.ssid : "boot default");

  if (s_data_mgr != NULL) {
    InternetManager_Clean(s_data_mgr, true);
  }

  bool wifi_connected = false;
  if (s_fota_job.ssid[0] != '\0') {
    wifi_connected = wifi_manager_connect_sta(s_fota_job.ssid, s_fota_job.password, 20000);
  } else {
    if (s_data_mgr != NULL) {
      InternetManager_SwitchMode(s_data_mgr, INTERNET_MODE_WIFI);
    }
    int wait_count = 0;
    while (!is_wifi_connected() && wait_count < 40) {
      vTaskDelay(pdMS_TO_TICKS(500));
      wait_count++;
    }
    wifi_connected = is_wifi_connected();
  }

  if (!wifi_connected) {
    ESP_LOGE(TAG, "Wi-Fi connection failed or timed out!");
    ScreenShowOtaProgress("Wi-Fi Timeout!", 0, 0, s_fota_job.size, s_fota_job.version);
    fota_send_progress_json("Failed", 0, 0, s_fota_job.size, running_label,
                            target_label, "Wi-Fi connection timeout");
    if (s_data_mgr != NULL) {
      InternetManager_SwitchMode(s_data_mgr, INTERNET_MODE_MESH);
    }
    ScreenManager_SetSuspended(false);
    MeshManager_SetTelemetryPaused(false);
    s_fota_running = false;
    s_fota_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "Wi-Fi connected! Starting HTTPS OTA download...");
  ScreenShowOtaProgress("Downloading", 0, 0, s_fota_job.size, s_fota_job.version);
  fota_send_progress_json("Downloading", 0, 0, s_fota_job.size, running_label,
                          target_label, "Wi-Fi connected. Downloading firmware...");

  // 4. Configure and begin HTTPS OTA
  esp_http_client_config_t http_config = {
      .url = s_fota_job.url,
      .timeout_ms = 40000,
      .keep_alive_enable = true,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };

  esp_https_ota_config_t ota_config = {
      .http_config = &http_config,
  };

  esp_https_ota_handle_t ota_handle = NULL;
  esp_err_t ret = esp_https_ota_begin(&ota_config, &ota_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(ret));
    ScreenShowOtaProgress("Connect Failed", 0, 0, s_fota_job.size, s_fota_job.version);
    fota_send_progress_json("Failed", 0, 0, s_fota_job.size, running_label,
                            target_label, "Connecting to firmware server failed");
    if (s_data_mgr != NULL) {
      InternetManager_SwitchMode(s_data_mgr, INTERNET_MODE_MESH);
    }
    ScreenManager_SetSuspended(false);
    MeshManager_SetTelemetryPaused(false);
    s_fota_running = false;
    s_fota_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }

  int total_image_size = esp_https_ota_get_image_size(ota_handle);
  if (total_image_size <= 0 && s_fota_job.size > 0) {
    total_image_size = (int)s_fota_job.size;
  }

  uint8_t last_reported_percent = 0;

  // 5. Download and flash stream loop
  do {
    ret = esp_https_ota_perform(ota_handle);
    int read_len = esp_https_ota_get_image_len_read(ota_handle);
    if (total_image_size > 0 && read_len >= 0) {
      uint32_t pct = ((uint32_t)read_len * 100U) / (uint32_t)total_image_size;
      if (pct > 100U) pct = 100U;
      s_fota_progress_percent = (uint8_t)pct;

      if (s_fota_progress_percent >= last_reported_percent + 5 ||
          s_fota_progress_percent == 100) {
        last_reported_percent = s_fota_progress_percent;
        ESP_LOGI(TAG, "OTA progress: %u%% (%d / %d bytes) -> [%s]",
                 (unsigned)s_fota_progress_percent, read_len, total_image_size,
                 target_label);
        ScreenShowOtaProgress("Flashing", s_fota_progress_percent, (uint32_t)read_len,
                              (uint32_t)total_image_size, s_fota_job.version);
        fota_send_progress_json("Downloading", s_fota_progress_percent,
                                (uint32_t)read_len, (uint32_t)total_image_size,
                                running_label, target_label,
                                "Downloading and flashing partition...");
      }
    }
  } while (ret == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

  // 6. Check download outcome
  if (ret != ESP_OK || !esp_https_ota_is_complete_data_received(ota_handle)) {
    ESP_LOGE(TAG, "OTA download failed: ret=%s", esp_err_to_name(ret));
    esp_https_ota_abort(ota_handle);
    ScreenShowOtaProgress("Download Failed", s_fota_progress_percent, 0,
                          (uint32_t)total_image_size, s_fota_job.version);
    fota_send_progress_json("Failed", s_fota_progress_percent, 0,
                            (uint32_t)total_image_size, running_label,
                            target_label, "Download or flash write interrupted");
    if (s_data_mgr != NULL) {
      InternetManager_SwitchMode(s_data_mgr, INTERNET_MODE_MESH);
    }
    ScreenManager_SetSuspended(false);
    MeshManager_SetTelemetryPaused(false);
    s_fota_running = false;
    s_fota_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }

  // 7. Validate and finish
  ScreenShowOtaProgress("Validating...", 100, (uint32_t)total_image_size,
                        (uint32_t)total_image_size, s_fota_job.version);
  fota_send_progress_json("Flashing", 100, (uint32_t)total_image_size,
                          (uint32_t)total_image_size, running_label, target_label,
                          "Validating firmware image...");

  esp_err_t finish_err = esp_https_ota_finish(ota_handle);
  if (finish_err == ESP_OK) {
    s_fota_progress_percent = 100;
    ESP_LOGI(TAG, "OTA finish SUCCESS! Target partition [%s] is verified and ready to boot",
             target_label);
    ScreenShowOtaProgress("100% Rebooting", 100, (uint32_t)total_image_size,
                          (uint32_t)total_image_size, s_fota_job.version);
    fota_send_progress_json("Success", 100, (uint32_t)total_image_size,
                            (uint32_t)total_image_size, running_label, target_label,
                            "Firmware flashed successfully! Restarting...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
  } else {
    ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(finish_err));
    ScreenShowOtaProgress("Validation Error", 100, 0, (uint32_t)total_image_size,
                          s_fota_job.version);
    fota_send_progress_json("Failed", 100, 0, (uint32_t)total_image_size,
                            running_label, target_label, "Image validation failed");
    if (s_data_mgr != NULL) {
      InternetManager_SwitchMode(s_data_mgr, INTERNET_MODE_MESH);
    }
    ScreenManager_SetSuspended(false);
    MeshManager_SetTelemetryPaused(false);
  }

  s_fota_running = false;
  s_fota_task_handle = NULL;
  vTaskDelete(NULL);
}

esp_err_t FOTAManager_StartJob(DataManager_t *data, const fota_job_info_t *job) {
  if (job == NULL || job->url[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_fota_running || s_fota_task_handle != NULL) {
    ESP_LOGW(TAG, "FOTA task is already running");
    return ESP_ERR_INVALID_STATE;
  }

  s_data_mgr = data;
  memcpy(&s_fota_job, job, sizeof(s_fota_job));

  BaseType_t ok = xTaskCreate(fota_worker_task, "fota_worker", 8192, NULL, 6,
                              &s_fota_task_handle);
  if (ok != pdPASS) {
    s_fota_task_handle = NULL;
    s_fota_running = false;
    ESP_LOGE(TAG, "Failed to create fota_worker task");
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

esp_err_t FOTAManager_HandleOtaCommand(DataManager_t *data, const char *json_str, size_t len) {
  if (json_str == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  cJSON *root = cJSON_ParseWithLength(json_str, len);
  if (root == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  cJSON *type_item = cJSON_GetObjectItem(root, "type");
  if (type_item == NULL || !cJSON_IsString(type_item) ||
      strcmp(type_item->valuestring, "ota_start") != 0) {
    cJSON_Delete(root);
    return ESP_OK; // Not an OTA command
  }

  fota_job_info_t job = {0};
  cJSON *target_item = cJSON_GetObjectItem(root, "target");
  cJSON *detail_item = cJSON_GetObjectItem(root, "targetDetail");
  cJSON *url_item = cJSON_GetObjectItem(root, "url");
  cJSON *ver_item = cJSON_GetObjectItem(root, "ver");
  cJSON *size_item = cJSON_GetObjectItem(root, "size");
  cJSON *md5_item = cJSON_GetObjectItem(root, "md5");
  cJSON *job_item = cJSON_GetObjectItem(root, "jobId");
  cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
  cJSON *password_item = cJSON_GetObjectItem(root, "password");

  if (target_item && cJSON_IsString(target_item)) {
    strlcpy(job.target, target_item->valuestring, sizeof(job.target));
  }
  if (detail_item && cJSON_IsString(detail_item)) {
    strlcpy(job.target_detail, detail_item->valuestring, sizeof(job.target_detail));
  }
  if (url_item && cJSON_IsString(url_item)) {
    strlcpy(job.url, url_item->valuestring, sizeof(job.url));
  }
  if (ver_item && cJSON_IsString(ver_item)) {
    strlcpy(job.version, ver_item->valuestring, sizeof(job.version));
  }
  if (size_item && cJSON_IsNumber(size_item)) {
    job.size = (uint32_t)size_item->valuedouble;
  }
  if (md5_item && cJSON_IsString(md5_item)) {
    strlcpy(job.md5, md5_item->valuestring, sizeof(job.md5));
  }
  if (job_item && cJSON_IsString(job_item)) {
    strlcpy(job.job_id, job_item->valuestring, sizeof(job.job_id));
  }
  if (ssid_item && cJSON_IsString(ssid_item)) {
    strlcpy(job.ssid, ssid_item->valuestring, sizeof(job.ssid));
  }
  if (password_item && cJSON_IsString(password_item)) {
    strlcpy(job.password, password_item->valuestring, sizeof(job.password));
  }

  cJSON_Delete(root);

  bool is_root = (MeshManager_GetRole() == MESH_ROLE_ROOT || esp_mesh_lite_get_level() <= 1);
  char self_mac[18] = {0};
  get_self_mac_str(self_mac, sizeof(self_mac));

  if (is_root) {
    // 1. If targeting Root (by "root" or Root's own MAC)
    if (strcasecmp(job.target_detail, "root") == 0 ||
        strcasecmp(job.target_detail, self_mac) == 0) {
      if (job.url[0] == '\0') {
        ESP_LOGE(TAG, "Root HTTPS OTA requires URL");
        return ESP_ERR_INVALID_ARG;
      }
      ESP_LOGI(TAG, "Root performing HTTPS OTA download: %s", job.job_id);
      return FOTAManager_StartJob(data, &job);
    }

    // 2. If targeting Child Nodes: Root stays in Mesh mode and serves firmware chunks via provide_file_cb
    memcpy(&s_fota_job, &job, sizeof(s_fota_job));
    s_fota_running = true;
    ScreenManager_SetSuspended(true);
    MeshManager_SetTelemetryPaused(true);
    if (s_root_lan_ota_timeout_timer != NULL) {
      xTimerReset(s_root_lan_ota_timeout_timer, 0);
    }
    ESP_LOGI(TAG, "Root received ota_start for child nodes (targetDetail='%s'). Ready to serve LAN OTA chunks",
             job.target_detail);
    ScreenShowOtaProgress("Broadcasting...", 0, 0, job.size, job.version);
    fota_send_progress_json("Downloading", 0, 0, job.size,
                            esp_ota_get_running_partition() ? esp_ota_get_running_partition()->label : "ota_0",
                            "node_ota", "Root ready for Mesh-Lite LAN OTA broadcast...");
    return ESP_OK;
  } else {
    // Child Node: Verify target matching for this node
    if (!FOTAManager_IsTargetMatch(job.target, job.target_detail)) {
      ESP_LOGI(TAG, "ota_start ignored: targetDetail '%s' does not match this child node (self: %s)",
               job.target_detail, self_mac);
      return ESP_OK;
    }

    // Save job info for progress reporting
    memcpy(&s_fota_job, &job, sizeof(s_fota_job));
    s_fota_running = true;
    ScreenManager_SetSuspended(true);
    MeshManager_SetTelemetryPaused(true);

    const esp_partition_t *running = esp_ota_get_running_partition();
    uint32_t req_size = (job.size > 0 && running && job.size <= running->size)
                            ? job.size
                            : (running ? running->size : 1835008);

    const esp_app_desc_t *app_desc = esp_app_get_description();

    esp_mesh_lite_file_transmit_config_t transmit_config = {
        .type = ESP_MESH_LITE_OTA_TRANSMIT_FIRMWARE,
        .size = req_size,
        .extern_url_ota_cb = NULL,
    };
    if (job.version[0] != '\0') {
      strncpy(transmit_config.fw_version, job.version, sizeof(transmit_config.fw_version) - 1);
    } else if (app_desc != NULL) {
      strncpy(transmit_config.fw_version, app_desc->version, sizeof(transmit_config.fw_version) - 1);
    } else {
      strncpy(transmit_config.fw_version, "062bf751-dirty", sizeof(transmit_config.fw_version) - 1);
    }

    ESP_LOGI(TAG, "Target match confirmed on child node (%s)! Requesting LAN OTA from Root (Job=%s Ver=%s Size=%" PRIu32 ")",
             self_mac, job.job_id, transmit_config.fw_version, req_size);
    ScreenShowOtaProgress("Requesting...", 0, 0, req_size, transmit_config.fw_version);

    esp_err_t ret = esp_mesh_lite_transmit_file_start(&transmit_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "esp_mesh_lite_transmit_file_start failed: %s", esp_err_to_name(ret));
      ScreenManager_SetSuspended(false);
      MeshManager_SetTelemetryPaused(false);
      s_fota_running = false;
    }
    return ret;
  }
}
