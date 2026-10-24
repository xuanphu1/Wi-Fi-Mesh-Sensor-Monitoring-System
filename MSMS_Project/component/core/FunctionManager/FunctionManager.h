#ifndef FUNCTION_MANAGER_H
#define FUNCTION_MANAGER_H

#include "DataManager.h"
#include "ErrorCodes.h"
#include "InternetManager.h"
#include "SensorTypes.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  SensorType_t detected[NUM_PORTS];
  size_t detected_count;
  size_t assigned_count;
} function_sensor_scan_result_t;

#define FM_EVENT_PORT_LABELS_CHANGED (1 << 0)
typedef void (*function_manager_ui_notify_fn)(uint32_t event_bits);
void FunctionManager_SetUiNotifier(function_manager_ui_notify_fn notify_fn);

typedef bool (*function_manager_bus_lock_fn)(uint32_t timeout_ms,
                                             void *context);
typedef void (*function_manager_bus_unlock_fn)(void *context);

typedef enum {
  FM_STATE_UNINITIALIZED = 0,
  FM_STATE_INITIALIZING,
  FM_STATE_RUNNING,
  FM_STATE_STOPPING,
  FM_STATE_CLEANUP_REQUIRED,
} fm_lifecycle_state_t;

fm_lifecycle_state_t FunctionManager_GetState(void);

system_err_t FunctionManager_Init(DataManager_t *data);
system_err_t FunctionManager_Deinit(void);

void FunctionManager_SetSensorBusGuard(
    function_manager_bus_lock_fn lock_fn,
    function_manager_bus_unlock_fn unlock_fn, void *context);

system_err_t FunctionManager_AssignSensor(PortId_t port,
                                          SensorType_t sensor_type);
system_err_t FunctionManager_ResetAllSensorPorts(void);

/* Immutable thread-safe snapshot APIs for UI and external consumers */
system_err_t FunctionManager_GetPortSnapshot(PortId_t port,
                                             SensorPortSnapshot_t *out_snapshot);
system_err_t FunctionManager_GetAllPortSnapshots(
    SensorPortSnapshot_t *out_snapshots, size_t max_snapshots,
    size_t *out_count);
system_err_t FunctionManager_GetAssignedSensor(PortId_t port,
                                               SensorType_t *out_sensor_type);

system_err_t FunctionManager_GetSensorSnapshot(PortId_t port,
                                               SensorType_t *out_sensor_type,
                                               SensorData_t *out_data);
system_err_t FunctionManager_GetSensorDescriptor(
    SensorType_t sensor_type, sensor_descriptor_t *out_descriptor);
const char *FunctionManager_GetSensorName(SensorType_t sensor_type);
size_t FunctionManager_GetSensorCountByInterface(
    TypeCommunication_t interface);
system_err_t FunctionManager_GetSensorDescriptorAtInterface(
    TypeCommunication_t interface, size_t index,
    sensor_descriptor_t *out_descriptor);
system_err_t FunctionManager_ScanAndAssignSensors(
    function_sensor_scan_result_t *out_result);

bool FunctionManager_IsSensorScanAllowed(void);
void FunctionManager_SetAutoScanEnabled(bool enable);
bool FunctionManager_IsAutoScanEnabled(void);
void FunctionManager_TriggerSensorScan(void);

system_err_t FunctionManager_RequestWifiConfiguration(void);
system_err_t FunctionManager_RequestMeshRole(mesh_role_t role);
system_err_t FunctionManager_GetNetworkStatus(InternetManagerStatus_t *out);

system_err_t FunctionManager_SetActuator(int gpio_num, bool enabled);
system_err_t FunctionManager_RefreshBatteryInfo(void);

#ifdef __cplusplus
}
#endif

#if CONFIG_MSMS_ENABLE_TESTS
void FunctionManager_TestInjectInitFault(int stage);
#endif

#endif /* FUNCTION_MANAGER_H */
