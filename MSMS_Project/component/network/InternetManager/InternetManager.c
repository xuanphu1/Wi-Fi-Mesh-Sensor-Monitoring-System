#include "InternetManager.h"

#include "MeshManager.h"
#include "WifiManager.h"
#include "esp_bridge_internal.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdlib.h>

static const char *TAG = "InternetManager";

static internet_mode_t s_mode = INTERNET_MODE_NONE;
static bool s_transitioning = false;
static system_err_t s_last_error = MRS_OK;

static volatile bool s_wifi_config_stop_req = false;
static SemaphoreHandle_t s_wifi_config_exit_sem = NULL;

static volatile bool s_mesh_join_stop_req = false;
static SemaphoreHandle_t s_mesh_join_exit_sem = NULL;

typedef struct {
  DataManager_t *data;
  mesh_role_t role;
} internet_mesh_role_arg_t;

static void InternetManager_PushError(DataManager_t *data, system_err_t err) {
  if (data != NULL && err != MRS_OK) {
    ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, err);
  }
  s_last_error = err;
}

static void wifi_configuration_task(void *parameter) {
  DataManager_t *data = (DataManager_t *)parameter;
  system_err_t result = InternetManager_SwitchMode(data, INTERNET_MODE_WIFI);
  if (result != MRS_OK) {
    InternetManager_PushError(data, result);
    ESP_LOGE(TAG, "Wi-Fi mode switch failed: %s", system_err_to_name(result));
  } else {
    for (;;) {
      if (s_wifi_config_stop_req) {
        break;
      }
      if (data != NULL) {
        data->objectInfo.wifiInfo.wifiStatus =
            is_wifi_connected() ? CONNECTED : DISCONNECTED;
        if (data->objectInfo.wifiInfo.wifiStatus == CONNECTED) {
          break;
        }
      }
      for (int i = 0; i < 10 && !s_wifi_config_stop_req; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
  }

  if (data != NULL) {
    data->TaskHandle_Array[TASK_WIFI_CONFIG] = NULL;
  }
  SemaphoreHandle_t sem = s_wifi_config_exit_sem;
  if (sem != NULL) {
    xSemaphoreGive(sem);
  }
  vTaskDelete(NULL);
}

static void mesh_role_task(void *parameter) {
  internet_mesh_role_arg_t *arg = (internet_mesh_role_arg_t *)parameter;
  DataManager_t *data = arg->data;
  mesh_role_t role = arg->role;
  free(arg);

  system_err_t result = InternetManager_SwitchMode(data, INTERNET_MODE_MESH);
  if (result == MRS_OK && !MeshManager_SwitchRole(role)) {
    result = MRS_ERR_MESHMANAGER_INIT_FAILED;
  }
  if (result != MRS_OK) {
    InternetManager_PushError(data, result);
    ESP_LOGE(TAG, "Mesh role request failed: %s", system_err_to_name(result));
  }

  if (data != NULL) {
    data->TaskHandle_Array[TASK_WIFI_MESH_JOIN] = NULL;
  }
  SemaphoreHandle_t sem = s_mesh_join_exit_sem;
  if (sem != NULL) {
    xSemaphoreGive(sem);
  }
  vTaskDelete(NULL);
}

system_err_t InternetManager_StopTask(DataManager_t *data, int task_id,
                                             TickType_t timeout_ticks) {
  if (data == NULL || task_id < 0 || task_id >= TASK_NONE) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  TaskHandle_t handle = data->TaskHandle_Array[task_id];
  if (handle == NULL) {
    return MRS_OK;
  }
  if (xTaskGetCurrentTaskHandle() == handle) {
    // Current task is stopping itself: do not wait on own exit semaphore
    return MRS_OK;
  }

  if (task_id == TASK_WIFI_CONFIG) {
    s_wifi_config_stop_req = true;
    if (s_wifi_config_exit_sem != NULL) {
      if (xSemaphoreTake(s_wifi_config_exit_sem, timeout_ticks) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout waiting for TASK_WIFI_CONFIG to exit");
        return MRS_ERR_CORE_TIMEOUT;
      }
    }
    data->TaskHandle_Array[task_id] = NULL;
    s_wifi_config_stop_req = false;
  } else if (task_id == TASK_WIFI_MESH_JOIN) {
    s_mesh_join_stop_req = true;
    if (s_mesh_join_exit_sem != NULL) {
      if (xSemaphoreTake(s_mesh_join_exit_sem, timeout_ticks) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout waiting for TASK_WIFI_MESH_JOIN to exit");
        return MRS_ERR_CORE_TIMEOUT;
      }
    }
    data->TaskHandle_Array[task_id] = NULL;
    s_mesh_join_stop_req = false;
  }

  return MRS_OK;
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
  ESP_LOGI(TAG, "Cleaning network stack, destroy_default_netifs=%d",
           destroy_default_netifs);

  s_mode = INTERNET_MODE_NONE; // Set mode to NONE early

  system_err_t stop_err = InternetManager_StopTask(data, TASK_WIFI_CONFIG, pdMS_TO_TICKS(3000));
  if (stop_err != MRS_OK) {
    ESP_LOGE(TAG, "Failed to stop TASK_WIFI_CONFIG: %s", system_err_to_name(stop_err));
    return stop_err;
  }

  stop_err = InternetManager_StopTask(data, TASK_WIFI_MESH_JOIN, pdMS_TO_TICKS(3000));
  if (stop_err != MRS_OK) {
    ESP_LOGE(TAG, "Failed to stop TASK_WIFI_MESH_JOIN: %s", system_err_to_name(stop_err));
    return stop_err;
  }

  if (!wifi_manager_stop_tasks()) {
    ESP_LOGE(TAG, "wifi_manager_stop_tasks timed out");
    return MRS_ERR_CORE_TIMEOUT;
  }
  esp_err_t mesh_err = MeshManager_ResetState();
  if (mesh_err != ESP_OK) {
    ESP_LOGE(TAG, "MeshManager_ResetState failed: %s", esp_err_to_name(mesh_err));
    s_last_error = (system_err_t)mesh_err;
    return (system_err_t)mesh_err;
  }

  InternetManager_LogIgnoredEspError("esp_wifi_stop", esp_wifi_stop());
  InternetManager_LogIgnoredEspError("esp_wifi_disconnect",
                                     esp_wifi_disconnect());

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

  s_last_error = MRS_OK;
  ESP_LOGI(TAG, "Network stack is clean");
  return MRS_OK;
}

system_err_t InternetManager_Clean(DataManager_t *data,
                                   bool destroy_default_netifs) {
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

  esp_err_t mesh_err = MeshManager_StartMesh(data, MESH_ROLE_NODE);
  if (mesh_err != ESP_OK) {
    ESP_LOGE(TAG, "MeshManager_StartMesh failed: %s", esp_err_to_name(mesh_err));
    s_mode = INTERNET_MODE_NONE;
    s_last_error = (system_err_t)mesh_err;
    return (system_err_t)mesh_err;
  }

  s_mode = INTERNET_MODE_MESH;
  s_last_error = MRS_OK;
  return MRS_OK;
}

system_err_t InternetManager_Init(DataManager_t *data, internet_mode_t mode) {
  return InternetManager_SwitchMode(data, mode);
}

system_err_t InternetManager_SwitchMode(DataManager_t *data,
                                        internet_mode_t mode) {
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

system_err_t InternetManager_RequestWifiConfiguration(DataManager_t *data) {
  if (data == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (data->TaskHandle_Array[TASK_WIFI_CONFIG] != NULL) {
    return MRS_ERR_CORE_INVALID_STATE;
  }
  if (s_wifi_config_exit_sem == NULL) {
    s_wifi_config_exit_sem = xSemaphoreCreateBinary();
    if (s_wifi_config_exit_sem == NULL) {
      return MRS_ERR_CORE_OUT_OF_MEMORY;
    }
  } else {
    (void)xSemaphoreTake(s_wifi_config_exit_sem, 0);
  }

  s_wifi_config_stop_req = false;
  if (xTaskCreate(wifi_configuration_task, "wifi_config", 4096, data, 5,
                  &data->TaskHandle_Array[TASK_WIFI_CONFIG]) != pdPASS) {
    return MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
  }
  return MRS_OK;
}

system_err_t InternetManager_RequestMeshRole(DataManager_t *data, mesh_role_t role) {
  if (data == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (role != MESH_ROLE_ROOT && role != MESH_ROLE_NODE) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (data->TaskHandle_Array[TASK_WIFI_MESH_JOIN] != NULL) {
    return MRS_ERR_CORE_INVALID_STATE;
  }

  internet_mesh_role_arg_t *argument = malloc(sizeof(*argument));
  if (argument == NULL) {
    return MRS_ERR_CORE_OUT_OF_MEMORY;
  }
  argument->data = data;
  argument->role = role;

  if (s_mesh_join_exit_sem == NULL) {
    s_mesh_join_exit_sem = xSemaphoreCreateBinary();
    if (s_mesh_join_exit_sem == NULL) {
      free(argument);
      return MRS_ERR_CORE_OUT_OF_MEMORY;
    }
  } else {
    (void)xSemaphoreTake(s_mesh_join_exit_sem, 0);
  }

  s_mesh_join_stop_req = false;
  if (xTaskCreate(mesh_role_task, "mesh_role", 4096, argument, 5,
                  &data->TaskHandle_Array[TASK_WIFI_MESH_JOIN]) != pdPASS) {
    free(argument);
    return MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
  }
  return MRS_OK;
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
