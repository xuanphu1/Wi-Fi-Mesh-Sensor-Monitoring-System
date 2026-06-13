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
 * @brief Pointer to the registered sensor driver table
 *
 * @return sensor_driver_t* driver array
 * @note Length from sensor_registry_get_count()
 */
sensor_driver_t *sensor_registry_get_drivers(void);

/**
 * @brief Number of registered sensor drivers
 *
 * @return Entry count
 */
size_t sensor_registry_get_count(void);

/**
 * @brief Lookup driver by sensor type
 *
 * @param sensor_type SensorType_t
 * @return Driver pointer, or NULL if unknown
 */
sensor_driver_t *sensor_registry_get_driver(SensorType_t sensor_type);

/**
 * @brief Count sensors for one interface (UART, I2C, SPI, PULSE)
 */
size_t sensor_registry_get_count_by_interface(TypeCommunication_t iface);

/**
 * @brief Driver and type at index within an interface group (menu builder)
 * @param iface COMMUNICATION_UART, COMMUNICATION_I2C, ...
 * @param index Index in group (0 .. count_by_interface - 1)
 * @param out_sensor_type If non-NULL, receives SensorType_t
 * @return Driver pointer, or NULL if index invalid
 */
sensor_driver_t *sensor_registry_get_driver_at_interface(TypeCommunication_t iface,
                                                          size_t index,
                                                          SensorType_t *out_sensor_type);

#endif /* __SENSOR_REGISTRY_H__ */

