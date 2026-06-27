#ifndef __SENSOR_CONFIG_H__
#define __SENSOR_CONFIG_H__

#include "SensorTypes.h"
#include "bme280.h"
#include "esp_log.h"
#include "pms7003.h"
#include "ErrorCodes.h"

/* -------------------- Sensor Config Functions -------------------- */
// Sensor configuration and driver wrappers

system_err_t SensorConfigInit(void);
system_err_t SensorConfigRead(SensorData_t *data);
system_err_t SensorConfigDeinit(void);

/* -------------------- BME280 Driver Wrapper Functions -------------------- */
// BME280 driver wrappers

system_err_t bme280Initialize(void);
system_err_t bme280ReadData(SensorData_t *data);
system_err_t bme280Deinitialize(void);

/* -------------------- AHT10 Driver Wrapper Functions -------------------- */
system_err_t aht10Initialize(void);
system_err_t aht10ReadData(SensorData_t *data);
system_err_t aht10Deinitialize(void);

/* -------------------- PMS7003 Driver Wrapper Functions -------------------- */
system_err_t pms7003Initialize(void);
system_err_t pms7003ReadData(SensorData_t *data);
system_err_t pms7003Deinitialize(void);

/* -------------------- MH-Z14A Driver Wrapper Functions -------------------- */
system_err_t mhz14aInitialize(void);
system_err_t mhz14aReadData(SensorData_t *data);
system_err_t mhz14aDeinitialize(void);

/* -------------------- DHT11 Driver Wrapper Functions -------------------- */
system_err_t dht11Initialize(void);
system_err_t dht11ReadData(SensorData_t *data);
system_err_t dht11Deinitialize(void);

/* -------------------- DHT22 Driver Wrapper Functions -------------------- */
system_err_t dht22Initialize(void);
system_err_t dht22ReadData(SensorData_t *data);
system_err_t dht22Deinitialize(void);

/* -------------------- HTU21D Driver Wrapper Functions -------------------- */
system_err_t htu21dInitialize(void);
system_err_t htu21dReadData(SensorData_t *data);
system_err_t htu21dDeinitialize(void);

#endif /* __SENSOR_CONFIG_H__ */
