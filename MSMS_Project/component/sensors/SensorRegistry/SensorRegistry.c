#include "SensorRegistry.h"
#include "SensorRegistry_internal.h"
#include "SensorConfig.h"
#include "sdkconfig.h"
#include <stddef.h>
#include <stdint.h>


#define SENSOR_DRIVERS_COUNT (sizeof(sensor_drivers) / sizeof(sensor_drivers[0]))
/* -------------------- Sensor Drivers Registry -------------------- */
// Table of all registered sensor drivers
static sensor_driver_t sensor_drivers[] = {
    {
        .type = SENSOR_BME280,
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
        .type = SENSOR_MHZ14A,
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
        .type = SENSOR_PMS7003,
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
        .type = SENSOR_DHT22,
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
        .type = SENSOR_AHT10,
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
        .type = SENSOR_DHT11,
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
        .type = SENSOR_HTU21D,
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

size_t sensor_registry_get_count(void) {
  return SENSOR_DRIVERS_COUNT;
}

static void fill_descriptor(const sensor_driver_t *driver,
                            sensor_descriptor_t *out_descriptor) {
  out_descriptor->type = driver->type;
  out_descriptor->name = driver->name;
  out_descriptor->description = driver->description;
  out_descriptor->unit = driver->unit;
  out_descriptor->unit_count = driver->unit_count;
  out_descriptor->interface = driver->interface;
}

bool sensor_registry_get_descriptor(SensorType_t sensor_type,
                                    sensor_descriptor_t *out_descriptor) {
  if (out_descriptor == NULL) {
    return false;
  }
  sensor_driver_t *driver = sensor_registry_get_driver_mutable(sensor_type);
  if (driver == NULL) {
    return false;
  }
  fill_descriptor(driver, out_descriptor);
  return true;
}

#if CONFIG_MSMS_ENABLE_TESTS
static sensor_driver_t s_driver_overrides[SENSOR_DRIVERS_COUNT];
static bool s_has_override[SENSOR_DRIVERS_COUNT] = {false};

void sensor_registry_set_driver_override(SensorType_t type,
                                         const sensor_driver_t *override_driver) {
  size_t count = sensor_registry_get_count();
  for (size_t i = 0; i < count; i++) {
    if (sensor_drivers[i].type == type) {
      if (override_driver != NULL) {
        s_driver_overrides[i] = *override_driver;
        s_has_override[i] = true;
      } else {
        s_has_override[i] = false;
      }
      return;
    }
  }
}

void sensor_registry_reset_driver_overrides(void) {
  for (size_t i = 0; i < SENSOR_DRIVERS_COUNT; i++) {
    s_has_override[i] = false;
  }
}
#endif

bool sensor_registry_get_descriptor_at(size_t index,
                                       sensor_descriptor_t *out_descriptor) {
  if (out_descriptor == NULL || index >= sensor_registry_get_count()) {
    return false;
  }
#if CONFIG_MSMS_ENABLE_TESTS
  const sensor_driver_t *driver =
      s_has_override[index] ? &s_driver_overrides[index] : &sensor_drivers[index];
#else
  const sensor_driver_t *driver = &sensor_drivers[index];
#endif
  fill_descriptor(driver, out_descriptor);
  return true;
}

sensor_driver_t *sensor_registry_get_driver_mutable(SensorType_t sensor_type) {
  size_t count = sensor_registry_get_count();
  for (size_t i = 0; i < count; i++) {
    if (sensor_drivers[i].type == sensor_type) {
#if CONFIG_MSMS_ENABLE_TESTS
      return s_has_override[i] ? &s_driver_overrides[i] : &sensor_drivers[i];
#else
      return &sensor_drivers[i];
#endif
    }
  }
  return NULL;
}

sensor_driver_t *sensor_registry_get_driver_mutable_at(
    size_t index, SensorType_t *out_sensor_type) {
  if (index >= sensor_registry_get_count()) {
    return NULL;
  }
  if (out_sensor_type != NULL) {
    *out_sensor_type = sensor_drivers[index].type;
  }
#if CONFIG_MSMS_ENABLE_TESTS
  return s_has_override[index] ? &s_driver_overrides[index] : &sensor_drivers[index];
#else
  return &sensor_drivers[index];
#endif
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

bool sensor_registry_get_descriptor_at_interface(
    TypeCommunication_t iface, size_t index,
    sensor_descriptor_t *out_descriptor) {
  if (out_descriptor == NULL) {
    return false;
  }
  size_t total = sensor_registry_get_count();
  size_t k = 0;
  for (size_t i = 0; i < total; i++) {
    if (sensor_drivers[i].interface != iface) {
      continue;
    }
    if (k == index) {
      fill_descriptor(&sensor_drivers[i], out_descriptor);
      return true;
    }
    k++;
  }
  return false;
}
