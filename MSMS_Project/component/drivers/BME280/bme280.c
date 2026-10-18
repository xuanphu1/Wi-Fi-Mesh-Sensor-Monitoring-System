#include "bme280.h"

__attribute__((unused)) static const char *TAG = " bme280";

static unsigned long max_timeout = 1000;

esp_err_t bme280_init(bme280 *bme280_device, bmp280_params_t *bme280_params,
                      uint8_t address, i2c_port_t i2c_port,
                      gpio_num_t sda_gpio, gpio_num_t scl_gpio)
{
    if (bme280_device == NULL || bme280_params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(bme280_device, 0, sizeof(bme280));

    // First, quickly probe if a device acknowledges this address on the I2C bus
    i2c_cmd_handle_t i2c_command = i2c_cmd_link_create();
    i2c_master_start(i2c_command);
    i2c_master_write_byte(i2c_command, (address << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_stop(i2c_command);
    esp_err_t error_t = i2c_master_cmd_begin(i2c_port, i2c_command, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(i2c_command);

    if (error_t != ESP_OK)
    {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t ret = bmp280_init_default_params(bme280_params);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = bmp280_init_desc(bme280_device, address, i2c_port, sda_gpio, scl_gpio);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = bmp280_init(bme280_device, bme280_params);
    if (ret != ESP_OK) {
        (void)bmp280_free_desc(bme280_device);
        ESP_LOGE(__func__, "I2C port(Wire%d) for BME280 sensor initialize failed.", i2c_port);
        return ESP_ERROR_BME_INIT_FAILED;
    }

    ESP_LOGI(__func__, "BME280 sensor initialize successful.");
    return ESP_OK;
}

esp_err_t bme280_readSensorData(bme280 *bme280_device, float *temperature,
                                float *pressure, float *humidity)
{
    bmp280_read_float(bme280_device, temperature, pressure, humidity);

    if ((*temperature < -40 || *temperature > 85)   ||
        (*pressure < 30000  || *pressure > 110000)  ||
        (*humidity < 0      || *humidity > 100  ) )
    {
        *temperature = TEMPERATURE_VALUE_INVALID;
        *pressure    = PRESSURE_VALUE_INVALID;
        *humidity    = HUMIDITY_VALUUE_INVALID;

        ESP_LOGE(__func__, "BME280 sensor read data failed.");
        return ESP_ERROR_BME_READ_DATA_FAILED;
    } else {
        // ESP_LOGI(__func__, "BME280 sensor read data successful.");
        return ESP_OK;
    }
}
