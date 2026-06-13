/**
 * @file ErrorCodes.c
 * @brief ErrorCodes helper implementation
 */

#include "ErrorCodes.h"
#include "esp_log.h"

static const char *TAG_ERR = "ErrorCodes";

/* ============================================================================
 * ERROR CODE TO STRING MAPPING
 * ============================================================================ */

/**
 * @brief Map error code to string name
 */
const char *system_err_to_name(system_err_t err)
{
    // ESP-IDF built-in range
    if (err < 0x10000) {
        return esp_err_to_name(err);
    }

    // MRS Project custom errors
    switch (err) {
        /* Core Module */
        case MRS_ERR_CORE_INVALID_PARAM:
            return "MRS_ERR_CORE_INVALID_PARAM";
        case MRS_ERR_CORE_NOT_INITIALIZED:
            return "MRS_ERR_CORE_NOT_INITIALIZED";
        case MRS_ERR_CORE_ALREADY_INIT:
            return "MRS_ERR_CORE_ALREADY_INIT";
        case MRS_ERR_CORE_OUT_OF_MEMORY:
            return "MRS_ERR_CORE_OUT_OF_MEMORY";
        case MRS_ERR_CORE_INVALID_STATE:
            return "MRS_ERR_CORE_INVALID_STATE";
        case MRS_ERR_CORE_NOT_FOUND:
            return "MRS_ERR_CORE_NOT_FOUND";
        case MRS_ERR_CORE_TIMEOUT:
            return "MRS_ERR_CORE_TIMEOUT";

        /* DataManager */
        case MRS_ERR_DATAMANAGER_INVALID_PORT:
            return "MRS_ERR_DATAMANAGER_INVALID_PORT";
        case MRS_ERR_DATAMANAGER_PORT_IN_USE:
            return "MRS_ERR_DATAMANAGER_PORT_IN_USE";
        case MRS_ERR_DATAMANAGER_NO_SENSOR:
            return "MRS_ERR_DATAMANAGER_NO_SENSOR";

        /* FunctionManager */
        case MRS_ERR_FUNCTIONMANAGER_TASK_FAILED:
            return "MRS_ERR_FUNCTIONMANAGER_TASK_FAILED";

        /* Sensors Module */
        case MRS_ERR_SENSORS_INVALID_TYPE:
            return "MRS_ERR_SENSORS_INVALID_TYPE";
        case MRS_ERR_SENSORS_NOT_INITIALIZED:
            return "MRS_ERR_SENSORS_NOT_INITIALIZED";
        case MRS_ERR_SENSORS_INIT_FAILED:
            return "MRS_ERR_SENSORS_INIT_FAILED";
        case MRS_ERR_SENSORS_READ_FAILED:
            return "MRS_ERR_SENSORS_READ_FAILED";
        case MRS_ERR_SENSORS_INVALID_DATA:
            return "MRS_ERR_SENSORS_INVALID_DATA";
        case MRS_ERR_SENSORS_NOT_FOUND:
            return "MRS_ERR_SENSORS_NOT_FOUND";
        case MRS_ERR_SENSORS_REGISTRY_FULL:
            return "MRS_ERR_SENSORS_REGISTRY_FULL";

        /* SensorConfig */
        case MRS_ERR_SENSORCONFIG_WRAPPER_FAILED:
            return "MRS_ERR_SENSORCONFIG_WRAPPER_FAILED";

        /* SensorRegistry */
        case MRS_ERR_SENSORREGISTRY_INVALID_INDEX:
            return "MRS_ERR_SENSORREGISTRY_INVALID_INDEX";

        /* BME280 */
        case MRS_ERR_SENSOR_BME280_INIT_FAILED:
            return "MRS_ERR_SENSOR_BME280_INIT_FAILED";
        case MRS_ERR_SENSOR_BME280_READ_FAILED:
            return "MRS_ERR_SENSOR_BME280_READ_FAILED";

        /* UI Module */
        case MRS_ERR_UI_INVALID_MENU:
            return "MRS_ERR_UI_INVALID_MENU";
        case MRS_ERR_UI_INVALID_ITEM:
            return "MRS_ERR_UI_INVALID_ITEM";
        case MRS_ERR_UI_RENDER_FAILED:
            return "MRS_ERR_UI_RENDER_FAILED";
        case MRS_ERR_UI_INVALID_BUTTON:
            return "MRS_ERR_UI_INVALID_BUTTON";

        /* MenuSystem */
        case MRS_ERR_MENUSYSTEM_INVALID_CALLBACK:
            return "MRS_ERR_MENUSYSTEM_INVALID_CALLBACK";
        case MRS_ERR_MENUSYSTEM_NAVIGATION_FAILED:
            return "MRS_ERR_MENUSYSTEM_NAVIGATION_FAILED";

        /* ScreenManager */
        case MRS_ERR_SCREENMANAGER_NOT_INIT:
            return "MRS_ERR_SCREENMANAGER_NOT_INIT";
        case MRS_ERR_SCREENMANAGER_DISPLAY_FAIL:
            return "MRS_ERR_SCREENMANAGER_DISPLAY_FAIL";

        /* ButtonManager */
        case MRS_ERR_BUTTONMANAGER_GPIO_FAILED:
            return "MRS_ERR_BUTTONMANAGER_GPIO_FAILED";

        /* Network Module */
        case MRS_ERR_NETWORK_NOT_INITIALIZED:
            return "MRS_ERR_NETWORK_NOT_INITIALIZED";
        case MRS_ERR_NETWORK_CONNECTION_FAILED:
            return "MRS_ERR_NETWORK_CONNECTION_FAILED";
        case MRS_ERR_NETWORK_TIMEOUT:
            return "MRS_ERR_NETWORK_TIMEOUT";
        case MRS_ERR_NETWORK_INVALID_CONFIG:
            return "MRS_ERR_NETWORK_INVALID_CONFIG";

        /* WifiManager */
        case MRS_ERR_WIFIMANAGER_INIT_FAILED:
            return "MRS_ERR_WIFIMANAGER_INIT_FAILED";
        case MRS_ERR_WIFIMANAGER_STA_FAILED:
            return "MRS_ERR_WIFIMANAGER_STA_FAILED";
        case MRS_ERR_WIFIMANAGER_AP_FAILED:
            return "MRS_ERR_WIFIMANAGER_AP_FAILED";
        case MRS_ERR_WIFIMANAGER_INVALID_SSID:
            return "MRS_ERR_WIFIMANAGER_INVALID_SSID";
        case MRS_ERR_WIFIMANAGER_INVALID_PASS:
            return "MRS_ERR_WIFIMANAGER_INVALID_PASS";

        /* MeshManager */
        case MRS_ERR_MESHMANAGER_INIT_FAILED:
            return "MRS_ERR_MESHMANAGER_INIT_FAILED";
        case MRS_ERR_MESHMANAGER_NOT_SUPPORTED:
            return "MRS_ERR_MESHMANAGER_NOT_SUPPORTED";

        /* Drivers Module */
        case MRS_ERR_DRIVERS_INIT_FAILED:
            return "MRS_ERR_DRIVERS_INIT_FAILED";
        case MRS_ERR_DRIVERS_NOT_INITIALIZED:
            return "MRS_ERR_DRIVERS_NOT_INITIALIZED";
        case MRS_ERR_DRIVERS_INVALID_PARAM:
            return "MRS_ERR_DRIVERS_INVALID_PARAM";
        case MRS_ERR_DRIVERS_COMM_FAILED:
            return "MRS_ERR_DRIVERS_COMM_FAILED";

        /* I2C */
        case MRS_ERR_I2CDEV_INIT_FAILED:
            return "MRS_ERR_I2CDEV_INIT_FAILED";
        case MRS_ERR_I2CDEV_BUSY:
            return "MRS_ERR_I2CDEV_BUSY";
        case MRS_ERR_I2CDEV_NACK:
            return "MRS_ERR_I2CDEV_NACK";
        case MRS_ERR_I2CDEV_TIMEOUT:
            return "MRS_ERR_I2CDEV_TIMEOUT";

        /* SSD1306 */
        case MRS_ERR_SSD1306_INIT_FAILED:
            return "MRS_ERR_SSD1306_INIT_FAILED";
        case MRS_ERR_SSD1306_DISPLAY_FAILED:
            return "MRS_ERR_SSD1306_DISPLAY_FAILED";

        /* DS3231 */
        case MRS_ERR_DS3231_INIT_FAILED:
            return "MRS_ERR_DS3231_INIT_FAILED";
        case MRS_ERR_DS3231_READ_FAILED:
            return "MRS_ERR_DS3231_READ_FAILED";
        case MRS_ERR_DS3231_WRITE_FAILED:
            return "MRS_ERR_DS3231_WRITE_FAILED";

        /* LED RGB */
        case MRS_ERR_LEDRGB_INIT_FAILED:
            return "MRS_ERR_LEDRGB_INIT_FAILED";
        case MRS_ERR_LEDRGB_RMT_FAILED:
            return "MRS_ERR_LEDRGB_RMT_FAILED";

        /* Utils Module */
        case MRS_ERR_UTILS_INVALID_OPERATION:
            return "MRS_ERR_UTILS_INVALID_OPERATION";

        default:
            return "MRS_ERR_UNKNOWN";
    }
}

/**
 * @brief Whether err belongs to module_id
 */
bool system_err_is_module(system_err_t err, uint8_t module_id)
{
    if (err < 0x10000) {
        return false;  // Standard ESP-IDF errors are not MRS module-scoped
    }
    
    uint8_t err_module = (err >> 12) & 0xFF;
    return (err_module == module_id);
}

/**
 * @brief Extract module ID from composite error
 */
uint8_t system_err_get_module(system_err_t err)
{
    if (err < 0x10000) {
        return 0;  // Standard ESP-IDF: no module field
    }
    
    return (err >> 12) & 0xFF;
}

/**
 * @brief 12-bit code within module (or raw ESP err)
 */
uint16_t system_err_get_code(system_err_t err)
{
    if (err < 0x10000) {
        return err;  // ESP-IDF standard errors
    }
    
    return err & 0xFFF;  // 12-bit error code
}

void ErrorCodes_PushError(uint16_t *error_buffer, size_t capacity, system_err_t err)
{
    if (error_buffer == NULL || capacity == 0 || err == MRS_OK) {
        return;
    }

    uint16_t code = (uint16_t)err;
    for (size_t i = 0; i < capacity; i++) {
        if (error_buffer[i] == 0U) {
            error_buffer[i] = code;
            ESP_LOGI(TAG_ERR, "Push error: 0x%04X (slot %u/%u)",
                     (unsigned)code, (unsigned)(i + 1), (unsigned)capacity);
            return;
        }
    }

    for (size_t i = 1; i < capacity; i++) {
        error_buffer[i - 1] = error_buffer[i];
    }
    error_buffer[capacity - 1] = code;
    ESP_LOGI(TAG_ERR, "Push error: 0x%04X (buffer full, rotate left)", (unsigned)code);
}


