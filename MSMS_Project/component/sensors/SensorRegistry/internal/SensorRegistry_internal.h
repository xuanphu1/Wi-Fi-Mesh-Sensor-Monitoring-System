#ifndef __SENSOR_REGISTRY_INTERNAL_H__
#define __SENSOR_REGISTRY_INTERNAL_H__

#include "SensorTypes.h"
#include "ErrorCodes.h"
#include "sdkconfig.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------- Sensor Driver Structure -------------------- */
// Internal driver descriptor with lifecycle callbacks.
// Accessible ONLY within SensorRegistry and SensorManager.
typedef struct {
  SensorType_t type;                    // Stable sensor identifier
  const char *name;                    // e.g. "BME280"
  const char *description[20];         // Field labels
  const char *unit[20];                // Units
  bool is_init;                        // Init completed
  uint8_t unit_count;                  // Number of fields
  TypeCommunication_t interface;       // UART, I2C, SPI, PULSE
  system_err_t (*init)(void);          // init()
  system_err_t (*read)(SensorData_t *); // read into SensorData_t
  system_err_t (*deinit)(void);        // optional deinit
} sensor_driver_t;

/**
 * @brief Internal lifecycle driver access for SensorManager.
 * Application, UI, scanner and telemetry must use public callback-free
 * sensor_descriptor_t APIs from SensorRegistry.h instead.
 */
sensor_driver_t *sensor_registry_get_driver_mutable(SensorType_t sensor_type);

sensor_driver_t *sensor_registry_get_driver_mutable_at(
    size_t index, SensorType_t *out_sensor_type);

#if CONFIG_MSMS_ENABLE_TESTS
/**
 * @brief Inject a mock driver override for a specific sensor type (for testing).
 */
void sensor_registry_set_driver_override(SensorType_t type,
                                         const sensor_driver_t *override_driver);

/**
 * @brief Clear all active driver overrides and restore hardware drivers.
 */
void sensor_registry_reset_driver_overrides(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_REGISTRY_INTERNAL_H__ */
