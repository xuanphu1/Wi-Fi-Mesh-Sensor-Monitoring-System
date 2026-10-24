#include "SensorScanner.h"

#include "SensorManager.h"
#include "SensorRegistry.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#define SENSOR_SCANNER_NVS_NAMESPACE "scanner_cfg"
#define SENSOR_SCANNER_NVS_KEY "auto_scan"
#define SENSOR_SCANNER_INTERVAL_MS 12000

static const char *TAG = "SensorScanner";
static uint32_t s_current_scan_interval_ms = SENSOR_SCANNER_INTERVAL_MS;
static bool s_backoff_active = false;

static bool s_auto_scan_enabled;
static TaskHandle_t s_scan_task_handle = NULL;
static volatile bool s_scan_stop_requested = false;
static SemaphoreHandle_t s_scan_task_stopped_sem = NULL;

typedef enum {
  SCANNER_TASK_STATE_STOPPED = 0,
  SCANNER_TASK_STATE_STARTING,
  SCANNER_TASK_STATE_RUNNING,
  SCANNER_TASK_STATE_STOPPING,
} scanner_task_state_t;

static volatile scanner_task_state_t s_scan_task_state = SCANNER_TASK_STATE_STOPPED;
static DataManager_t *s_data;
static sensor_scanner_permission_fn s_permission_policy = NULL;
static sensor_scanner_assignment_cb_t s_assignment_cb = NULL;
static void *s_assignment_cb_context = NULL;

esp_err_t SensorScanner_Init(DataManager_t *data) {
  if (data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_data != NULL) {
    return s_data == data ? ESP_OK : ESP_ERR_INVALID_STATE;
  }
  s_data = data;

  if (s_scan_task_stopped_sem == NULL) {
    s_scan_task_stopped_sem = xSemaphoreCreateBinary();
    if (s_scan_task_stopped_sem == NULL) {
      s_data = NULL;
      return ESP_ERR_NO_MEM;
    }
  }
  s_scan_stop_requested = false;

  nvs_handle_t handle;
  if (nvs_open(SENSOR_SCANNER_NVS_NAMESPACE, NVS_READONLY, &handle) ==
      ESP_OK) {
    uint8_t value = 0;
    if (nvs_get_u8(handle, SENSOR_SCANNER_NVS_KEY, &value) == ESP_OK) {
      s_auto_scan_enabled = value != 0;
    }
    nvs_close(handle);
  }

  s_data->objectInfo.auto_scan_enabled = s_auto_scan_enabled;
  ESP_LOGI(TAG, "Initialized (auto scan: %s)",
           s_auto_scan_enabled ? "enabled" : "disabled");
  return ESP_OK;
}

esp_err_t SensorScanner_StopTask(void) {
  if (s_scan_task_state == SCANNER_TASK_STATE_STOPPED && s_scan_task_handle == NULL) {
    return ESP_OK;
  }
  s_scan_stop_requested = true;
  if (s_scan_task_handle != NULL) {
    xTaskNotifyGive(s_scan_task_handle);
  }
  if (s_scan_task_stopped_sem == NULL ||
      xSemaphoreTake(s_scan_task_stopped_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
    ESP_LOGE(TAG, "Timeout waiting for auto-scan task to stop");
    return ESP_ERR_TIMEOUT;
  }
  s_scan_stop_requested = false;
  s_scan_task_handle = NULL;
  s_scan_task_state = SCANNER_TASK_STATE_STOPPED;
  return ESP_OK;
}

esp_err_t SensorScanner_Deinit(void) {
  esp_err_t stop_err = SensorScanner_StopTask();
  if (stop_err != ESP_OK) {
    ESP_LOGE(TAG, "SensorScanner_Deinit: StopTask failed: %s", esp_err_to_name(stop_err));
    return stop_err;
  }
  if (s_scan_task_stopped_sem != NULL) {
    SemaphoreHandle_t sem = s_scan_task_stopped_sem;
    s_scan_task_stopped_sem = NULL;
    vSemaphoreDelete(sem);
  }
  s_data = NULL;
  s_permission_policy = NULL;
  s_assignment_cb = NULL;
  s_assignment_cb_context = NULL;
  return ESP_OK;
}

void SensorScanner_SetPermissionPolicy(sensor_scanner_permission_fn policy_fn) {
  s_permission_policy = policy_fn;
}

void SensorScanner_SetAssignmentCallback(sensor_scanner_assignment_cb_t callback,
                                         void *context) {
  s_assignment_cb = callback;
  s_assignment_cb_context = context;
}

bool SensorScanner_IsAllowed(void) {
  if (s_data == NULL) {
    return false;
  }
  if (s_permission_policy != NULL) {
    return s_permission_policy();
  }
  return true;
}

int SensorScanner_Scan(DataManager_t *data, SensorType_t *found_sensors,
                       size_t max_sensors) {
  if (data == NULL || data != s_data || found_sensors == NULL ||
      max_sensors == 0 || !SensorScanner_IsAllowed() ||
      !SensorManager_HasEmptyPort()) {
    return 0;
  }

  bool shared_interface_found[COMMUNICATION_DIGITAL + 1] = {false};
  int found_count = 0;
  size_t driver_count = sensor_registry_get_count();
  size_t available_ports = SensorManager_GetAvailablePortCount();
  size_t result_capacity =
      max_sensors < available_ports ? max_sensors : available_ports;

  for (size_t i = 0;
       i < driver_count && (size_t)found_count < result_capacity; i++) {
    sensor_descriptor_t descriptor = {0};
    if (!sensor_registry_get_descriptor_at(i, &descriptor)) {
      continue;
    }
    if (SensorManager_IsSensorAssigned(descriptor.type)) {
      continue;
    }

    /* UART and pulse sensors share one physical port per interface. */
    if ((descriptor.interface == COMMUNICATION_UART ||
         descriptor.interface == COMMUNICATION_PULSE) &&
        (shared_interface_found[descriptor.interface] ||
         SensorManager_IsInterfaceAssigned(descriptor.interface))) {
      continue;
    }

    SensorData_t sample = {0};
    system_err_t result =
        SensorManager_ProbeSensor(descriptor.type, &sample);
    if (result != MRS_OK) {
      if (result == MRS_ERR_CORE_TIMEOUT) {
        if (!s_backoff_active) {
          ESP_LOGW(TAG, "UART ownership timeout during probe %s; applying backoff", descriptor.name);
          s_backoff_active = true;
        }
        s_current_scan_interval_ms = (s_current_scan_interval_ms < 60000) ? (s_current_scan_interval_ms * 2) : 60000;
      }
      ESP_LOGD(TAG, "Probe missed %s: %s", descriptor.name,
               system_err_to_name(result));
      continue;
    }
    s_current_scan_interval_ms = SENSOR_SCANNER_INTERVAL_MS;
    s_backoff_active = false;

    found_sensors[found_count++] = descriptor.type;
    if (descriptor.interface == COMMUNICATION_UART ||
        descriptor.interface == COMMUNICATION_PULSE) {
      shared_interface_found[descriptor.interface] = true;
    }
    ESP_LOGI(TAG, "Detected %s", descriptor.name);
  }

  ESP_LOGI(TAG, "Scan complete: %d sensor(s)", found_count);
  return found_count;
}

int SensorScanner_AutoAssign(DataManager_t *data,
                             const SensorType_t *found_sensors,
                             size_t count) {
  if (data == NULL || data != s_data || found_sensors == NULL) {
    return 0;
  }

  int assigned_count = 0;
  for (size_t i = 0; i < count; i++) {
    if (SensorManager_IsSensorAssigned(found_sensors[i])) {
      continue;
    }

    PortId_t port = PORT_NONE;
    system_err_t result =
        SensorManager_AssignFirstAvailable(found_sensors[i], &port);
    if (result != MRS_OK) {
      ESP_LOGW(TAG, "Could not assign %s: %s",
               sensor_type_to_name(found_sensors[i]),
               system_err_to_name(result));
      (void)SensorManager_ReleaseSensorIfUnassigned(found_sensors[i]);
      continue;
    }

    ESP_LOGI(TAG, "Assigned %s to port %d",
             sensor_type_to_name(found_sensors[i]), (int)port + 1);
    assigned_count++;

    if (s_assignment_cb != NULL) {
      s_assignment_cb(s_assignment_cb_context, port, found_sensors[i]);
    }
  }

  return assigned_count;
}

static void sensor_auto_scan_task(void *parameter) {
  DataManager_t *data = (DataManager_t *)parameter;

  while (!s_scan_stop_requested) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(s_current_scan_interval_ms));
    if (s_scan_stop_requested) {
      break;
    }
    if (!s_auto_scan_enabled || !SensorScanner_IsAllowed() ||
        !SensorManager_HasEmptyPort()) {
      continue;
    }

    SensorType_t found[NUM_PORTS];
    int count = SensorScanner_Scan(data, found, NUM_PORTS);
    if (s_scan_stop_requested) {
      break;
    }
    if (count > 0) {
      (void)SensorScanner_AutoAssign(data, found, (size_t)count);
    }
  }

  s_scan_task_state = SCANNER_TASK_STATE_STOPPING;
  SemaphoreHandle_t sem = s_scan_task_stopped_sem;
  if (sem != NULL) {
    xSemaphoreGive(sem);
  }
  vTaskDelete(NULL);
}

esp_err_t SensorScanner_StartTask(DataManager_t *data) {
  if (data == NULL || data != s_data) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_scan_task_handle != NULL) {
    return ESP_OK;
  }
  s_scan_stop_requested = false;
  if (s_scan_task_stopped_sem != NULL) {
    (void)xSemaphoreTake(s_scan_task_stopped_sem, 0);
  }
  s_scan_task_state = SCANNER_TASK_STATE_STARTING;
  if (xTaskCreate(sensor_auto_scan_task, "sensor_scan", 4096, data, 4,
                  &s_scan_task_handle) != pdPASS) {
    s_scan_task_handle = NULL;
    s_scan_task_state = SCANNER_TASK_STATE_STOPPED;
    return ESP_FAIL;
  }
  s_scan_task_state = SCANNER_TASK_STATE_RUNNING;
  return ESP_OK;
}

void SensorScanner_SetAutoScanEnabled(bool enable) {
  s_auto_scan_enabled = enable;
  if (s_data != NULL) {
    s_data->objectInfo.auto_scan_enabled = enable;
  }

  nvs_handle_t handle;
  if (nvs_open(SENSOR_SCANNER_NVS_NAMESPACE, NVS_READWRITE, &handle) ==
      ESP_OK) {
    (void)nvs_set_u8(handle, SENSOR_SCANNER_NVS_KEY, enable ? 1 : 0);
    (void)nvs_commit(handle);
    nvs_close(handle);
  }
}

bool SensorScanner_IsAutoScanEnabled(void) {
  return s_auto_scan_enabled;
}

void SensorScanner_TriggerScan(void) {
  s_current_scan_interval_ms = SENSOR_SCANNER_INTERVAL_MS;
  s_backoff_active = false;
  if (s_scan_task_handle != NULL) {
    xTaskNotifyGive(s_scan_task_handle);
  }
}
