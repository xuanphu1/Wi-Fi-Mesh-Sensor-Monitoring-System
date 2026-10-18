#ifndef __SENSOR_REGISTRY_H__
#define __SENSOR_REGISTRY_H__

#include "SensorTypes.h"

/* -------------------- Sensor Registry Functions -------------------- */

/**
 * @brief Map SensorType_t to a display name string
 *
 * @param t Sensor type
 * @return Name (e.g. "BME280", "MH-Z14A")
 */
const char *sensor_type_to_name(SensorType_t t);

/**
 * @brief Number of registered sensor drivers
 *
 * @return Entry count
 */
size_t sensor_registry_get_count(void);

bool sensor_registry_get_descriptor(SensorType_t sensor_type,
                                    sensor_descriptor_t *out_descriptor);
bool sensor_registry_get_descriptor_at(size_t index,
                                       sensor_descriptor_t *out_descriptor);

/** Internal lifecycle access for SensorManager. Application/UI code must use
 * the callback-free descriptor APIs above. */
sensor_driver_t *sensor_registry_get_driver_mutable(SensorType_t sensor_type);
sensor_driver_t *sensor_registry_get_driver_mutable_at(
    size_t index, SensorType_t *out_sensor_type);

/**
 * @brief Count sensors for one interface (UART, I2C, SPI, PULSE)
 */
size_t sensor_registry_get_count_by_interface(TypeCommunication_t iface);

/**
 * @brief Descriptor at index within an interface group
 * @param iface COMMUNICATION_UART, COMMUNICATION_I2C, ...
 * @param index Index in group (0 .. count_by_interface - 1)
 * @param out_descriptor Receives callback-free sensor metadata
 * @return true when the descriptor exists
 */
bool sensor_registry_get_descriptor_at_interface(
    TypeCommunication_t iface, size_t index,
    sensor_descriptor_t *out_descriptor);

#endif /* __SENSOR_REGISTRY_H__ */

