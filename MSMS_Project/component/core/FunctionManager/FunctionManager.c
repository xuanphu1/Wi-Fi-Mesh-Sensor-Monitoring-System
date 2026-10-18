#include "FunctionManager.h"

#include "BatteryManager.h"
#include "MeshManager.h"
#include "SensorManager.h"
#include "SensorRegistry.h"
#include "SensorScanner.h"
#include "UartToGateWay.h"
#include "WifiManager.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

#define TAG "FunctionManager"

typedef struct {
  mesh_role_t role;
} mesh_role_task_arg_t;

static DataManager_t *s_data;
static bool s_initialized;

static void push_error(system_err_t error) {
  if (s_data != NULL && error != MRS_OK) {
    ErrorCodes_PushError(s_data->error_code, DATA_MANAGER_ERROR_CAPACITY,
                         error);
  }
}

static void update_wifi_status(void) {
  if (s_data == NULL) {
    return;
  }
  s_data->objectInfo.wifiInfo.wifiStatus =
      is_wifi_connected() ? CONNECTED : DISCONNECTED;
}

static void wifi_configuration_task(void *parameter) {
  (void)parameter;
  system_err_t result =
      InternetManager_SwitchMode(s_data, INTERNET_MODE_WIFI);
  if (result != MRS_OK) {
    push_error(result);
    ESP_LOGE(TAG, "Wi-Fi mode switch failed: %s",
             system_err_to_name(result));
  } else {
    for (;;) {
      update_wifi_status();
      if (s_data->objectInfo.wifiInfo.wifiStatus == CONNECTED) {
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  s_data->TaskHandle_Array[TASK_WIFI_CONFIG] = NULL;
  vTaskDelete(NULL);
}

static void mesh_role_task(void *parameter) {
  mesh_role_task_arg_t *argument = (mesh_role_task_arg_t *)parameter;
  mesh_role_t role = argument->role;
  free(argument);

  system_err_t result =
      InternetManager_SwitchMode(s_data, INTERNET_MODE_MESH);
  if (result == MRS_OK && !MeshManager_SwitchRole(role)) {
    result = MRS_ERR_MESHMANAGER_INIT_FAILED;
  }
  if (result != MRS_OK) {
    push_error(result);
    ESP_LOGE(TAG, "Mesh role request failed: %s",
             system_err_to_name(result));
  }

  s_data->TaskHandle_Array[TASK_WIFI_MESH_JOIN] = NULL;
  vTaskDelete(NULL);
}

static system_err_t uart_role_request(mesh_role_t role, void *context) {
  (void)context;
  return FunctionManager_RequestMeshRole(role);
}

system_err_t FunctionManager_Init(DataManager_t *data) {
  if (data == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_data != NULL) {
    if (s_data != data) {
      return MRS_ERR_CORE_ALREADY_INIT;
    }
    return s_initialized ? MRS_OK : MRS_ERR_CORE_INVALID_STATE;
  }
  s_data = data;

  UartToGateWay_SetRoleRequestHandler(uart_role_request, NULL);
  system_err_t result = UartToGateWay_Init(data);
  if (result != MRS_OK) {
    s_data = NULL;
    return result;
  }

  result = SensorManager_Init(data);
  if (result != MRS_OK) {
    s_data = NULL;
    return result;
  }
  SensorManager_SetUartHooks(UartToGateWay_Pause, UartToGateWay_Resume);

  result = SensorManager_Start();
  if (result != MRS_OK) {
    push_error(result);
    return result;
  }

  esp_err_t scanner_result = SensorScanner_Init(data);
  if (scanner_result != ESP_OK) {
    result = (system_err_t)scanner_result;
    push_error(result);
    return result;
  }
  scanner_result = SensorScanner_StartTask(data);
  if (scanner_result != ESP_OK) {
    result = MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
    push_error(result);
    return result;
  }
  s_initialized = true;
  return MRS_OK;
}

void FunctionManager_SetSensorBusGuard(
    function_manager_bus_lock_fn lock_fn,
    function_manager_bus_unlock_fn unlock_fn, void *context) {
  SensorManager_SetBusGuard(lock_fn, unlock_fn, context);
}

system_err_t FunctionManager_AssignSensor(PortId_t port,
                                          SensorType_t sensor_type) {
  if (s_data == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  system_err_t result = SensorManager_AssignSensor(port, sensor_type);
  if (result != MRS_OK) {
    push_error(result);
    return result;
  }
  if (s_data->on_sensor_selected != NULL) {
    s_data->on_sensor_selected(s_data, port);
  }
  return MRS_OK;
}

system_err_t FunctionManager_ResetAllSensorPorts(void) {
  if (s_data == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  system_err_t result = SensorManager_ResetAll();
  if (result != MRS_OK) {
    push_error(result);
    return result;
  }
  if (s_data->on_ports_reset != NULL) {
    s_data->on_ports_reset(s_data);
  }
  return MRS_OK;
}

system_err_t FunctionManager_GetSensorSnapshot(PortId_t port,
                                               SensorType_t *out_sensor_type,
                                               SensorData_t *out_data) {
  return SensorManager_GetSnapshot(port, out_sensor_type, out_data);
}

system_err_t FunctionManager_GetSensorDescriptor(
    SensorType_t sensor_type, sensor_descriptor_t *out_descriptor) {
  if (out_descriptor == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  return sensor_registry_get_descriptor(sensor_type, out_descriptor)
             ? MRS_OK
             : MRS_ERR_SENSORS_NOT_FOUND;
}

const char *FunctionManager_GetSensorName(SensorType_t sensor_type) {
  return sensor_type_to_name(sensor_type);
}

size_t FunctionManager_GetSensorCountByInterface(
    TypeCommunication_t interface) {
  return sensor_registry_get_count_by_interface(interface);
}

system_err_t FunctionManager_GetSensorDescriptorAtInterface(
    TypeCommunication_t interface, size_t index,
    sensor_descriptor_t *out_descriptor) {
  if (out_descriptor == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  return sensor_registry_get_descriptor_at_interface(interface, index,
                                                     out_descriptor)
             ? MRS_OK
             : MRS_ERR_SENSORS_NOT_FOUND;
}

system_err_t FunctionManager_ScanAndAssignSensors(
    function_sensor_scan_result_t *out_result) {
  if (s_data == NULL || out_result == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  out_result->detected_count = 0;
  out_result->assigned_count = 0;
  if (!SensorScanner_IsAllowed()) {
    return MRS_ERR_CORE_INVALID_STATE;
  }

  int detected = SensorScanner_Scan(s_data, out_result->detected, NUM_PORTS);
  if (detected < 0) {
    return MRS_FAIL;
  }
  out_result->detected_count = (size_t)detected;
  out_result->assigned_count = (size_t)SensorScanner_AutoAssign(
      s_data, out_result->detected, out_result->detected_count);
  return MRS_OK;
}

bool FunctionManager_IsSensorScanAllowed(void) {
  return SensorScanner_IsAllowed();
}

void FunctionManager_SetAutoScanEnabled(bool enable) {
  SensorScanner_SetAutoScanEnabled(enable);
}

bool FunctionManager_IsAutoScanEnabled(void) {
  return SensorScanner_IsAutoScanEnabled();
}

void FunctionManager_TriggerSensorScan(void) {
  SensorScanner_TriggerScan();
}

system_err_t FunctionManager_RequestWifiConfiguration(void) {
  if (s_data == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (s_data->TaskHandle_Array[TASK_WIFI_CONFIG] != NULL) {
    return MRS_ERR_CORE_INVALID_STATE;
  }
  if (xTaskCreate(wifi_configuration_task, "wifi_config", 4096, NULL, 5,
                  &s_data->TaskHandle_Array[TASK_WIFI_CONFIG]) != pdPASS) {
    return MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
  }
  return MRS_OK;
}

system_err_t FunctionManager_RequestMeshRole(mesh_role_t role) {
  if (s_data == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (role != MESH_ROLE_ROOT && role != MESH_ROLE_NODE) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_data->TaskHandle_Array[TASK_WIFI_MESH_JOIN] != NULL) {
    return MRS_ERR_CORE_INVALID_STATE;
  }

  mesh_role_task_arg_t *argument = malloc(sizeof(*argument));
  if (argument == NULL) {
    return MRS_ERR_CORE_OUT_OF_MEMORY;
  }
  argument->role = role;
  if (xTaskCreate(mesh_role_task, "mesh_role", 4096, argument, 5,
                  &s_data->TaskHandle_Array[TASK_WIFI_MESH_JOIN]) != pdPASS) {
    free(argument);
    return MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
  }
  return MRS_OK;
}

system_err_t FunctionManager_GetNetworkStatus(InternetManagerStatus_t *out) {
  if (s_data == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  return InternetManager_GetStatus(s_data, out);
}

system_err_t FunctionManager_SetActuator(int gpio_num, bool enabled) {
  if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  gpio_config_t config = {
      .pin_bit_mask = 1ULL << gpio_num,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  esp_err_t result = gpio_config(&config);
  if (result == ESP_OK) {
    result = gpio_set_level((gpio_num_t)gpio_num, enabled ? 1 : 0);
  }
  if (result != ESP_OK) {
    push_error(result);
  }
  return result;
}

system_err_t FunctionManager_RefreshBatteryInfo(void) {
  if (s_data == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  BatteryManager_UpdateInfo(&s_data->objectInfo.batteryInfo);
  return MRS_OK;
}
