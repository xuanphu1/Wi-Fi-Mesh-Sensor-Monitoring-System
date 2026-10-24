#include "FunctionManager.h"

#include "ActuatorManager.h"
#include "BatteryManager.h"
#include "InternetManager.h"
#include "MeshManager.h"
#include "PinManager.h"
#include "SensorManager.h"
#include "SensorRegistry.h"
#include "SensorScanner.h"
#include "UartResourceManager.h"
#include "UartToGateWay.h"
#include "WifiManager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdlib.h>

#define TAG "FunctionManager"

typedef enum {
  STAGE_UNINITIALIZED = 0,
  STAGE_ACTUATOR_INIT,
  STAGE_UART_INIT,
  STAGE_SENSOR_INIT,
  STAGE_SENSOR_STARTED,
  STAGE_SCANNER_INIT,
  STAGE_SCANNER_STARTED,
  STAGE_BATTERY_INIT,
  STAGE_BATTERY_STARTED,
  STAGE_TELEMETRY_PROVIDER_SET,
  STAGE_INTERNET_STARTED,
  STAGE_ALL_DONE,
} init_stage_t;

static DataManager_t *s_data = NULL;
static fm_lifecycle_state_t s_fm_state = FM_STATE_UNINITIALIZED;
static init_stage_t s_init_stage = STAGE_UNINITIALIZED;
static SemaphoreHandle_t s_fm_mutex = NULL;
static portMUX_TYPE s_fm_gate_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile int s_active_api_calls = 0;
static function_manager_ui_notify_fn s_ui_notify = NULL;

fm_lifecycle_state_t FunctionManager_GetState(void) {
  taskENTER_CRITICAL(&s_fm_gate_lock);
  fm_lifecycle_state_t st = s_fm_state;
  taskEXIT_CRITICAL(&s_fm_gate_lock);
  return st;
}

void FunctionManager_SetUiNotifier(function_manager_ui_notify_fn notify_fn) {
  s_ui_notify = notify_fn;
}

static void notify_ui(uint32_t event_bits) {
  if (s_ui_notify != NULL) {
    s_ui_notify(event_bits);
  }
}

#if CONFIG_MSMS_ENABLE_TESTS
static int s_fm_init_fault_stage = 0;
void FunctionManager_TestInjectInitFault(int stage) {
  s_fm_init_fault_stage = stage;
}
#endif

static system_err_t fm_api_enter(void) {
  taskENTER_CRITICAL(&s_fm_gate_lock);
  if (s_fm_state != FM_STATE_RUNNING || s_data == NULL) {
    system_err_t err = (s_fm_state == FM_STATE_STOPPING || s_fm_state == FM_STATE_CLEANUP_REQUIRED)
                           ? MRS_ERR_CORE_INVALID_STATE
                           : MRS_ERR_CORE_NOT_INITIALIZED;
    taskEXIT_CRITICAL(&s_fm_gate_lock);
    return err;
  }
  s_active_api_calls++;
  taskEXIT_CRITICAL(&s_fm_gate_lock);
  return MRS_OK;
}

static void fm_api_exit(void) {
  taskENTER_CRITICAL(&s_fm_gate_lock);
  if (s_active_api_calls > 0) {
    s_active_api_calls--;
  }
  taskEXIT_CRITICAL(&s_fm_gate_lock);
}

static void push_error(system_err_t error) {
  if (s_data != NULL && error != MRS_OK) {
    ErrorCodes_PushError(s_data->error_code, DATA_MANAGER_ERROR_CAPACITY,
                         error);
  }
}

static system_err_t uart_role_request(mesh_role_t role, void *context) {
  (void)context;
  return FunctionManager_RequestMeshRole(role);
}

static bool scanner_permission_policy(void) {
  return !MeshManager_IsRoot();
}

static bool function_manager_telemetry_provider(mesh_telemetry_port_entry_t *out_entries,
                                                size_t max_entries,
                                                size_t *out_count,
                                                void *context) {
  (void)context;
  if (out_entries == NULL || out_count == NULL || max_entries == 0) {
    return false;
  }
  size_t count = 0;
  for (PortId_t port = PORT_1; port < NUM_PORTS && count < max_entries; port++) {
    SensorPortSnapshot_t snapshot = {0};
    if (SensorManager_GetPortSnapshot(port, &snapshot) != MRS_OK || !snapshot.assigned) {
      continue;
    }
    sensor_descriptor_t desc = {0};
    if (!sensor_registry_get_descriptor(snapshot.type, &desc)) {
      continue;
    }
    out_entries[count].port = port;
    out_entries[count].type = snapshot.type;
    out_entries[count].valid = snapshot.data_valid;
    uint8_t fields = desc.unit_count > 5 ? 5 : desc.unit_count;
    out_entries[count].field_count = fields;
    for (uint8_t f = 0; f < fields; f++) {
      out_entries[count].values[f] = snapshot.data.data_fl[f];
    }
    count++;
  }
  *out_count = count;
  return true;
}

static void scanner_assignment_callback(void *context, PortId_t port,
                                        SensorType_t type) {
  (void)context;
  (void)port;
  (void)type;
  notify_ui(FM_EVENT_PORT_LABELS_CHANGED);
}

static system_err_t rollback_init(init_stage_t stage) {
  while (stage > STAGE_UNINITIALIZED) {
    system_err_t err = MRS_OK;
    switch (stage) {
    case STAGE_ALL_DONE:
      stage = STAGE_INTERNET_STARTED;
      continue;

    case STAGE_INTERNET_STARTED: {
      err = InternetManager_Clean(s_data, false);
      if (err != MRS_OK) {
        ESP_LOGE(TAG, "InternetManager_Clean failed: %s", system_err_to_name(err));
        return err;
      }
      s_init_stage = STAGE_TELEMETRY_PROVIDER_SET;
      stage = STAGE_TELEMETRY_PROVIDER_SET;
      break;
    }

    case STAGE_TELEMETRY_PROVIDER_SET: {
      esp_err_t es_err = MeshManager_SetTelemetryProvider(NULL, NULL);
      if (es_err != ESP_OK) {
        ESP_LOGE(TAG, "MeshManager_SetTelemetryProvider unregister failed: %s", esp_err_to_name(es_err));
        return (system_err_t)es_err;
      }
      s_init_stage = STAGE_BATTERY_STARTED;
      stage = STAGE_BATTERY_STARTED;
      break;
    }

    case STAGE_BATTERY_STARTED: {
      esp_err_t es_err = BatteryManager_StopTask(pdMS_TO_TICKS(2000));
      if (es_err != ESP_OK) {
        ESP_LOGE(TAG, "BatteryManager_StopTask failed: %s", esp_err_to_name(es_err));
        return (system_err_t)es_err;
      }
      s_init_stage = STAGE_BATTERY_INIT;
      stage = STAGE_BATTERY_INIT;
      break;
    }

    case STAGE_BATTERY_INIT: {
      esp_err_t es_err = BatteryManager_Deinit();
      if (es_err != ESP_OK) {
        ESP_LOGE(TAG, "BatteryManager_Deinit failed: %s", esp_err_to_name(es_err));
        return (system_err_t)es_err;
      }
      s_init_stage = STAGE_SCANNER_STARTED;
      stage = STAGE_SCANNER_STARTED;
      break;
    }

    case STAGE_SCANNER_STARTED: {
      esp_err_t es_err = SensorScanner_StopTask();
      if (es_err != ESP_OK) {
        ESP_LOGE(TAG, "SensorScanner_StopTask failed: %s", esp_err_to_name(es_err));
        return (system_err_t)es_err;
      }
      s_init_stage = STAGE_SCANNER_INIT;
      stage = STAGE_SCANNER_INIT;
      break;
    }

    case STAGE_SCANNER_INIT: {
      esp_err_t es_err = SensorScanner_Deinit();
      if (es_err != ESP_OK) {
        ESP_LOGE(TAG, "SensorScanner_Deinit failed: %s", esp_err_to_name(es_err));
        return (system_err_t)es_err;
      }
      s_init_stage = STAGE_SENSOR_STARTED;
      stage = STAGE_SENSOR_STARTED;
      break;
    }

    case STAGE_SENSOR_STARTED: {
      err = SensorManager_Stop();
      if (err != MRS_OK) {
        ESP_LOGE(TAG, "SensorManager_Stop failed: %s", system_err_to_name(err));
        return err;
      }
      s_init_stage = STAGE_SENSOR_INIT;
      stage = STAGE_SENSOR_INIT;
      break;
    }

    case STAGE_SENSOR_INIT: {
      err = SensorManager_Deinit();
      if (err != MRS_OK) {
        ESP_LOGE(TAG, "SensorManager_Deinit failed: %s", system_err_to_name(err));
        return err;
      }
      s_init_stage = STAGE_UART_INIT;
      stage = STAGE_UART_INIT;
      break;
    }

    case STAGE_UART_INIT: {
      err = UartToGateWay_Deinit();
      if (err != MRS_OK) {
        ESP_LOGE(TAG, "UartToGateWay_Deinit failed: %s", system_err_to_name(err));
        return err;
      }
      (void)UartResource_Deinit();
      s_init_stage = STAGE_ACTUATOR_INIT;
      stage = STAGE_ACTUATOR_INIT;
      break;
    }

    case STAGE_ACTUATOR_INIT: {
      err = ActuatorManager_Deinit();
      if (err != MRS_OK) {
        ESP_LOGE(TAG, "ActuatorManager_Deinit failed: %s", system_err_to_name(err));
        return err;
      }
      s_init_stage = STAGE_UNINITIALIZED;
      stage = STAGE_UNINITIALIZED;
      break;
    }

    case STAGE_UNINITIALIZED:
    default:
      stage = STAGE_UNINITIALIZED;
      break;
    }
  }

  s_init_stage = STAGE_UNINITIALIZED;
  return MRS_OK;
}

static system_err_t function_manager_handle_init_failure(init_stage_t stage,
                                                        system_err_t failure_err) {
  push_error(failure_err);
  system_err_t rberr = rollback_init(stage);
  if (rberr == MRS_OK) {
    taskENTER_CRITICAL(&s_fm_gate_lock);
    s_data = NULL;
    s_fm_state = FM_STATE_UNINITIALIZED;
    taskEXIT_CRITICAL(&s_fm_gate_lock);
    return failure_err;
  }

  push_error(rberr);
  ESP_LOGE(TAG, "Init failure rollback failed: %s; system partial at stage %d",
           system_err_to_name(rberr), (int)s_init_stage);
  taskENTER_CRITICAL(&s_fm_gate_lock);
  s_fm_state = FM_STATE_CLEANUP_REQUIRED;
  taskEXIT_CRITICAL(&s_fm_gate_lock);
  return rberr;
}

system_err_t FunctionManager_Init(DataManager_t *data) {
  if (data == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_fm_state == FM_STATE_CLEANUP_REQUIRED) {
    ESP_LOGE(TAG, "Init rejected: previous cleanup failed, retry Deinit first");
    return MRS_ERR_CORE_INVALID_STATE;
  }
  if (s_data != NULL) {
    if (s_data != data) {
      return MRS_ERR_CORE_ALREADY_INIT;
    }
    return (s_fm_state == FM_STATE_RUNNING) ? MRS_OK : MRS_ERR_CORE_INVALID_STATE;
  }

  if (s_fm_mutex == NULL) {
    s_fm_mutex = xSemaphoreCreateMutex();
    if (s_fm_mutex == NULL) {
      return MRS_ERR_CORE_OUT_OF_MEMORY;
    }
  }

  taskENTER_CRITICAL(&s_fm_gate_lock);
  s_fm_state = FM_STATE_INITIALIZING;
  s_data = data;
  s_active_api_calls = 0;
  taskEXIT_CRITICAL(&s_fm_gate_lock);

  // Stage 1: Actuator Manager
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 1) {
    s_fm_init_fault_stage = 0;
    return function_manager_handle_init_failure(STAGE_UNINITIALIZED, MRS_FAIL);
  }
#endif
  system_err_t result = ActuatorManager_Init();
  if (result != MRS_OK) {
    return function_manager_handle_init_failure(STAGE_UNINITIALIZED, result);
  }
  s_init_stage = STAGE_ACTUATOR_INIT;

  // Stage 2: UART Resource & Gateway
  system_err_t res_err = UartResource_Init(PIN_UART_NUM, PIN_UART_TX, PIN_UART_RX);
  if (res_err != MRS_OK) {
    return function_manager_handle_init_failure(STAGE_ACTUATOR_INIT, res_err);
  }
  UartToGateWay_SetRoleRequestHandler(uart_role_request, NULL);
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 2) {
    s_fm_init_fault_stage = 0;
    (void)UartResource_Deinit();
    return function_manager_handle_init_failure(STAGE_ACTUATOR_INIT, MRS_FAIL);
  }
#endif
  result = UartToGateWay_Init(data);
  if (result != MRS_OK) {
    (void)UartResource_Deinit();
    return function_manager_handle_init_failure(STAGE_ACTUATOR_INIT, result);
  }
  s_init_stage = STAGE_UART_INIT;

  // Stage 3: Sensor Manager Init
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 3) {
    s_fm_init_fault_stage = 0;
    return function_manager_handle_init_failure(STAGE_UART_INIT, MRS_FAIL);
  }
#endif
  result = SensorManager_Init(data);
  if (result != MRS_OK) {
    return function_manager_handle_init_failure(STAGE_UART_INIT, result);
  }
  s_init_stage = STAGE_SENSOR_INIT;

  // Stage 4: Sensor Manager Sample Task
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 4) {
    s_fm_init_fault_stage = 0;
    return function_manager_handle_init_failure(STAGE_SENSOR_INIT, MRS_FAIL);
  }
#endif
  result = SensorManager_Start();
  if (result != MRS_OK) {
    return function_manager_handle_init_failure(STAGE_SENSOR_INIT, result);
  }
  s_init_stage = STAGE_SENSOR_STARTED;

  // Stage 5: Sensor Scanner Init & Injected Policy/Callbacks
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 5) {
    s_fm_init_fault_stage = 0;
    return function_manager_handle_init_failure(STAGE_SENSOR_STARTED, MRS_FAIL);
  }
#endif
  esp_err_t scanner_result = SensorScanner_Init(data);
  if (scanner_result != ESP_OK) {
    return function_manager_handle_init_failure(STAGE_SENSOR_STARTED, (system_err_t)scanner_result);
  }
  s_init_stage = STAGE_SCANNER_INIT;
  SensorScanner_SetPermissionPolicy(scanner_permission_policy);
  SensorScanner_SetAssignmentCallback(scanner_assignment_callback, NULL);

  // Stage 6: Sensor Scanner Task
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 6) {
    s_fm_init_fault_stage = 0;
    return function_manager_handle_init_failure(STAGE_SCANNER_INIT, MRS_FAIL);
  }
#endif
  scanner_result = SensorScanner_StartTask(data);
  if (scanner_result != ESP_OK) {
    return function_manager_handle_init_failure(STAGE_SCANNER_INIT, MRS_ERR_FUNCTIONMANAGER_TASK_FAILED);
  }
  s_init_stage = STAGE_SCANNER_STARTED;

  // Stage 7: Battery Manager Init
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 7) {
    s_fm_init_fault_stage = 0;
    return function_manager_handle_init_failure(STAGE_SCANNER_STARTED, MRS_FAIL);
  }
#endif
  esp_err_t batt_ret = BatteryManager_Init();
  if (batt_ret != ESP_OK) {
    return function_manager_handle_init_failure(STAGE_SCANNER_STARTED, (system_err_t)batt_ret);
  }
  s_init_stage = STAGE_BATTERY_INIT;

  // Stage 8: Battery Manager Task
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 8) {
    s_fm_init_fault_stage = 0;
    return function_manager_handle_init_failure(STAGE_BATTERY_INIT, MRS_FAIL);
  }
#endif
  batt_ret = BatteryManager_StartTask(data);
  if (batt_ret != ESP_OK) {
    return function_manager_handle_init_failure(STAGE_BATTERY_INIT, (system_err_t)batt_ret);
  }
  s_init_stage = STAGE_BATTERY_STARTED;

  // Stage 9: Telemetry Provider for MeshManager
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 9) {
    s_fm_init_fault_stage = 0;
    return function_manager_handle_init_failure(STAGE_BATTERY_STARTED, MRS_FAIL);
  }
#endif
  esp_err_t mesh_p_ret = MeshManager_SetTelemetryProvider(function_manager_telemetry_provider, NULL);
  if (mesh_p_ret != ESP_OK) {
    return function_manager_handle_init_failure(STAGE_BATTERY_STARTED, (system_err_t)mesh_p_ret);
  }
  s_init_stage = STAGE_TELEMETRY_PROVIDER_SET;

  // Stage 10: Internet Manager (Boot default: MESH mode)
#if CONFIG_MSMS_ENABLE_TESTS
  if (s_fm_init_fault_stage == 10) {
    s_fm_init_fault_stage = 0;
    return function_manager_handle_init_failure(STAGE_TELEMETRY_PROVIDER_SET, MRS_FAIL);
  }
#endif
  result = InternetManager_Init(data, INTERNET_MODE_MESH);
  if (result != MRS_OK) {
    return function_manager_handle_init_failure(STAGE_TELEMETRY_PROVIDER_SET, result);
  }
  s_init_stage = STAGE_ALL_DONE;

  taskENTER_CRITICAL(&s_fm_gate_lock);
  s_fm_state = FM_STATE_RUNNING;
  taskEXIT_CRITICAL(&s_fm_gate_lock);
  return MRS_OK;
}

system_err_t FunctionManager_Deinit(void) {
  if (s_fm_state == FM_STATE_UNINITIALIZED && s_data == NULL && s_init_stage == STAGE_UNINITIALIZED) {
    return MRS_OK;
  }

  if (s_fm_mutex != NULL) {
    if (xSemaphoreTake(s_fm_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
      ESP_LOGE(TAG, "FunctionManager_Deinit: timeout taking mutex");
      return MRS_ERR_CORE_TIMEOUT;
    }
  }

  taskENTER_CRITICAL(&s_fm_gate_lock);
  if (s_fm_state == FM_STATE_STOPPING) {
    taskEXIT_CRITICAL(&s_fm_gate_lock);
    if (s_fm_mutex != NULL) {
      xSemaphoreGive(s_fm_mutex);
    }
    return MRS_ERR_CORE_INVALID_STATE;
  }
  s_fm_state = FM_STATE_STOPPING;
  taskEXIT_CRITICAL(&s_fm_gate_lock);

  /* Wait for active API callers to drain */
  int wait_calls = 0;
  while (s_active_api_calls > 0 && wait_calls < 40) {
    vTaskDelay(pdMS_TO_TICKS(50));
    wait_calls++;
  }
  if (s_active_api_calls > 0) {
    ESP_LOGE(TAG, "FunctionManager_Deinit: %d active API calls still running after timeout", s_active_api_calls);
    taskENTER_CRITICAL(&s_fm_gate_lock);
    s_fm_state = FM_STATE_RUNNING;
    taskEXIT_CRITICAL(&s_fm_gate_lock);
    if (s_fm_mutex != NULL) {
      xSemaphoreGive(s_fm_mutex);
    }
    return MRS_ERR_CORE_TIMEOUT;
  }

  system_err_t err = rollback_init(s_init_stage);
  if (err != MRS_OK) {
    ESP_LOGE(TAG, "FunctionManager_Deinit: rollback failed at stage %d: %s; entering CLEANUP_REQUIRED for retry",
             (int)s_init_stage, system_err_to_name(err));
    push_error(err);
    taskENTER_CRITICAL(&s_fm_gate_lock);
    s_fm_state = FM_STATE_CLEANUP_REQUIRED;
    taskEXIT_CRITICAL(&s_fm_gate_lock);
    if (s_fm_mutex != NULL) {
      xSemaphoreGive(s_fm_mutex);
    }
    return err;
  }

  taskENTER_CRITICAL(&s_fm_gate_lock);
  s_fm_state = FM_STATE_UNINITIALIZED;
  s_data = NULL;
  taskEXIT_CRITICAL(&s_fm_gate_lock);

  if (s_fm_mutex != NULL) {
    xSemaphoreGive(s_fm_mutex);
  }
  return MRS_OK;
}

void FunctionManager_SetSensorBusGuard(
    function_manager_bus_lock_fn lock_fn,
    function_manager_bus_unlock_fn unlock_fn, void *context) {
  SensorManager_SetBusGuard(lock_fn, unlock_fn, context);
}

system_err_t FunctionManager_AssignSensor(PortId_t port,
                                          SensorType_t sensor_type) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = SensorManager_AssignSensor(port, sensor_type);
  if (result != MRS_OK) {
    push_error(result);
    fm_api_exit();
    return result;
  }
  if (s_data->on_sensor_selected != NULL) {
    s_data->on_sensor_selected(s_data, port);
  }
  fm_api_exit();
  return MRS_OK;
}

system_err_t FunctionManager_ResetAllSensorPorts(void) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = SensorManager_ResetAll();
  if (result != MRS_OK) {
    push_error(result);
    fm_api_exit();
    return result;
  }
  notify_ui(FM_EVENT_PORT_LABELS_CHANGED);
  fm_api_exit();
  return MRS_OK;
}

system_err_t FunctionManager_GetPortSnapshot(PortId_t port,
                                             SensorPortSnapshot_t *out_snapshot) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = SensorManager_GetPortSnapshot(port, out_snapshot);
  fm_api_exit();
  return result;
}

system_err_t FunctionManager_GetAllPortSnapshots(
    SensorPortSnapshot_t *out_snapshots, size_t max_snapshots,
    size_t *out_count) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = SensorManager_GetAllPortSnapshots(out_snapshots, max_snapshots,
                                                        out_count);
  fm_api_exit();
  return result;
}

system_err_t FunctionManager_GetAssignedSensor(PortId_t port,
                                               SensorType_t *out_sensor_type) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = SensorManager_GetAssignedSensor(port, out_sensor_type);
  fm_api_exit();
  return result;
}

system_err_t FunctionManager_GetSensorSnapshot(PortId_t port,
                                               SensorType_t *out_sensor_type,
                                               SensorData_t *out_data) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = SensorManager_GetSnapshot(port, out_sensor_type, out_data);
  fm_api_exit();
  return result;
}

system_err_t FunctionManager_GetSensorDescriptor(
    SensorType_t sensor_type, sensor_descriptor_t *out_descriptor) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  if (out_descriptor == NULL) {
    fm_api_exit();
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  system_err_t result = sensor_registry_get_descriptor(sensor_type, out_descriptor)
                            ? MRS_OK
                            : MRS_ERR_SENSORS_NOT_FOUND;
  fm_api_exit();
  return result;
}

const char *FunctionManager_GetSensorName(SensorType_t sensor_type) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return NULL;
  const char *res = sensor_type_to_name(sensor_type);
  fm_api_exit();
  return res;
}

size_t FunctionManager_GetSensorCountByInterface(
    TypeCommunication_t interface) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return 0;
  size_t res = sensor_registry_get_count_by_interface(interface);
  fm_api_exit();
  return res;
}

system_err_t FunctionManager_GetSensorDescriptorAtInterface(
    TypeCommunication_t interface, size_t index,
    sensor_descriptor_t *out_descriptor) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  if (out_descriptor == NULL) {
    fm_api_exit();
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  system_err_t result = sensor_registry_get_descriptor_at_interface(interface, index,
                                                                   out_descriptor)
                            ? MRS_OK
                            : MRS_ERR_SENSORS_NOT_FOUND;
  fm_api_exit();
  return result;
}

system_err_t FunctionManager_ScanAndAssignSensors(
    function_sensor_scan_result_t *out_result) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  if (out_result == NULL) {
    fm_api_exit();
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  out_result->detected_count = 0;
  out_result->assigned_count = 0;
  if (!SensorScanner_IsAllowed()) {
    fm_api_exit();
    return MRS_ERR_CORE_INVALID_STATE;
  }

  int detected = SensorScanner_Scan(s_data, out_result->detected, NUM_PORTS);
  if (detected < 0) {
    fm_api_exit();
    return MRS_FAIL;
  }
  out_result->detected_count = (size_t)detected;
  out_result->assigned_count = (size_t)SensorScanner_AutoAssign(
      s_data, out_result->detected, out_result->detected_count);
  fm_api_exit();
  return MRS_OK;
}

bool FunctionManager_IsSensorScanAllowed(void) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return false;
  bool res = SensorScanner_IsAllowed();
  fm_api_exit();
  return res;
}

void FunctionManager_SetAutoScanEnabled(bool enable) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return;
  SensorScanner_SetAutoScanEnabled(enable);
  fm_api_exit();
}

bool FunctionManager_IsAutoScanEnabled(void) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return false;
  bool res = SensorScanner_IsAutoScanEnabled();
  fm_api_exit();
  return res;
}

void FunctionManager_TriggerSensorScan(void) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return;
  SensorScanner_TriggerScan();
  fm_api_exit();
}

system_err_t FunctionManager_RequestWifiConfiguration(void) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = InternetManager_RequestWifiConfiguration(s_data);
  fm_api_exit();
  return result;
}

system_err_t FunctionManager_RequestMeshRole(mesh_role_t role) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = InternetManager_RequestMeshRole(s_data, role);
  fm_api_exit();
  return result;
}

system_err_t FunctionManager_GetNetworkStatus(InternetManagerStatus_t *out) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = InternetManager_GetStatus(s_data, out);
  fm_api_exit();
  return result;
}

system_err_t FunctionManager_SetActuator(int gpio_num, bool enabled) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  system_err_t result = ActuatorManager_SetLevel(gpio_num, enabled);
  if (result != MRS_OK) {
    push_error(result);
  }
  fm_api_exit();
  return result;
}

system_err_t FunctionManager_RefreshBatteryInfo(void) {
  system_err_t gate = fm_api_enter();
  if (gate != MRS_OK) return gate;

  BatteryManager_UpdateInfo(&s_data->objectInfo.batteryInfo);
  fm_api_exit();
  return MRS_OK;
}
