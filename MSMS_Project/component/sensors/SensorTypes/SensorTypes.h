#ifndef __SENSOR_TYPES_H__
#define __SENSOR_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ErrorCodes.h"

/* -------------------- Sensor Definitions -------------------- */
// Number of sensor ports
#define NUM_PORTS 3
// Actual sensor count comes from sensor_registry_get_count(); no MAX_SENSORS — dynamic alloc

/* -------------------- Port ID Enum -------------------- */
// Port id for menu / selection tracking
typedef enum {
  PORT_NONE = -1,
  PORT_1 = 0,
  PORT_2 = 1,
  PORT_3 = 2,
} PortId_t;

/* -------------------- Sensor Type Enum -------------------- */
// Sensor type id
typedef enum {
  SENSOR_NONE = -1,
  SENSOR_BME280 = 0,
  SENSOR_MHZ14A = 1,
  SENSOR_PMS7003 = 2,
  SENSOR_DHT22 = 3,
  SENSOR_AHT10 = 4,
  SENSOR_DHT11 = 5,
  SENSOR_HTU21D = 6
} SensorType_t;

typedef enum {
  PIN_1 = 0,
  PIN_2 = 1,
  PIN_3 = 2,
  PIN_4 = 3,
  PIN_5 = 4
} PinPort_t;

/** Menu order: UART, I2C, SPI, PULSE (see sensor_registry_get_count_by_interface) */
typedef enum {
  COMMUNICATION_NONE = -1,
  COMMUNICATION_UART = 0,
  COMMUNICATION_I2C = 1,
  COMMUNICATION_SPI = 2,
  COMMUNICATION_PULSE = 3,
  COMMUNICATION_DIGITAL = 4,
} TypeCommunication_t;

/* -------------------- Sensor Data Structure -------------------- */
// Sensor read payload
typedef struct {
  float data_fl[5];
  uint32_t data_uint32[5];
  uint16_t data_uint16[5];
  uint8_t data_uint8[5];
} SensorData_t;

/* -------------------- Sensor Driver Structure -------------------- */
// One registered sensor driver
typedef struct {
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

#endif /* __SENSOR_TYPES_H__ */

