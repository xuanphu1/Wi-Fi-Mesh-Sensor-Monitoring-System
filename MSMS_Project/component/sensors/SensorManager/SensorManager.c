#include "SensorManager.h"

#include "SensorRegistry.h"
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
static bool s_snapshot_valid[NUM_PORTS];
static sensor_manager_resource_hook_t s_uart_pause;
static sensor_manager_resource_hook_t s_uart_resume;
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

  if (driver->interface == COMMUNICATION_UART && s_uart_pause != NULL) {
    s_uart_pause();
  }

  bool bus_locked = take_bus_lock(driver);
  if (!bus_locked) {
    if (driver->interface == COMMUNICATION_UART && s_uart_resume != NULL) {
      s_uart_resume();
    }
    return MRS_ERR_I2CDEV_BUSY;
  }

  system_err_t result = driver->init();
  give_bus_lock(driver, bus_locked);
  if (result != MRS_OK) {
    if (driver->interface == COMMUNICATION_UART && s_uart_resume != NULL) {
      s_uart_resume();
    }
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

static void deinitialize_driver_locked(sensor_driver_t *driver) {
  if (driver == NULL || !driver->is_init) {
    return;
  }

  if (driver->deinit != NULL) {
    (void)driver->deinit();
  }
  driver->is_init = false;
}

static void sensor_sample_task(void *parameter) {
  (void)parameter;

  for (;;) {
    for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
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
    vTaskDelay(pdMS_TO_TICKS(SENSOR_SAMPLE_INTERVAL_MS));
  }
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

  s_data = data;
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
  if (s_sample_task != NULL) {
    return MRS_OK;
  }
  if (xTaskCreate(sensor_sample_task, "sensor_sample", 4096, NULL, 5,
                  &s_sample_task) != pdPASS) {
    return MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
  }
  return MRS_OK;
}

void SensorManager_SetUartHooks(sensor_manager_resource_hook_t pause_hook,
                                sensor_manager_resource_hook_t resume_hook) {
  s_uart_pause = pause_hook;
  s_uart_resume = resume_hook;
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
  if (previous_type != sensor_type &&
      type_assigned_locked(sensor_type, port)) {
    return MRS_ERR_DATAMANAGER_PORT_IN_USE;
  }

  if ((driver->interface == COMMUNICATION_UART ||
       driver->interface == COMMUNICATION_PULSE) &&
      interface_assigned_locked(driver->interface, port)) {
    return MRS_ERR_DATAMANAGER_PORT_IN_USE;
  }

  sensor_driver_t *previous =
      sensor_registry_get_driver_mutable(previous_type);
  bool replacing_uart =
      previous_type != sensor_type && previous != NULL &&
      previous->interface == COMMUNICATION_UART &&
      driver->interface == COMMUNICATION_UART;
  if (replacing_uart) {
    deinitialize_driver_locked(previous);
    if (s_uart_resume != NULL) {
      s_uart_resume();
    }
  }

  system_err_t result = initialize_driver_locked(driver);
  if (result == MRS_OK) {
    s_data->selectedSensor[port] = sensor_type;
    s_data->objectInfo.selectedSensorName[port] = driver->name;
    s_snapshot_valid[port] = false;

    if (previous_type != SENSOR_NONE && previous_type != sensor_type &&
        !type_assigned_locked(previous_type, port)) {
      bool previous_was_uart =
          previous != NULL && previous->interface == COMMUNICATION_UART;
      if (!replacing_uart) {
        deinitialize_driver_locked(previous);
      }
      if (previous_was_uart && !any_uart_sensor_assigned_locked() &&
          s_uart_resume != NULL) {
        s_uart_resume();
      }
    }
  } else if (replacing_uart) {
    system_err_t restore_result = initialize_driver_locked(previous);
    if (restore_result != MRS_OK) {
      ESP_LOGE(TAG, "Failed to restore %s after UART replacement failure: %s",
               sensor_type_to_name(previous_type),
               system_err_to_name(restore_result));
    }
  }

  return result;
}

system_err_t SensorManager_AssignSensor(PortId_t port,
                                        SensorType_t sensor_type) {
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

system_err_t SensorManager_ResetAll(void) {
  if (s_data == NULL || s_mutex == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }
  if (!take_manager_lock(pdMS_TO_TICKS(2000))) {
    return MRS_ERR_CORE_TIMEOUT;
  }

  size_t count = sensor_registry_get_count();
  for (size_t i = 0; i < count; i++) {
    SensorType_t type;
    sensor_driver_t *driver = sensor_registry_get_driver_mutable_at(i, &type);
    (void)type;
    deinitialize_driver_locked(driver);
  }

  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    s_data->selectedSensor[port] = SENSOR_NONE;
    s_data->objectInfo.selectedSensorName[port] =
        sensor_type_to_name(SENSOR_NONE);
    memset(&s_data->port_data[port], 0, sizeof(s_data->port_data[port]));
    s_snapshot_valid[port] = false;
  }

  xSemaphoreGive(s_mutex);
  if (s_uart_resume != NULL) {
    s_uart_resume();
  }
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

  bool was_initialized = driver->is_init;
  system_err_t result = initialize_driver_locked(driver);
  SensorData_t sample = {0};
  if (result == MRS_OK) {
    result = read_driver_locked(driver, &sample);
  }

  if (result != MRS_OK && !was_initialized) {
    deinitialize_driver_locked(driver);
    if (driver->interface == COMMUNICATION_UART && s_uart_resume != NULL &&
        !any_uart_sensor_assigned_locked()) {
      s_uart_resume();
    }
  } else if (result == MRS_OK && out_sample != NULL) {
    *out_sample = sample;
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
    if (was_uart && !any_uart_sensor_assigned_locked() &&
        s_uart_resume != NULL) {
      s_uart_resume();
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
