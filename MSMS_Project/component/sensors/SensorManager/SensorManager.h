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

typedef void (*sensor_manager_resource_hook_t)(void);
typedef bool (*sensor_manager_bus_lock_fn)(uint32_t timeout_ms, void *context);
typedef void (*sensor_manager_bus_unlock_fn)(void *context);

system_err_t SensorManager_Init(DataManager_t *data);
system_err_t SensorManager_Start(void);

void SensorManager_SetUartHooks(sensor_manager_resource_hook_t pause_hook,
                                sensor_manager_resource_hook_t resume_hook);
void SensorManager_SetBusGuard(sensor_manager_bus_lock_fn lock_fn,
                               sensor_manager_bus_unlock_fn unlock_fn,
                               void *context);

system_err_t SensorManager_AssignSensor(PortId_t port,
                                        SensorType_t sensor_type);
system_err_t SensorManager_AssignFirstAvailable(SensorType_t sensor_type,
                                                PortId_t *out_port);
system_err_t SensorManager_ResetAll(void);

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

#endif /* SENSOR_MANAGER_H */
