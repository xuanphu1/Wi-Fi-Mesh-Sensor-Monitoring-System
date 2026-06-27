#include "SensorConfig.h"
#include "PinManager.h"
#include "aht.h"
#include "dht.h"
#include "mhz14a.h"
#include "sdkconfig.h"
#include "si7021.h"

/*
 * ============================================================================
 * PIN / BUS SETUP FOR SENSOR INIT (this file)
 * ============================================================================
 *
 * 1) I2C (BME280, AHT10, and any sensor on the shared I2C bus)
 *    - SDA / SCL / I2C port / bus speed come from PinManager.h (PIN_I2C_*
 * macros).
 *    - Configure via: idf.py menuconfig -> "PinManager — I2C".
 *    - The bus must be enabled first: the app usually calls i2cInitDevCommon()
 * (i2cdev) in main before OLED / sensors; without that, bme280_init /
 * aht_init_desc will fail.
 *
 * 2) UART — PMS7003
 *    - TX/RX pins and UART port are defined in the PMS7003 component
 *      (Kconfig "PMS7003 configuration"), not necessarily the same as
 * PIN_UART_* in PinManager unless you keep those values in sync manually.
 *    - Init path: pms7003_initUart() + pms7003_activeMode() (see
 * pms7003Initialize).
 *
 * 3) Adding a new sensor by interface
 *    - I2C: use PIN_I2C_PORT_NUM, PIN_I2C_SDA, PIN_I2C_SCL.
 *    - UART: add a dedicated Kconfig or reuse PinManager UART if shared.
 * ============================================================================
 */

bmp280_t bme280_device;
bmp280_params_t bme280_params;
static bool bme280_initialized = false;

static aht_t aht10_device;
static bool aht10_initialized = false;

/* -------------------- BME280 Driver Wrapper Functions -------------------- */

system_err_t bme280Initialize(void) {
  if (bme280_initialized) {
    ESP_LOGW("sensor_bme280_init", "BME280 already initialized");
    return MRS_OK;
  }

  esp_err_t ret = bme280_init(&bme280_device, &bme280_params, BME280_ADDRESS,
                              PIN_I2C_PORT_NUM, PIN_I2C_SDA, PIN_I2C_SCL);
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_bme280_init", "bme280_init failed: %s",
             esp_err_to_name(ret));
    return MRS_ERR_SENSOR_BME280_INIT_FAILED;
  }

  bme280_initialized = true;
  return MRS_OK;
}

system_err_t bme280ReadData(SensorData_t *data) {
  if (data == NULL) {
    ESP_LOGE("sensor_bme280_read", "data pointer is NULL");
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  if (!bme280_initialized) {
    ESP_LOGE("sensor_bme280_read", "BME280 not initialized");
    return MRS_ERR_SENSORS_NOT_INITIALIZED;
  }

  esp_err_t ret = bme280_readSensorData(&bme280_device, &data->data_fl[0],
                                        &data->data_fl[1], &data->data_fl[2]);
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_bme280_read", "read failed: %s", esp_err_to_name(ret));
    return MRS_ERR_SENSOR_BME280_READ_FAILED;
  }

  return MRS_OK;
}

system_err_t bme280Deinitialize(void) {
  if (!bme280_initialized) {
    return MRS_OK;
  }

  // TODO: Implement deinit if needed
  // ESP_ERROR_CHECK(bme280_deinit(&bme280_device));
  bme280_initialized = false;
  return MRS_OK;
}

/* -------------------- AHT10 Driver Wrapper Functions -------------------- */

system_err_t aht10Initialize(void) {
  if (aht10_initialized) {
    ESP_LOGW("sensor_aht10_init", "AHT10 already initialized");
    return MRS_OK;
  }

  esp_err_t ret = aht_init_desc(&aht10_device, AHT_I2C_ADDRESS_GND,
                                PIN_I2C_PORT_NUM, PIN_I2C_SDA, PIN_I2C_SCL);
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_aht10_init", "aht_init_desc failed: %s",
             esp_err_to_name(ret));
    return MRS_ERR_I2CDEV_INIT_FAILED;
  }

  aht10_device.type = AHT_TYPE_AHT1x;
  aht10_device.mode = AHT_MODE_NORMAL;

  ret = aht_init(&aht10_device);
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_aht10_init", "aht_init failed: %s", esp_err_to_name(ret));
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }

  aht10_initialized = true;
  return MRS_OK;
}

system_err_t aht10ReadData(SensorData_t *data) {
  if (data == NULL) {
    ESP_LOGE("sensor_aht10_read", "data pointer is NULL");
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  if (!aht10_initialized) {
    system_err_t ret = aht10Initialize();
    if (ret != MRS_OK) {
      return ret;
    }
  }

  float temp = 0.0f, hum = 0.0f;
  esp_err_t ret = aht_get_data(&aht10_device, &temp, &hum);
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_aht10_read", "aht_get_data failed: %s",
             esp_err_to_name(ret));
    return MRS_ERR_DRIVERS_COMM_FAILED;
  }

  data->data_fl[0] = temp;
  data->data_fl[1] = hum;
  return MRS_OK;
}

system_err_t aht10Deinitialize(void) {
  if (!aht10_initialized) {
    return MRS_OK;
  }

  esp_err_t ret = aht_free_desc(&aht10_device);
  if (ret != ESP_OK) {
    ESP_LOGW("sensor_aht10_deinit", "aht_free_desc failed: %s",
             esp_err_to_name(ret));
  }
  aht10_initialized = false;
  return MRS_OK;
}

/* Custom names; wrappers use system_err_t and similar parameter patterns. */

/* -------------------- PMS7003 Driver Wrapper Functions -------------------- */
system_err_t pms7003Initialize(void) {
  esp_err_t ret = pms7003_initUart(PIN_UART_NUM, PIN_UART_TX, PIN_UART_RX);
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_pms7003_init", "init failed: %s", esp_err_to_name(ret));
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }

  ret = pms7003_activeMode();
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_pms7003_init", "pms7003_activeMode failed: %s",
             esp_err_to_name(ret));
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }

  return MRS_OK;
}

system_err_t pms7003ReadData(SensorData_t *data) {
  if (data == NULL) {
    ESP_LOGE("sensor_pms7003_read", "data pointer is NULL");
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  uint32_t pm1_0 = 0;
  uint32_t pm2_5 = 0;
  uint32_t pm10 = 0;
  esp_err_t ret = pms7003_readData(outdoor, &pm1_0, &pm2_5, &pm10);
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_pms7003_read", "pms7003_readData failed: %s",
             esp_err_to_name(ret));
    return MRS_ERR_SENSORS_READ_FAILED;
  }

  data->data_fl[0] = (float)pm1_0;
  data->data_fl[1] = (float)pm2_5;
  data->data_fl[2] = (float)pm10;
  data->data_uint32[0] = pm1_0;
  data->data_uint32[1] = pm2_5;
  data->data_uint32[2] = pm10;

  return MRS_OK;
}

system_err_t pms7003Deinitialize(void) { return MRS_OK; }

/* -------------------- MH-Z14A Driver Wrapper Functions -------------------- */
system_err_t mhz14aInitialize(void) {
#ifdef CONFIG_MHZ14A_UART
  uart_config_t uart_config = {
      .baud_rate = PIN_UART_PERIPHERAL_BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .stop_bits = UART_STOP_BITS_1,
      .parity = UART_PARITY_DISABLE,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };
  esp_err_t ret =
      mhz14a_init_uart(PIN_UART_NUM, PIN_UART_TX, PIN_UART_RX, &uart_config);
#else
  esp_err_t ret = mhz14a_init_pwm(PIN_PULSE_2);
#endif
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_mhz14a_init", "init failed: %s", esp_err_to_name(ret));
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }
  return MRS_OK;
}

system_err_t mhz14aReadData(SensorData_t *data) {
  if (data == NULL)
    return MRS_ERR_CORE_INVALID_PARAM;
#ifdef CONFIG_MHZ14A_UART
  mhz14a_data_t mhz_data;
  esp_err_t ret = mhz14a_get_data_uart(&mhz_data);
  if (ret != ESP_OK)
    return MRS_ERR_SENSORS_READ_FAILED;
  data->data_fl[0] = (float)mhz_data.co2_ppm;
  data->data_fl[1] = (float)mhz_data.temperature;
#else
  uint32_t co2 = 0;
  esp_err_t ret = mhz14a_get_data_pwm(&co2);
  if (ret != ESP_OK)
    return MRS_ERR_SENSORS_READ_FAILED;
  data->data_fl[0] = (float)co2;
  data->data_fl[1] = 0.0f; // PWM mode does not return temperature
#endif
  return MRS_OK;
}

system_err_t mhz14aDeinitialize(void) { return MRS_OK; }

/* -------------------- DHT11 / DHT22 Driver Wrapper Functions
 * -------------------- */

static uint32_t last_dht_read_time = 0;
static float cached_dht_temp = 0.0f;
static float cached_dht_hum = 0.0f;

static system_err_t dht_read_common(dht_sensor_type_t type,
                                    SensorData_t *data) {
  if (data == NULL)
    return MRS_ERR_CORE_INVALID_PARAM;
    
  uint32_t current_time = pdTICKS_TO_MS(xTaskGetTickCount());
  if (current_time - last_dht_read_time < 2000 && last_dht_read_time != 0) {
    data->data_fl[0] = cached_dht_temp;
    data->data_fl[1] = cached_dht_hum;
    return MRS_OK;
  }

  float temp = 0.0f, hum = 0.0f;
  gpio_num_t pin = PIN_PULSE_1;
  esp_err_t ret = dht_read_float_data(type, pin, &hum, &temp);
  ESP_LOGI("sensor_dht_read_data", "Humid %f, Temp %f", hum, temp);
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_dht_read", "read failed: %s", esp_err_to_name(ret));
    return MRS_ERR_SENSORS_READ_FAILED;
  }
  
  cached_dht_temp = temp;
  cached_dht_hum = hum;
  last_dht_read_time = current_time;

  data->data_fl[0] = temp;
  data->data_fl[1] = hum;
  return MRS_OK;
}

system_err_t dht11Initialize(void) { return MRS_OK; }
system_err_t dht11ReadData(SensorData_t *data) {
  return dht_read_common(DHT_TYPE_DHT11, data);
}
system_err_t dht11Deinitialize(void) { return MRS_OK; }

system_err_t dht22Initialize(void) { return MRS_OK; }
system_err_t dht22ReadData(SensorData_t *data) {
  return dht_read_common(DHT_TYPE_AM2301, data);
}
system_err_t dht22Deinitialize(void) { return MRS_OK; }

/* -------------------- HTU21D Driver Wrapper Functions -------------------- */
static i2c_dev_t htu21d_device;
static bool htu21d_initialized = false;

system_err_t htu21dInitialize(void) {
  if (htu21d_initialized)
    return MRS_OK;
  esp_err_t ret = si7021_init_desc(&htu21d_device, PIN_I2C_PORT_NUM,
                                   PIN_I2C_SDA, PIN_I2C_SCL);
  if (ret != ESP_OK) {
    ESP_LOGE("sensor_htu21d_init", "init_desc failed: %s",
             esp_err_to_name(ret));
    return MRS_ERR_I2CDEV_INIT_FAILED;
  }
  htu21d_initialized = true;
  return MRS_OK;
}

system_err_t htu21dReadData(SensorData_t *data) {
  if (data == NULL)
    return MRS_ERR_CORE_INVALID_PARAM;
  if (!htu21d_initialized)
    return MRS_ERR_SENSORS_NOT_INITIALIZED;

  float temp = 0.0f, hum = 0.0f;
  if (si7021_measure_temperature(&htu21d_device, &temp) != ESP_OK ||
      si7021_measure_humidity(&htu21d_device, &hum) != ESP_OK) {
    ESP_LOGE("sensor_htu21d_read", "read failed");
    return MRS_ERR_SENSORS_READ_FAILED;
  }
  data->data_fl[0] = temp;
  data->data_fl[1] = hum;
  return MRS_OK;
}

system_err_t htu21dDeinitialize(void) {
  if (htu21d_initialized) {
    si7021_free_desc(&htu21d_device);
    htu21d_initialized = false;
  }
  return MRS_OK;
}

/* -------------------- Sensor Config Functions -------------------- */

system_err_t SensorConfigInit(void) {
  // Initialize configured sensors; extend to init more devices if needed.
  return MRS_OK;
}

system_err_t SensorConfigRead(SensorData_t *data) {
  if (data == NULL) {
    ESP_LOGE("SensorConfig", "SensorConfigRead: data is NULL");
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  // Read from configured sensors; extend for additional sources if needed.
  return MRS_OK;
}

system_err_t SensorConfigDeinit(void) {
  // Release sensor resources; extend to deinit more drivers if needed.
  return bme280Deinitialize();
}
