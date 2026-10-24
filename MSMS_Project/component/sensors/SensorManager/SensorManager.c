#include "SensorManager.h"
#include "UartResourceManager.h"
#include "PinManager.h"

#include "SensorRegistry.h"
#include "SensorRegistry_internal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

#define SENSOR_SAMPLE_INTERVAL_MS 1000
#define SENSOR_BUS_LOCK_TIMEOUT_MS 1000

static const char *TAG = "SensorManager";

static DataManager_t *s_data;
static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_sample_task;
static volatile bool s_stop_requested = false;
static volatile sensor_manager_state_t s_sm_state = SENSOR_MANAGER_STATE_UNINITIALIZED;
static SemaphoreHandle_t s_sample_task_stopped_sem = NULL;

typedef enum {
  SENSOR_TASK_STATE_STOPPED = 0,
  SENSOR_TASK_STATE_STARTING,
  SENSOR_TASK_STATE_RUNNING,
  SENSOR_TASK_STATE_STOPPING,
} sensor_task_state_t;

static volatile sensor_task_state_t s_sensor_task_state = SENSOR_TASK_STATE_STOPPED;
static uart_lease_t s_sensor_active_uart_lease = {0};

static system_err_t acquire_sensor_uart_lease_locked(uart_owner_t owner, uart_lease_t *out_lease) {
  uart_resource_config_t uart_cfg = {
      .uart_config = {
          .baud_rate = PIN_UART_PERIPHERAL_BAUD_RATE,
          .data_bits = UART_DATA_8_BITS,
          .parity = UART_PARITY_DISABLE,
          .stop_bits = UART_STOP_BITS_1,
          .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
          .source_clk = UART_SCLK_APB,
      },
      .rx_buffer_size = 256,
      .tx_buffer_size = 0,
      .queue_size = 0,
      .out_queue = NULL,
      .intr_alloc_flags = 0,
  };
  return UartResource_Acquire(owner, &uart_cfg, pdMS_TO_TICKS(1500), out_lease);
}

static void release_sensor_uart_lease_locked(uart_lease_t *lease) {
  if (lease != NULL && UartResource_IsLeaseValid(lease)) {
    (void)UartResource_Release(lease);
  }
}

sensor_manager_state_t SensorManager_GetState(void) {
  return s_sm_state;
}

#if CONFIG_MSMS_ENABLE_TESTS
static int s_sensor_restore_fault = 0;
static int s_uart_resume_fault = 0;
void SensorManager_TestInjectRestoreFault(int enable) {
  s_sensor_restore_fault = enable;
}
void SensorManager_TestInjectUartResumeFault(int enable) {
  s_uart_resume_fault = enable;
}
#endif
static bool s_snapshot_valid[NUM_PORTS];
static sensor_manager_bus_lock_fn s_bus_lock;
static sensor_manager_bus_unlock_fn s_bus_unlock;
static void *s_bus_context;

static bool valid_port(PortId_t port) {
  return port >= PORT_1 && port < NUM_PORTS;
}

static bool take_manager_lock(TickType_t timeout) {
  return s_mutex != NULL && xSemaphoreTake(s_mutex, timeout) == pdTRUE;
}

static bool take_bus_lock(const sensor_driver_t *driver) {
  if (driver == NULL || driver->interface != COMMUNICATION_I2C ||
      s_bus_lock == NULL) {
    return true;
  }
  return s_bus_lock(SENSOR_BUS_LOCK_TIMEOUT_MS, s_bus_context);
}

static void give_bus_lock(const sensor_driver_t *driver, bool locked) {
  if (locked && driver != NULL &&
      driver->interface == COMMUNICATION_I2C && s_bus_unlock != NULL) {
    s_bus_unlock(s_bus_context);
  }
}

static bool type_assigned_locked(SensorType_t sensor_type,
                                 PortId_t ignored_port) {
  if (s_data == NULL) {
    return false;
  }
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    if (port != ignored_port && s_data->selectedSensor[port] == sensor_type) {
      return true;
    }
  }
  return false;
}

static bool any_uart_sensor_assigned_locked(void) {
  if (s_data == NULL) {
    return false;
  }
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    sensor_driver_t *driver =
        sensor_registry_get_driver_mutable(s_data->selectedSensor[port]);
    if (driver != NULL && driver->interface == COMMUNICATION_UART) {
      return true;
    }
  }
  return false;
}

static bool interface_assigned_locked(TypeCommunication_t interface,
                                      PortId_t ignored_port) {
  if (s_data == NULL) {
    return false;
  }
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    if (port == ignored_port) {
      continue;
    }
    sensor_driver_t *driver =
        sensor_registry_get_driver_mutable(s_data->selectedSensor[port]);
    if (driver != NULL && driver->interface == interface) {
      return true;
    }
  }
  return false;
}

static system_err_t initialize_driver_locked(sensor_driver_t *driver) {
  if (driver == NULL || driver->init == NULL) {
    return MRS_ERR_SENSORS_INVALID_TYPE;
  }
  if (driver->is_init) {
    return MRS_OK;
  }

  bool bus_locked = take_bus_lock(driver);
  if (!bus_locked) {
    return MRS_ERR_I2CDEV_BUSY;
  }

  system_err_t result = driver->init();
  give_bus_lock(driver, bus_locked);
  if (result != MRS_OK) {
    return result;
  }

  driver->is_init = true;
  return MRS_OK;
}

static system_err_t read_driver_locked(sensor_driver_t *driver,
                                       SensorData_t *out_data) {
  if (driver == NULL || out_data == NULL || driver->read == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (!driver->is_init) {
    return MRS_ERR_SENSORS_NOT_INITIALIZED;
  }

  bool bus_locked = take_bus_lock(driver);
  if (!bus_locked) {
    return MRS_ERR_I2CDEV_BUSY;
  }
  system_err_t result = driver->read(out_data);
  give_bus_lock(driver, bus_locked);
  return result;
}

static system_err_t deinitialize_driver_locked(sensor_driver_t *driver) {
  if (driver == NULL || !driver->is_init) {
    return MRS_OK;
  }

  bool bus_locked = take_bus_lock(driver);
  if (!bus_locked) {
    return MRS_ERR_I2CDEV_BUSY;
  }

  system_err_t err = MRS_OK;
  if (driver->deinit != NULL) {
    err = driver->deinit();
  }
  give_bus_lock(driver, bus_locked);

  if (err == MRS_OK) {
    driver->is_init = false;
  }
  return err;
}

static void sensor_sample_task(void *parameter) {
  (void)parameter;

  while (!s_stop_requested) {
    for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
      if (s_stop_requested) {
        break;
      }
      if (!take_manager_lock(pdMS_TO_TICKS(1000))) {
        continue;
      }
      SensorType_t type = s_data->selectedSensor[port];
      if (type == SENSOR_NONE) {
        s_snapshot_valid[port] = false;
        xSemaphoreGive(s_mutex);
        continue;
      }

      sensor_driver_t *driver =
          sensor_registry_get_driver_mutable(type);
      SensorData_t sample = {0};
      system_err_t result = read_driver_locked(driver, &sample);
      if (result == MRS_OK) {
        s_data->port_data[port] = sample;
        s_snapshot_valid[port] = true;
      } else {
        ErrorCodes_PushError(s_data->error_code,
                             DATA_MANAGER_ERROR_CAPACITY, result);
        ESP_LOGW(TAG, "Read failed on port %d (%s): %s", (int)port + 1,
                 sensor_type_to_name(type), system_err_to_name(result));
      }
      xSemaphoreGive(s_mutex);
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    for (int ms = 0; ms < SENSOR_SAMPLE_INTERVAL_MS && !s_stop_requested; ms += 50) {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }

  s_sensor_task_state = SENSOR_TASK_STATE_STOPPING;
  SemaphoreHandle_t sem = s_sample_task_stopped_sem;
  if (sem != NULL) {
    xSemaphoreGive(sem);
  }
  vTaskDelete(NULL);
}

static system_err_t stop_sample_task_internal(TickType_t timeout) {
  if (s_sensor_task_state == SENSOR_TASK_STATE_STOPPED && s_sample_task == NULL) {
    return MRS_OK;
  }
  s_stop_requested = true;
  if (s_sample_task_stopped_sem == NULL ||
      xSemaphoreTake(s_sample_task_stopped_sem, timeout) != pdTRUE) {
    ESP_LOGE(TAG, "Timeout waiting for sensor_sample_task to exit");
    return MRS_ERR_CORE_TIMEOUT;
  }
  s_stop_requested = false;
  s_sample_task = NULL;
  s_sensor_task_state = SENSOR_TASK_STATE_STOPPED;
  return MRS_OK;
}

system_err_t SensorManager_Init(DataManager_t *data) {
  if (data == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_data != NULL) {
    return s_data == data ? MRS_OK : MRS_ERR_CORE_ALREADY_INIT;
  }

  s_mutex = xSemaphoreCreateMutex();
  if (s_mutex == NULL) {
    return MRS_ERR_CORE_OUT_OF_MEMORY;
  }

  s_sample_task_stopped_sem = xSemaphoreCreateBinary();
  if (s_sample_task_stopped_sem == NULL) {
    vSemaphoreDelete(s_mutex);
    s_mutex = NULL;
    return MRS_ERR_CORE_OUT_OF_MEMORY;
  }

  s_stop_requested = false;
  s_data = data;
  s_sm_state = SENSOR_MANAGER_STATE_INITIALIZED;
  memset(s_snapshot_valid, 0, sizeof(s_snapshot_valid));
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    s_data->selectedSensor[port] = SENSOR_NONE;
    s_data->objectInfo.selectedSensorName[port] =
        sensor_type_to_name(SENSOR_NONE);
  }
  return MRS_OK;
}

system_err_t SensorManager_Start(void) {
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (s_sm_state == SENSOR_MANAGER_STATE_RUNNING && s_sample_task != NULL) {
    return MRS_OK;
  }
  s_stop_requested = false;
  if (s_sample_task_stopped_sem != NULL) {
    (void)xSemaphoreTake(s_sample_task_stopped_sem, 0);
  }
  s_sensor_task_state = SENSOR_TASK_STATE_STARTING;
  if (xTaskCreate(sensor_sample_task, "sensor_sample", 4096, NULL, 5,
                  &s_sample_task) != pdPASS) {
    s_sample_task = NULL;
    s_sensor_task_state = SENSOR_TASK_STATE_STOPPED;
    return MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
  }
  s_sensor_task_state = SENSOR_TASK_STATE_RUNNING;
  s_sm_state = SENSOR_MANAGER_STATE_RUNNING;
  return MRS_OK;
}

static system_err_t reset_all_internal(void);

system_err_t SensorManager_Stop(void) {
  if (s_sm_state == SENSOR_MANAGER_STATE_UNINITIALIZED ||
      s_sm_state == SENSOR_MANAGER_STATE_INITIALIZED) {
    return MRS_OK;
  }
  if (s_sm_state == SENSOR_MANAGER_STATE_STOPPING) {
    return MRS_ERR_CORE_INVALID_STATE;
  }
  s_sm_state = SENSOR_MANAGER_STATE_STOPPING;
  system_err_t err = stop_sample_task_internal(pdMS_TO_TICKS(3000));
  if (err != MRS_OK) {
    s_sm_state = SENSOR_MANAGER_STATE_RUNNING;
    return err;
  }
  s_sm_state = SENSOR_MANAGER_STATE_INITIALIZED;
  return MRS_OK;
}

system_err_t SensorManager_Deinit(void) {
  if (s_sm_state == SENSOR_MANAGER_STATE_UNINITIALIZED && s_data == NULL && s_mutex == NULL) {
    return MRS_OK;
  }
  if (s_sm_state == SENSOR_MANAGER_STATE_STOPPING) {
    return MRS_ERR_CORE_INVALID_STATE;
  }

  s_sm_state = SENSOR_MANAGER_STATE_STOPPING;
  system_err_t stop_err = stop_sample_task_internal(pdMS_TO_TICKS(3000));
  if (stop_err != MRS_OK) {
    ESP_LOGE(TAG, "SensorManager_Deinit: Stop failed: %s; preserving resources for retry", system_err_to_name(stop_err));
    s_sm_state = SENSOR_MANAGER_STATE_RUNNING;
    return stop_err;
  }

  system_err_t reset_err = reset_all_internal();
  if (reset_err != MRS_OK) {
    ESP_LOGE(TAG, "SensorManager_Deinit: ResetAll failed: %s; preserving resources for retry", system_err_to_name(reset_err));
    s_sm_state = SENSOR_MANAGER_STATE_INITIALIZED;
    return reset_err;
  }

  if (s_sample_task_stopped_sem != NULL) {
    SemaphoreHandle_t sem = s_sample_task_stopped_sem;
    s_sample_task_stopped_sem = NULL;
    vSemaphoreDelete(sem);
  }
  if (s_mutex != NULL) {
    SemaphoreHandle_t mutex = s_mutex;
    s_mutex = NULL;
    vSemaphoreDelete(mutex);
  }
  s_data = NULL;
  s_sm_state = SENSOR_MANAGER_STATE_UNINITIALIZED;
  return MRS_OK;
}

void SensorManager_SetBusGuard(sensor_manager_bus_lock_fn lock_fn,
                               sensor_manager_bus_unlock_fn unlock_fn,
                               void *context) {
  s_bus_lock = lock_fn;
  s_bus_unlock = unlock_fn;
  s_bus_context = context;
}

static system_err_t assign_sensor_locked(PortId_t port,
                                         SensorType_t sensor_type,
                                         sensor_driver_t *driver) {
  SensorType_t previous_type = s_data->selectedSensor[port];
  if (previous_type == sensor_type) {
    return MRS_OK;
  }

  // Phase 1: Validation
  if (type_assigned_locked(sensor_type, port)) {
    return MRS_ERR_DATAMANAGER_PORT_IN_USE;
  }

  if ((driver->interface == COMMUNICATION_UART ||
       driver->interface == COMMUNICATION_PULSE) &&
      interface_assigned_locked(driver->interface, port)) {
    return MRS_ERR_DATAMANAGER_PORT_IN_USE;
  }

  // Phase 2: Prepare UART lease if new sensor is UART
  uart_lease_t new_uart_lease = {0};
  if (driver->interface == COMMUNICATION_UART) {
    system_err_t uerr = acquire_sensor_uart_lease_locked(UART_OWNER_SENSOR_ACTIVE, &new_uart_lease);
    if (uerr != MRS_OK) {
      ESP_LOGE(TAG, "assign_sensor: Failed to acquire UART lease for %s: %s",
               driver->name, system_err_to_name(uerr));
      return uerr;
    }
  }

  sensor_driver_t *previous =
      sensor_registry_get_driver_mutable(previous_type);
  bool previous_deactivated = false;

  // Phase 3: Deactivate old sensor
  if (previous != NULL && previous_type != SENSOR_NONE) {
    if (!type_assigned_locked(previous_type, port)) {
      system_err_t deact_err = deinitialize_driver_locked(previous);
      if (deact_err != MRS_OK) {
        ESP_LOGE(TAG, "Deactivate previous sensor %s failed: %s; aborting assignment",
                 sensor_type_to_name(previous_type),
                 system_err_to_name(deact_err));
        if (driver->interface == COMMUNICATION_UART) {
          release_sensor_uart_lease_locked(&new_uart_lease);
        }
        return deact_err;
      }
      previous_deactivated = true;
    }
  }

  // Phase 4: Activate new driver
  system_err_t result = initialize_driver_locked(driver);

  // Phase 5: UART ownership transition & Rollback
  if (result == MRS_OK) {
    // If old was UART and new is not UART, release the old active lease (restores gateway)
    if (previous != NULL && previous->interface == COMMUNICATION_UART &&
        driver->interface != COMMUNICATION_UART &&
        !any_uart_sensor_assigned_locked()) {
      release_sensor_uart_lease_locked(&s_sensor_active_uart_lease);
    }
    // If new is UART, take ownership of lease
    if (driver->interface == COMMUNICATION_UART) {
      s_sensor_active_uart_lease = new_uart_lease;
    }

    // Phase 6: Commit DataManager
    s_data->selectedSensor[port] = sensor_type;
    s_data->objectInfo.selectedSensorName[port] = driver->name;
    s_snapshot_valid[port] = false;
  } else {
    // Activation failed: Rollback
    (void)deinitialize_driver_locked(driver);
    if (driver->interface == COMMUNICATION_UART) {
      release_sensor_uart_lease_locked(&new_uart_lease);
    }

    if (previous_deactivated && previous != NULL) {
      if (previous->interface == COMMUNICATION_UART) {
        (void)acquire_sensor_uart_lease_locked(UART_OWNER_SENSOR_ACTIVE, &s_sensor_active_uart_lease);
      }
      system_err_t restore_result = initialize_driver_locked(previous);
#if CONFIG_MSMS_ENABLE_TESTS
      if (s_sensor_restore_fault) {
        restore_result = MRS_FAIL;
      }
#endif
      if (restore_result != MRS_OK) {
        ESP_LOGE(TAG, "Failed to restore %s on port %d: %s. Rolling back to SENSOR_NONE.",
                 sensor_type_to_name(previous_type), (int)port + 1,
                 system_err_to_name(restore_result));
        s_data->selectedSensor[port] = SENSOR_NONE;
        s_data->objectInfo.selectedSensorName[port] = sensor_type_to_name(SENSOR_NONE);
        memset(&s_data->port_data[port], 0, sizeof(s_data->port_data[port]));
        s_snapshot_valid[port] = false;
        if (previous->interface == COMMUNICATION_UART) {
          release_sensor_uart_lease_locked(&s_sensor_active_uart_lease);
        }
      }
    }
  }

  return result;
}

system_err_t SensorManager_AssignSensor(PortId_t port,
                                        SensorType_t sensor_type) {
  if (s_sm_state == SENSOR_MANAGER_STATE_STOPPING) return MRS_ERR_CORE_INVALID_STATE;
  if (s_sm_state == SENSOR_MANAGER_STATE_UNINITIALIZED) return MRS_ERR_CORE_NOT_INITIALIZED;
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (!valid_port(port)) {
    return MRS_ERR_DATAMANAGER_INVALID_PORT;
  }
  sensor_driver_t *driver = sensor_registry_get_driver_mutable(sensor_type);
  if (driver == NULL) {
    return MRS_ERR_SENSORS_NOT_FOUND;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(2000))) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  system_err_t result = assign_sensor_locked(port, sensor_type, driver);
  xSemaphoreGive(s_mutex);
  if (result != MRS_OK) {
    ErrorCodes_PushError(s_data->error_code, DATA_MANAGER_ERROR_CAPACITY,
                         result);
  }
  return result;
}

system_err_t SensorManager_AssignFirstAvailable(SensorType_t sensor_type,
                                                PortId_t *out_port) {
  if (s_sm_state == SENSOR_MANAGER_STATE_STOPPING) return MRS_ERR_CORE_INVALID_STATE;
  if (s_sm_state == SENSOR_MANAGER_STATE_UNINITIALIZED) return MRS_ERR_CORE_NOT_INITIALIZED;
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (out_port == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  sensor_driver_t *driver = sensor_registry_get_driver_mutable(sensor_type);
  if (driver == NULL) {
    return MRS_ERR_SENSORS_NOT_FOUND;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(2000))) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  PortId_t selected_port = PORT_NONE;
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    if (s_data->selectedSensor[port] == SENSOR_NONE) {
      selected_port = port;
      break;
    }
  }

  system_err_t result = MRS_ERR_DATAMANAGER_PORT_IN_USE;
  if (selected_port != PORT_NONE) {
    result = assign_sensor_locked(selected_port, sensor_type, driver);
    if (result == MRS_OK) {
      *out_port = selected_port;
    }
  }

  xSemaphoreGive(s_mutex);
  if (result != MRS_OK) {
    ErrorCodes_PushError(s_data->error_code, DATA_MANAGER_ERROR_CAPACITY,
                         result);
  }
  return result;
}

static system_err_t reset_all_internal(void) {
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(2000))) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  system_err_t first_deinit_err = MRS_OK;
  size_t count = sensor_registry_get_count();
  for (size_t i = 0; i < count; i++) {
    SensorType_t type;
    sensor_driver_t *driver = sensor_registry_get_driver_mutable_at(i, &type);
    (void)type;
    system_err_t err = deinitialize_driver_locked(driver);
    if (err != MRS_OK && first_deinit_err == MRS_OK) {
      first_deinit_err = err;
    }
  }

  // Only clear ports whose driver was successfully deinitialized
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    SensorType_t ptype = s_data->selectedSensor[port];
    if (ptype == SENSOR_NONE) {
      continue;
    }
    sensor_driver_t *driver = sensor_registry_get_driver_mutable(ptype);
    if (driver == NULL || !driver->is_init) {
      s_data->selectedSensor[port] = SENSOR_NONE;
      s_data->objectInfo.selectedSensorName[port] = sensor_type_to_name(SENSOR_NONE);
      memset(&s_data->port_data[port], 0, sizeof(s_data->port_data[port]));
      s_snapshot_valid[port] = false;
    } else {
      ESP_LOGW(TAG, "ResetAll: Driver for port %d (%s) failed to deinit; preserving metadata",
               (int)port + 1, sensor_type_to_name(ptype));
    }
  }

  if (!any_uart_sensor_assigned_locked()) {
    release_sensor_uart_lease_locked(&s_sensor_active_uart_lease);
  }

  xSemaphoreGive(s_mutex);
  return first_deinit_err;
}

system_err_t SensorManager_ResetAll(void) {
  if (s_sm_state == SENSOR_MANAGER_STATE_STOPPING) return MRS_ERR_CORE_INVALID_STATE;
  if (s_sm_state == SENSOR_MANAGER_STATE_UNINITIALIZED) return MRS_ERR_CORE_NOT_INITIALIZED;
  return reset_all_internal();
}

system_err_t SensorManager_GetPortSnapshot(PortId_t port,
                                           SensorPortSnapshot_t *out_snapshot) {
  if (s_sm_state == SENSOR_MANAGER_STATE_STOPPING) return MRS_ERR_CORE_INVALID_STATE;
  if (s_sm_state == SENSOR_MANAGER_STATE_UNINITIALIZED) return MRS_ERR_CORE_NOT_INITIALIZED;
  if (!valid_port(port) || out_snapshot == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(1500))) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  SensorType_t type = s_data->selectedSensor[port];
  out_snapshot->port = port;
  out_snapshot->type = type;
  out_snapshot->assigned = (type != SENSOR_NONE);
  out_snapshot->data_valid = s_snapshot_valid[port];
  if (out_snapshot->assigned && out_snapshot->data_valid) {
    out_snapshot->data = s_data->port_data[port];
  } else {
    memset(&out_snapshot->data, 0, sizeof(out_snapshot->data));
  }

  xSemaphoreGive(s_mutex);
  return MRS_OK;
}

system_err_t SensorManager_GetAllPortSnapshots(
    SensorPortSnapshot_t *out_snapshots, size_t max_snapshots,
    size_t *out_count) {
  if (s_sm_state == SENSOR_MANAGER_STATE_STOPPING) return MRS_ERR_CORE_INVALID_STATE;
  if (s_sm_state == SENSOR_MANAGER_STATE_UNINITIALIZED) return MRS_ERR_CORE_NOT_INITIALIZED;
  if (out_snapshots == NULL || max_snapshots == 0) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(1500))) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  size_t count = max_snapshots < NUM_PORTS ? max_snapshots : NUM_PORTS;
  for (size_t i = 0; i < count; i++) {
    PortId_t port = (PortId_t)i;
    SensorType_t type = s_data->selectedSensor[port];
    out_snapshots[i].port = port;
    out_snapshots[i].type = type;
    out_snapshots[i].assigned = (type != SENSOR_NONE);
    out_snapshots[i].data_valid = s_snapshot_valid[port];
    if (out_snapshots[i].assigned && out_snapshots[i].data_valid) {
      out_snapshots[i].data = s_data->port_data[port];
    } else {
      memset(&out_snapshots[i].data, 0, sizeof(out_snapshots[i].data));
    }
  }
  if (out_count != NULL) {
    *out_count = count;
  }

  xSemaphoreGive(s_mutex);
  return MRS_OK;
}

system_err_t SensorManager_GetAssignedSensor(PortId_t port,
                                             SensorType_t *out_sensor_type) {
  if (out_sensor_type == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (!valid_port(port)) {
    return MRS_ERR_DATAMANAGER_INVALID_PORT;
  }
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(500))) {
    return MRS_ERR_CORE_TIMEOUT;
  }
  *out_sensor_type = s_data->selectedSensor[port];
  xSemaphoreGive(s_mutex);
  return MRS_OK;
}

system_err_t SensorManager_GetSnapshot(PortId_t port,
                                       SensorType_t *out_sensor_type,
                                       SensorData_t *out_data) {
  if (!valid_port(port) || out_sensor_type == NULL || out_data == NULL) {
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(1500))) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  SensorType_t type = s_data->selectedSensor[port];
  *out_sensor_type = type;
  system_err_t result = MRS_OK;
  if (type == SENSOR_NONE) {
    result = MRS_ERR_DATAMANAGER_NO_SENSOR;
  } else if (!s_snapshot_valid[port]) {
    result = MRS_ERR_SENSORS_INVALID_DATA;
  } else {
    *out_sensor_type = type;
    *out_data = s_data->port_data[port];
  }

  xSemaphoreGive(s_mutex);
  return result;
}

system_err_t SensorManager_ProbeSensor(SensorType_t sensor_type,
                                       SensorData_t *out_sample) {
  if (s_sm_state == SENSOR_MANAGER_STATE_STOPPING) return MRS_ERR_CORE_INVALID_STATE;
  if (s_sm_state == SENSOR_MANAGER_STATE_UNINITIALIZED) return MRS_ERR_CORE_NOT_INITIALIZED;
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  sensor_driver_t *driver = sensor_registry_get_driver_mutable(sensor_type);
  if (driver == NULL) {
    return MRS_ERR_SENSORS_NOT_FOUND;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(3000))) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  esp_log_level_t prev_dht = esp_log_level_get("dht");
  esp_log_level_t prev_i2cdev = esp_log_level_get("i2cdev");
  esp_log_level_t prev_bmp = esp_log_level_get("bmp280");
  esp_log_level_t prev_mhz = esp_log_level_get("MHZ14a");
  esp_log_level_t prev_pms = esp_log_level_get("PMS7003");
  esp_log_level_set("dht", ESP_LOG_NONE);
  esp_log_level_set("i2cdev", ESP_LOG_NONE);
  esp_log_level_set("bmp280", ESP_LOG_NONE);
  esp_log_level_set("MHZ14a", ESP_LOG_NONE);
  esp_log_level_set("PMS7003", ESP_LOG_NONE);

  uart_lease_t probe_lease = {0};
  if (driver->interface == COMMUNICATION_UART) {
    system_err_t acq_err = acquire_sensor_uart_lease_locked(UART_OWNER_SENSOR_SCANNER, &probe_lease);
    if (acq_err != MRS_OK) {
      esp_log_level_set("dht", prev_dht);
      esp_log_level_set("i2cdev", prev_i2cdev);
      esp_log_level_set("bmp280", prev_bmp);
      esp_log_level_set("MHZ14a", prev_mhz);
      esp_log_level_set("PMS7003", prev_pms);
      xSemaphoreGive(s_mutex);
      return acq_err;
    }
  }

  bool was_initialized = driver->is_init;
  system_err_t result = initialize_driver_locked(driver);
  SensorData_t sample = {0};
  if (result == MRS_OK) {
    result = read_driver_locked(driver, &sample);
  }

  if (result != MRS_OK && !was_initialized) {
    deinitialize_driver_locked(driver);
  } else if (result == MRS_OK && out_sample != NULL) {
    *out_sample = sample;
  }

  if (driver->interface == COMMUNICATION_UART) {
    release_sensor_uart_lease_locked(&probe_lease);
  }

  esp_log_level_set("dht", prev_dht);
  esp_log_level_set("i2cdev", prev_i2cdev);
  esp_log_level_set("bmp280", prev_bmp);
  esp_log_level_set("MHZ14a", prev_mhz);
  esp_log_level_set("PMS7003", prev_pms);

  xSemaphoreGive(s_mutex);
  return result;
}

system_err_t SensorManager_ReleaseSensorIfUnassigned(
    SensorType_t sensor_type) {
  if (s_sm_state == SENSOR_MANAGER_STATE_STOPPING) return MRS_ERR_CORE_INVALID_STATE;
  if (s_sm_state == SENSOR_MANAGER_STATE_UNINITIALIZED) return MRS_ERR_CORE_NOT_INITIALIZED;
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  sensor_driver_t *driver = sensor_registry_get_driver_mutable(sensor_type);
  if (driver == NULL) {
    return MRS_ERR_SENSORS_NOT_FOUND;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(2000))) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  if (!type_assigned_locked(sensor_type, PORT_NONE)) {
    bool was_uart = driver->interface == COMMUNICATION_UART;
    deinitialize_driver_locked(driver);
    if (was_uart && !any_uart_sensor_assigned_locked()) {
      release_sensor_uart_lease_locked(&s_sensor_active_uart_lease);
    }
  }

  xSemaphoreGive(s_mutex);
  return MRS_OK;
}

bool SensorManager_IsSensorAssigned(SensorType_t sensor_type) {
  if (s_data == NULL || !take_manager_lock(pdMS_TO_TICKS(1500))) {
    return false;
  }
  bool assigned = type_assigned_locked(sensor_type, PORT_NONE);
  xSemaphoreGive(s_mutex);
  return assigned;
}

bool SensorManager_IsInterfaceAssigned(TypeCommunication_t interface) {
  if (s_data == NULL || !take_manager_lock(pdMS_TO_TICKS(1500))) {
    return false;
  }
  bool assigned = interface_assigned_locked(interface, PORT_NONE);
  xSemaphoreGive(s_mutex);
  return assigned;
}

bool SensorManager_HasEmptyPort(void) {
  if (s_data == NULL || !take_manager_lock(pdMS_TO_TICKS(1500))) {
    return false;
  }
  bool has_empty = false;
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    if (s_data->selectedSensor[port] == SENSOR_NONE) {
      has_empty = true;
      break;
    }
  }
  xSemaphoreGive(s_mutex);
  return has_empty;
}

size_t SensorManager_GetAvailablePortCount(void) {
  if (s_data == NULL || !take_manager_lock(pdMS_TO_TICKS(1500))) {
    return 0;
  }
  size_t count = 0;
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    if (s_data->selectedSensor[port] == SENSOR_NONE) {
      count++;
    }
  }
  xSemaphoreGive(s_mutex);
  return count;
}
