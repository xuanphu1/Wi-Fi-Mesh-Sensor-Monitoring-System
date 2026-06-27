#ifndef __FUNCTION_MANAGER_H__
#define __FUNCTION_MANAGER_H__

#include "DataManager.h"
#include "ErrorCodes.h"
#include "MeshManager.h"
#include "ScreenManager.h"
#include "SensorConfig.h"
#include "esp_log.h"
#include "string.h"
#include <stdbool.h>

#define TAG_FUNCTION_MANAGER "FUNCTION_MANAGER"

void wifi_config_callback(void *ctx);
void wifi_mesh_join_as_root_callback(void *ctx);
void wifi_mesh_join_as_node_callback(void *ctx);
void information_callback(void *ctx);
void read_temperature_cb(void *ctx);
void read_humidity_cb(void *ctx);
void read_pressure_cb(void *ctx);
void read_dht22_cb(void *ctx);
void battery_status_callback(void *ctx);
void reset_all_ports_callback(void *ctx);
void actuator_on_cb(void *ctx);
void actuator_off_cb(void *ctx);
void select_sensor_cb(void *ctx);
void show_data_sensor_cb(void *ctx);

#endif /* __FUNCTION_MANAGER_H__ */
