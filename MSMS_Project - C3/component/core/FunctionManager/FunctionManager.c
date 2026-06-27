#include "FunctionManager.h"
#include "BatteryManager.h"
#include "DataManager.h"
#include "InternetManager.h"
#include "MeshManager.h"
#include "ScreenManager.h"
#include "SensorConfig.h"
#include "SensorRegistry.h"
#include "WifiManager.h"
#include "driver/gpio.h"
#include "freertos/idf_additions.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static PortId_t PortSelected[NUM_PORTS] = {PORT_NONE, PORT_NONE, PORT_NONE};
static const char *port_name[NUM_PORTS] = {"Port 1", "Port 2", "Port 3"};
static TaskHandle_t readDataSensorTaskHandle[NUM_PORTS];
static TaskHandle_t mesh_root_screen_task_handle = NULL;
static TaskHandle_t mesh_join_task_handle = NULL;

static void mesh_root_screen_task(void *pvParameters) {
  DataManager_t *data = (DataManager_t *)pvParameters;

  while (data != NULL && InternetManager_GetMode() == INTERNET_MODE_MESH &&
         data->meshIo.role == MESH_ROLE_ROOT) {
    data->screen.is_menu_active = false;
    (void)ScreenMeshRoot(data);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  mesh_root_screen_task_handle = NULL;
  vTaskDelete(NULL);
}

static void FunctionManager_StartMeshRootScreen(DataManager_t *data) {
  if (data == NULL || mesh_root_screen_task_handle != NULL) {
    return;
  }

  if (xTaskCreate(mesh_root_screen_task, "mesh_root_screen", 3072, data, 4,
                  &mesh_root_screen_task_handle) != pdPASS) {
    ESP_LOGE(TAG_FUNCTION_MANAGER, "Create mesh_root_screen failed");
    mesh_root_screen_task_handle = NULL;
  }
}

static void FunctionManager_StopMeshRootScreen(void) {
  if (mesh_root_screen_task_handle != NULL) {
    vTaskDelete(mesh_root_screen_task_handle);
    mesh_root_screen_task_handle = NULL;
  }
}

static void FunctionManager_ReturnMainScreen(DataManager_t *data) {
  if (data == NULL || data->screen.current == NULL) {
    return;
  }

  menu_list_t *root = data->screen.current;
  while (root->parent != NULL) {
    root = root->parent;
  }

  data->screen.current = root;
  data->screen.selected = 0;
  data->screen.prev_selected = 0;
  data->screen.is_menu_active = true;
}

static void FunctionManager_UpdateWifiInfo(DataManager_t *data) {
  if (data == NULL) {
    return;
  }

  if (is_wifi_connected()) {
    data->objectInfo.wifiInfo.wifiStatus = CONNECTED;
  } else if (is_wifi_connecting()) {
    data->objectInfo.wifiInfo.wifiStatus = DISCONNECTED;
  } else {
    data->objectInfo.wifiInfo.wifiStatus = DISCONNECTED;
  }
}

void wifi_config_task(void *pvParameters) {
  DataManager_t *data = (DataManager_t *)pvParameters;
  system_err_t ret = InternetManager_Init(data, INTERNET_MODE_WIFI);
  if (ret != MRS_OK) {
    ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, ret);
    ESP_LOGE(TAG_FUNCTION_MANAGER, "Failed to initialize WiFi AP: %s",
             system_err_to_name(ret));
  }

  if (data->objectInfo.wifiInfo.wifiStatus == CONNECTED) {
    data->objectInfo.wifiInfo.wifiStatus = DISCONNECTED;
    ESP_LOGI(TAG_FUNCTION_MANAGER, "WiFi Config callback triggered");
  }

  data->screen.is_menu_active = false;
  while (1) {
    ScreenWifiConnecting(data);
    FunctionManager_UpdateWifiInfo(data);

    if (data->objectInfo.wifiInfo.wifiStatus == CONNECTED) {
      data->screen.is_menu_active = true;
      vTaskDelete(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void wifi_config_callback(void *ctx) {
  DataManager_t *data = (DataManager_t *)ctx;
  ESP_LOGI(TAG_FUNCTION_MANAGER, "WiFi Config callback triggered");
  xTaskCreate(wifi_config_task, "wifi_connect_task", 4096, data, 5, NULL);
}

/**
 * @brief FreeRTOS task: clean network state, start mesh (root or child), poll
 * until connected.
 *
 * @param pvParameters Heap-allocated MeshJoinTaskArg_t (freed before task
 * self-delete).
 */
static void wifi_mesh_join_task(void *pvParameters) {
  MeshJoinTaskArg_t *arg = (MeshJoinTaskArg_t *)pvParameters;
  DataManager_t *data = arg->data;
  bool as_root = arg->as_root;
  free(arg);

  mesh_role_t role = as_root ? MESH_ROLE_ROOT : MESH_ROLE_NODE;
  system_err_t ret = InternetManager_SwitchMeshMode(data, role);
  if (ret != MRS_OK) {
    ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, ret);
    ESP_LOGE(TAG_FUNCTION_MANAGER, "Mesh mode switch failed: %s",
             system_err_to_name(ret));
    mesh_join_task_handle = NULL;
    vTaskDelete(NULL);
    return;
  }

  if (as_root) {
    FunctionManager_StartMeshRootScreen(data);
  } else {
    FunctionManager_StopMeshRootScreen();
    FunctionManager_ReturnMainScreen(data);
  }
  mesh_join_task_handle = NULL;
  vTaskDelete(NULL);
}

void wifi_mesh_join_as_root_callback(void *ctx) {
  DataManager_t *data = (DataManager_t *)ctx;
  if (data == NULL) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "Mesh join: Data is NULL");
    return;
  }
  if (mesh_join_task_handle != NULL || InternetManager_IsTransitioning()) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "Join WiFi Mesh (root) ignored: transition running");
    return;
  }
  if (InternetManager_GetMode() == INTERNET_MODE_MESH &&
      MeshManager_IsStarted() && data->meshIo.role == MESH_ROLE_ROOT) {
    ESP_LOGI(TAG_FUNCTION_MANAGER, "Join WiFi Mesh (root) ignored: already root");
    FunctionManager_StartMeshRootScreen(data);
    return;
  }
  MeshJoinTaskArg_t *arg = (MeshJoinTaskArg_t *)malloc(sizeof(*arg));
  if (arg == NULL) {
    ESP_LOGE(TAG_FUNCTION_MANAGER, "Mesh join: malloc failed");
    return;
  }
  arg->data = data;
  arg->as_root = true;
  ESP_LOGI(TAG_FUNCTION_MANAGER, "Join WiFi Mesh (root)");
  if (xTaskCreate(wifi_mesh_join_task, "wifi_mesh_join_task", 4096, arg, 5,
                  &mesh_join_task_handle) != pdPASS) {
    ESP_LOGE(TAG_FUNCTION_MANAGER, "Mesh join: xTaskCreate failed");
    mesh_join_task_handle = NULL;
    free(arg);
  }
}

void wifi_mesh_join_as_node_callback(void *ctx) {
  DataManager_t *data = (DataManager_t *)ctx;
  if (data == NULL) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "Mesh join: Data is NULL");
    return;
  }
  if (mesh_join_task_handle != NULL || InternetManager_IsTransitioning()) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "Join WiFi Mesh (node) ignored: transition running");
    return;
  }
  if (InternetManager_GetMode() == INTERNET_MODE_MESH &&
      MeshManager_IsStarted() && data->meshIo.role == MESH_ROLE_NODE) {
    ESP_LOGI(TAG_FUNCTION_MANAGER, "Join WiFi Mesh (node) ignored: already node");
    FunctionManager_StopMeshRootScreen();
    FunctionManager_ReturnMainScreen(data);
    return;
  }
  MeshJoinTaskArg_t *arg = (MeshJoinTaskArg_t *)malloc(sizeof(*arg));
  if (arg == NULL) {
    ESP_LOGE(TAG_FUNCTION_MANAGER, "Mesh join: malloc failed");
    return;
  }
  arg->data = data;
  arg->as_root = false;
  ESP_LOGI(TAG_FUNCTION_MANAGER, "Join WiFi Mesh (node)");
  if (xTaskCreate(wifi_mesh_join_task, "wifi_mesh_join_task", 4096, arg, 5,
                  &mesh_join_task_handle) != pdPASS) {
    ESP_LOGE(TAG_FUNCTION_MANAGER, "Mesh join: xTaskCreate failed");
    mesh_join_task_handle = NULL;
    free(arg);
  }
}

/* -------------------- Actuators (GPIO output) -------------------- */

/**
 * @brief Configure GPIO as output and set level (actuator menu items).
 *
 * @param ctx GPIO number as (void *)(uintptr_t).
 * @param level 0 = off, non‑zero = on.
 */
static void actuator_set_level(void *ctx, int level) {
  int gpio_num = (int)(uintptr_t)ctx;
  gpio_config_t io = {
      .pin_bit_mask = (1ULL << gpio_num),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  if (gpio_config(&io) != ESP_OK) {
    ESP_LOGE(TAG_FUNCTION_MANAGER, "Actuator GPIO %d config failed", gpio_num);
    return;
  }
  gpio_set_level((gpio_num_t)gpio_num, level);
  ESP_LOGI(TAG_FUNCTION_MANAGER, "Actuator GPIO %d -> %s", gpio_num,
           level ? "ON" : "OFF");
}

void actuator_on_cb(void *ctx) { actuator_set_level(ctx, 1); }

void actuator_off_cb(void *ctx) { actuator_set_level(ctx, 0); }

void information_callback(void *ctx) {
  DataManager_t *data = (DataManager_t *)ctx;
  if (data == NULL) {
    return;
  }
  static const char *info_lines[] = {
      "MSMS Project",
      "Multi-sensor module",
      "Multi-interface sensors",
      "Author: MrKoi",
  };
  data->screen.is_menu_active = false;
  ScreenShowInformation(info_lines, sizeof(info_lines) / sizeof(info_lines[0]));
  vTaskDelay(pdMS_TO_TICKS(4000));
  /* Redraw menu after info screen */
  data->screen.is_menu_active = true;
}

void read_temperature_cb(void *ctx) {}

void read_humidity_cb(void *ctx) {}

void read_pressure_cb(void *ctx) {}

void read_dht22_cb(void *ctx) {}

void battery_status_callback(void *ctx) {
  DataManager_t *data = (DataManager_t *)ctx;
  if (data == NULL) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "battery_status_callback: data is NULL");
    return;
  }

  ESP_LOGI(TAG_FUNCTION_MANAGER, "Battery Status callback triggered");

  // Refresh battery fields from BatteryManager
  BatteryManager_UpdateInfo(&(data->objectInfo.batteryInfo));

  // Show Battery Status screen
  if (data->MenuReturn[1] != NULL) {
    data->screen.is_menu_active = true;
  }
}

// Shared buffers for port row labels after reset
static char g_port_label_buf[NUM_PORTS][24];

void reset_all_ports_callback(void *ctx) {
  DataManager_t *data = (DataManager_t *)ctx;
  for (PortId_t i = 0; i < NUM_PORTS; i++) {
    data->selectedSensor[i] = SENSOR_NONE;
    PortSelected[i] = PORT_NONE;
    snprintf(g_port_label_buf[i], sizeof(g_port_label_buf[i]), "%s",
             (const char *)port_name[i]);
  }
  sensor_driver_t *drivers = sensor_registry_get_drivers();
  size_t driver_count = sensor_registry_get_count();
  for (size_t i = 0; i < driver_count; i++) {
    drivers[i].is_init = false;
  }

  if (data->on_ports_reset) {
    data->on_ports_reset(data);
  }
  /* Refresh current menu item names (e.g. Show data sensor rows) */
  for (PortId_t i = 0; i < NUM_PORTS; i++) {
    data->screen.current->items[i].name = g_port_label_buf[i];
  }

  // Stop any running per-port read tasks
  for (PortId_t i = 0; i < NUM_PORTS; i++) {
    if (readDataSensorTaskHandle[i] != NULL) {
      vTaskDelete(readDataSensorTaskHandle[i]);
      readDataSensorTaskHandle[i] = NULL;
    }
  }
  ESP_LOGI(TAG_FUNCTION_MANAGER, "Reset all ports");
  data->screen.is_menu_active = true;
}

void select_sensor_cb(void *ctx) {
  SelectionParam_t *param = (SelectionParam_t *)ctx;

  // Debug: log as soon as callback runs
  ESP_LOGI(TAG_FUNCTION_MANAGER, "select_sensor_cb called: param=%p", param);
  if (param != NULL) {
    ESP_LOGI(TAG_FUNCTION_MANAGER, "  param->port=%d, param->sensor=%d",
             param->port, param->sensor);
    ESP_LOGI(TAG_FUNCTION_MANAGER, "  param->data=%p", param->data);
  }

  // 1. Validate parameters
  if (param == NULL || param->data == NULL) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "select_sensor_cb: invalid ctx");
    return;
  }
  if (param->port < 0 || param->port >= NUM_PORTS) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "select_sensor_cb: invalid port %d",
             param->port);
    return;
  }

  ESP_LOGI(TAG_FUNCTION_MANAGER, "Selected sensor %d (%s) for port %d",
           param->sensor, sensor_type_to_name(param->sensor), param->port);

  // 2. Resolve driver; set selectedSensor only after successful or prior init
  sensor_driver_t *driver = sensor_registry_get_driver(param->sensor);
  if (driver == NULL) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "select_sensor_cb: invalid sensor type %d",
             param->sensor);
    return;
  }
  Message_t message_type = MESSAGE_NONE;
  bool allow_set_sensor = false; // only commit selectedSensor[port] when true

  if (driver->init != NULL) {
    if (PortSelected[param->port] == param->port) {
      message_type = MESSAGE_PORT_SELECTED;
      ESP_LOGI(TAG_FUNCTION_MANAGER, "Port %d was selected", param->port);
      allow_set_sensor = true;
    } else if (!driver->is_init) {
      system_err_t init_ret = driver->init();
      if (init_ret != MRS_OK) {
        ErrorCodes_PushError(param->data->error_code,
                             DATA_MANAGER_ERROR_CAPACITY, init_ret);
        ESP_LOGE(TAG_FUNCTION_MANAGER,
                 "select_sensor_cb: init failed for sensor %d: %s",
                 param->sensor, system_err_to_name(init_ret));
        message_type = MESSAGE_SENSOR_NOT_INITIALIZED;
        /* Do not set selectedSensor — keep plain "Port X" */
      } else {
        driver->is_init = true;
        PortSelected[param->port] = param->port;
        allow_set_sensor = true;
      }
    } else {
      PortSelected[param->port] = param->port;
      ESP_LOGI(TAG_FUNCTION_MANAGER,
               "select_sensor_cb: sensor %d is already initialized",
               param->sensor);
      message_type = MESSAGE_SENSOR_USED_OTHER_PORT;
      allow_set_sensor = true;
    }
  } else {
    ESP_LOGW(TAG_FUNCTION_MANAGER,
             "select_sensor_cb: init is NULL for sensor %d", param->sensor);
    message_type = MESSAGE_SENSOR_NOT_INITIALIZED;
    /* No init hook — treat as not selected, keep "Port X" */
  }

  if (allow_set_sensor) {
    param->data->selectedSensor[param->port] = (int8_t)param->sensor;
  } else {
    param->data->selectedSensor[param->port] = (int8_t)SENSOR_NONE;
  }

  if (message_type != MESSAGE_NONE) {
    param->data->screen.is_menu_active = false;
    ScreenShowMessage(message_type);
    vTaskDelay(pdMS_TO_TICKS(1000));
    param->data->screen.is_menu_active = true;
  }

  // 5. MenuSystem updates port labels and returns via on_sensor_selected
  if (param->data->on_sensor_selected) {
    param->data->on_sensor_selected(param->data, param->port);
  }
}

/**
 * @brief Periodic read loop for one port: driver read, optional live graph.
 *
 * @param pvParameters ShowDataSensorParam_t * as void *.
 */
void readDataSensorTask(void *pvParameters) {
  ShowDataSensorParam_t *param = (ShowDataSensorParam_t *)pvParameters;
  if (param == NULL || param->data == NULL) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "readDataSensorTask: data is NULL");
    return;
  }
  while (1) {
    SensorData_t data;
    sensor_driver_t *driver =
        sensor_registry_get_driver(param->data->selectedSensor[param->port]);
    if (driver == NULL || driver->read == NULL) {
      ESP_LOGW(TAG_FUNCTION_MANAGER, "readDataSensorTask: invalid driver");
      param->data->screen.is_menu_active = false;
      ScreenShowMessage(MESSAGE_SENSOR_NOT_INITIALIZED);
      vTaskDelay(pdMS_TO_TICKS(1000));
      param->data->screen.is_menu_active = true;
      readDataSensorTaskHandle[param->port] = NULL;
      vTaskDelete(readDataSensorTaskHandle[param->port]);
      vTaskDelete(NULL);
      return;
    }

    system_err_t read_ret = driver->read(&data);
    if (read_ret != MRS_OK) {
      ErrorCodes_PushError(param->data->error_code, DATA_MANAGER_ERROR_CAPACITY,
                           read_ret);
      ESP_LOGW(TAG_FUNCTION_MANAGER, "readDataSensorTask: read failed: %s",
               system_err_to_name(read_ret));
      // Retry on next iteration
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    
    // Save to global data
    param->data->port_data[param->port] = data;

    const char **field_names = driver->description;
    const char **units = driver->unit;
    size_t count = driver->unit_count;
    if (param->ShowDataScreen) {
      param->data->screen.is_menu_active = false;
      ScreenShowDataSensor(field_names, data.data_fl, units, count);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void show_data_sensor_cb(void *ctx) {
  ShowDataSensorParam_t *param = (ShowDataSensorParam_t *)ctx;
  param->ShowDataScreen = true;
  if (param == NULL || param->data == NULL) {
    ESP_LOGW(TAG_FUNCTION_MANAGER, "show_data_sensor_cb: data is NULL");
    return;
  }
  ESP_LOGI(TAG_FUNCTION_MANAGER, "Port %d read sensor %s", param->port,
           sensor_type_to_name(param->data->selectedSensor[param->port]));
  if (readDataSensorTaskHandle[param->port] == NULL) {
    ESP_LOGI(TAG_FUNCTION_MANAGER, "Task is running");
    xTaskCreate(readDataSensorTask, "readDataSensorTask", 4096, param, 5,
                &readDataSensorTaskHandle[param->port]);
  }
}
