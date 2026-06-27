#include "SensorRegistry.h"
#include "SensorConfig.h"
#include "esp_log.h"
#include <stddef.h>
#include <stdint.h>

#define TAG_SENSOR_REGISTRY "SENSOR_REGISTRY"

/* -------------------- Test Functions (temporary) -------------------- */
static system_err_t TestInit(void) {
  ESP_LOGI(TAG_SENSOR_REGISTRY, "TestInit");
  return MRS_OK;
}

static system_err_t TestRead(SensorData_t *data) {
  if (data == NULL) {
    ESP_LOGW(TAG_SENSOR_REGISTRY, "TestRead: data is NULL");
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  ESP_LOGI(TAG_SENSOR_REGISTRY, "TestRead");
  return MRS_OK;
}

/* -------------------- Sensor Drivers Registry -------------------- */
// Table of all registered sensor drivers
static sensor_driver_t sensor_drivers[] = {
    {
        .name = "BME280",
        .init = bme280Initialize,
        .read = bme280ReadData,
        .deinit = bme280Deinitialize,
        .description = {"Temperature", "Pressure", "Humidity"},
        .unit = {"°C", "hPa", "%"},
        .unit_count = 3,
        .is_init = false,
        .interface = COMMUNICATION_I2C,
    },
    {
        .name = "MH-Z14A",
        .init = mhz14aInitialize,
        .read = mhz14aReadData,
        .deinit = mhz14aDeinitialize,
        .description = {"CO2", "Temperature"},
        .unit = {"ppm", "°C"},
        .unit_count = 2,
        .is_init = false,
        .interface = COMMUNICATION_UART,
    },
    {
        .name = "PMS7003",
        .init = pms7003Initialize,
        .read = pms7003ReadData,
        .deinit = pms7003Deinitialize,
        .description = {"PM1.0", "PM2.5", "PM10"},
        .unit = {"ug/m3", "ug/m3", "ug/m3"},
        .unit_count = 3,
        .is_init = false,
        .interface = COMMUNICATION_UART,
    },
    {
        .name = "DHT22",
        .init = dht22Initialize,
        .read = dht22ReadData,
        .deinit = dht22Deinitialize,
        .description = {"Temperature", "Humidity"},
        .unit = {"°C", "%"},
        .unit_count = 2,
        .is_init = false,
        .interface = COMMUNICATION_PULSE,
    },
    {
        .name = "AHT10",
        .init = aht10Initialize,
        .read = aht10ReadData,
        .deinit = aht10Deinitialize,
        .description = {"Temperature", "Humidity"},
        .unit = {"°C", "%"},
        .unit_count = 2,
        .is_init = false,
        .interface = COMMUNICATION_I2C,
    },
    {
        .name = "DHT11",
        .init = dht11Initialize,
        .read = dht11ReadData,
        .deinit = dht11Deinitialize,
        .description = {"Temperature", "Humidity"},
        .unit = {"°C", "%"},
        .unit_count = 2,
        .is_init = false,
        .interface = COMMUNICATION_PULSE,
    },
    {
        .name = "HTU21D",
        .init = htu21dInitialize,
        .read = htu21dReadData,
        .deinit = htu21dDeinitialize,
        .description = {"Temperature", "Humidity"},
        .unit = {"°C", "%"},
        .unit_count = 2,
        .is_init = false,
        .interface = COMMUNICATION_I2C,
    },
};

/* -------------------- Sensor Registry Functions -------------------- */

const char *sensor_type_to_name(SensorType_t t) {
  switch (t) {
  case SENSOR_BME280:
    return "BME280";
  case SENSOR_MHZ14A:
    return "MH-Z14A";
  case SENSOR_PMS7003:
    return "PMS7003";
  case SENSOR_DHT22:
    return "DHT22";
  case SENSOR_AHT10:
    return "AHT10";
  case SENSOR_DHT11:
    return "DHT11";
  case SENSOR_HTU21D:
    return "HTU21D";
  case SENSOR_NONE:
    return "None";
  default:
    return "Unknown";
  }
}

sensor_driver_t *sensor_registry_get_drivers(void) {
  return sensor_drivers;
}

size_t sensor_registry_get_count(void) {
  return sizeof(sensor_drivers) / sizeof(sensor_drivers[0]);
}

sensor_driver_t *sensor_registry_get_driver(SensorType_t sensor_type) {
  size_t count = sensor_registry_get_count();
  if (sensor_type < 0 || (size_t)sensor_type >= count) {
    return NULL;
  }
  return &sensor_drivers[sensor_type];
}

size_t sensor_registry_get_count_by_interface(TypeCommunication_t iface) {
  size_t total = sensor_registry_get_count();
  size_t n = 0;
  for (size_t i = 0; i < total; i++) {
    if (sensor_drivers[i].interface == iface) {
      n++;
    }
  }
  return n;
}

sensor_driver_t *sensor_registry_get_driver_at_interface(TypeCommunication_t iface,
                                                         size_t index,
                                                         SensorType_t *out_sensor_type) {
  size_t total = sensor_registry_get_count();
  size_t k = 0;
  for (size_t i = 0; i < total; i++) {
    if (sensor_drivers[i].interface != iface) {
      continue;
    }
    if (k == index) {
      if (out_sensor_type) {
        *out_sensor_type = (SensorType_t)i;
      }
      return &sensor_drivers[i];
    }
    k++;
  }
  return NULL;
}

