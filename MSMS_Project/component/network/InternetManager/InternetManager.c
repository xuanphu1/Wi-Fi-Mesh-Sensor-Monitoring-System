#include "InternetManager.h"

#include "MeshManager.h"
#include "WifiManager.h"
#include "esp_bridge_internal.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "InternetManager";

static internet_mode_t s_mode = INTERNET_MODE_NONE;
static bool s_transitioning = false;
static system_err_t s_last_error = MRS_OK;

static void InternetManager_PushError(DataManager_t *data, system_err_t err) {
  if (data != NULL && err != MRS_OK) {
    ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, err);
  }
  s_last_error = err;
}

static void InternetManager_StopTask(DataManager_t *data, int task_id) {
  if (data == NULL || task_id < 0 || task_id >= TASK_NONE) {
    return;
  }

  TaskHandle_t handle = data->TaskHandle_Array[task_id];
  if (handle == NULL) {
    return;
  }

  data->TaskHandle_Array[task_id] = NULL;
  if (handle != xTaskGetCurrentTaskHandle()) {
    vTaskDelete(handle);
  }
}

static void InternetManager_LogIgnoredEspError(const char *op, esp_err_t err) {
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
    ESP_LOGW(TAG, "%s failed: %s", op, esp_err_to_name(err));
  }
}

const char *InternetManager_ModeName(internet_mode_t mode) {
  switch (mode) {
  case INTERNET_MODE_NONE:
    return "none";
  case INTERNET_MODE_WIFI:
    return "wifi";
  case INTERNET_MODE_MESH:
    return "mesh";
  default:
    return "unknown";
  }
}

internet_mode_t InternetManager_GetMode(void) { return s_mode; }

bool InternetManager_IsTransitioning(void) { return s_transitioning; }

static system_err_t InternetManager_CleanCore(DataManager_t *data,
                                              bool destroy_default_netifs) {
  ESP_LOGI(TAG, "Cleaning network stack, destroy_default_netifs=%d", destroy_default_netifs);

  InternetManager_StopTask(data, TASK_MESH_LINK);
  InternetManager_StopTask(data, TASK_MESH_DATA);
  InternetManager_StopTask(data, TASK_WIFI_CONFIG);
  InternetManager_StopTask(data, TASK_WIFI_MESH_JOIN);

  wifi_manager_stop_tasks();
  MeshManager_ResetState();

  InternetManager_LogIgnoredEspError("esp_wifi_stop", esp_wifi_stop());
  InternetManager_LogIgnoredEspError("esp_wifi_disconnect", esp_wifi_disconnect());

  if (destroy_default_netifs) {
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif != NULL) {
      esp_bridge_netif_list_remove(sta_netif);
      esp_netif_destroy_default_wifi(sta_netif);
      ESP_LOGI(TAG, "Destroyed STA default netif");
    }

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif != NULL) {
      esp_bridge_netif_list_remove(ap_netif);
      esp_netif_destroy_default_wifi(ap_netif);
      ESP_LOGI(TAG, "Destroyed AP default netif");
    }
  }

  InternetManager_LogIgnoredEspError("esp_wifi_restore", esp_wifi_restore());

  s_mode = INTERNET_MODE_NONE;
  s_last_error = MRS_OK;
  ESP_LOGI(TAG, "Network stack is clean");
  return MRS_OK;
}

system_err_t InternetManager_Clean(DataManager_t *data, bool destroy_default_netifs) {
  if (s_transitioning) {
    ESP_LOGW(TAG, "Clean requested while transition is running");
    return MRS_ERR_CORE_INVALID_STATE;
  }

  s_transitioning = true;
  system_err_t ret = InternetManager_CleanCore(data, destroy_default_netifs);
  s_transitioning = false;
  return ret;
}

static system_err_t InternetManager_StartWifi(DataManager_t *data) {
  system_err_t ret = InternetManager_CleanCore(data, true);
  if (ret != MRS_OK) {
    return ret;
  }

  wifi_init_sta();

  s_mode = INTERNET_MODE_WIFI;
  s_last_error = MRS_OK;
  return MRS_OK;
}

static system_err_t InternetManager_StartMesh(DataManager_t *data) {
  if (data == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  if (MeshManager_IsStarted() && s_mode == INTERNET_MODE_MESH) {
    ESP_LOGI(TAG, "Mesh already running");
    s_last_error = MRS_OK;
    return MRS_OK;
  }

  system_err_t ret = InternetManager_CleanCore(data, true);
  if (ret != MRS_OK) {
    return ret;
  }

  MeshManager_StartMesh(data, MESH_ROLE_NODE);
  s_mode = INTERNET_MODE_MESH;
  s_last_error = MRS_OK;
  return MRS_OK;
}

system_err_t InternetManager_Init(DataManager_t *data, internet_mode_t mode) {
  return InternetManager_SwitchMode(data, mode);
}

system_err_t InternetManager_SwitchMode(DataManager_t *data, internet_mode_t mode) {
  if (s_transitioning) {
    ESP_LOGW(TAG, "Switch requested while transition is running");
    return MRS_ERR_CORE_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Switch mode: %s -> %s", InternetManager_ModeName(s_mode),
           InternetManager_ModeName(mode));
  s_transitioning = true;

  system_err_t ret = MRS_OK;
  switch (mode) {
  case INTERNET_MODE_NONE:
    ret = InternetManager_CleanCore(data, true);
    break;
  case INTERNET_MODE_WIFI:
    ret = InternetManager_StartWifi(data);
    break;
  case INTERNET_MODE_MESH:
    ret = InternetManager_StartMesh(data);
    break;
  default:
    ret = MRS_ERR_CORE_INVALID_PARAM;
    break;
  }

  if (ret != MRS_OK) {
    InternetManager_PushError(data, ret);
  }
  s_transitioning = false;
  return ret;
}

system_err_t InternetManager_GetStatus(DataManager_t *data,
                                       InternetManagerStatus_t *out) {
  if (out == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  out->mode = s_mode;
  out->transitioning = s_transitioning;
  out->mesh_started = MeshManager_IsStarted();
  out->mesh_connected = MeshManager_IsConnected();
  out->last_error = s_last_error;
  out->wifi_status = DISCONNECTED;
  out->mesh_status = DISCONNECTED;

  if (data != NULL) {
    out->wifi_status = data->objectInfo.wifiInfo.wifiStatus;
    out->mesh_status = data->objectInfo.meshInfo.meshStatus;
  }

  return MRS_OK;
}
