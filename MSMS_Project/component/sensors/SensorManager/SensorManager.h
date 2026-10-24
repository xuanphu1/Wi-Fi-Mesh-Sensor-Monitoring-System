#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "DataManager.h"
#include "ErrorCodes.h"
#include "SensorTypes.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SENSOR_MANAGER_STATE_UNINITIALIZED = 0,
  SENSOR_MANAGER_STATE_INITIALIZED,
  SENSOR_MANAGER_STATE_RUNNING,
  SENSOR_MANAGER_STATE_STOPPING
} sensor_manager_state_t;

typedef bool (*sensor_manager_bus_lock_fn)(uint32_t timeout_ms, void *context);
typedef void (*sensor_manager_bus_unlock_fn)(void *context);

sensor_manager_state_t SensorManager_GetState(void);
system_err_t SensorManager_Init(DataManager_t *data);
system_err_t SensorManager_Start(void);
system_err_t SensorManager_Stop(void);
system_err_t SensorManager_Deinit(void);

void SensorManager_SetBusGuard(sensor_manager_bus_lock_fn lock_fn,
                               sensor_manager_bus_unlock_fn unlock_fn,
                               void *context);

system_err_t SensorManager_AssignSensor(PortId_t port,
                                        SensorType_t sensor_type);
system_err_t SensorManager_AssignFirstAvailable(SensorType_t sensor_type,
                                                PortId_t *out_port);
system_err_t SensorManager_ResetAll(void);

/* Immutable thread-safe snapshot APIs */
system_err_t SensorManager_GetPortSnapshot(PortId_t port,
                                           SensorPortSnapshot_t *out_snapshot);
system_err_t SensorManager_GetAllPortSnapshots(
    SensorPortSnapshot_t *out_snapshots, size_t max_snapshots,
    size_t *out_count);
system_err_t SensorManager_GetAssignedSensor(PortId_t port,
                                             SensorType_t *out_sensor_type);

/* Backward compatible snapshot query */
system_err_t SensorManager_GetSnapshot(PortId_t port,
                                       SensorType_t *out_sensor_type,
                                       SensorData_t *out_data);

system_err_t SensorManager_ProbeSensor(SensorType_t sensor_type,
                                       SensorData_t *out_sample);
system_err_t SensorManager_ReleaseSensorIfUnassigned(
    SensorType_t sensor_type);

bool SensorManager_IsSensorAssigned(SensorType_t sensor_type);
bool SensorManager_IsInterfaceAssigned(TypeCommunication_t interface);
bool SensorManager_HasEmptyPort(void);
size_t SensorManager_GetAvailablePortCount(void);

#ifdef __cplusplus
}
#endif

#if CONFIG_MSMS_ENABLE_TESTS
void SensorManager_TestInjectRestoreFault(int enable);
void SensorManager_TestInjectUartResumeFault(int enable);
#endif

#endif /* SENSOR_MANAGER_H */
