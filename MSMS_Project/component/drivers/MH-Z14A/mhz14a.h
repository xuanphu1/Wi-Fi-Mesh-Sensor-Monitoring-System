/**
 * @file mhz14a.h
 * @author Nguyen Nhu Hai Long K65 HUST ( @long27032002 )
 * @brief Driver for MH-Z14A CO2 Sensor.
 * @version 1.0
 * @date 2023-03-26
 * 
 * @copyright Copyright (c) 2023
 */

#ifndef __MHZ14A_H__
#define __MHZ14A_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "sdkconfig.h"

/**
 * @brief Pre-defined values for CO2 detection ranges.
 */
typedef enum {
    MHZ14A_RANGE_2000  = 2000,
    MHZ14A_RANGE_5000  = 5000,
    MHZ14A_RANGE_10000 = 10000,
} mhz14a_co2_range_t;

/**
 * @brief Time in milliseconds required for sensor calibration.
 */
#define MHZ14A_TIME_FOR_CALIBRATION_MS  (7000)

/**
 * @brief Default UART configuration for the MH-Z14A sensor.
 *        The sensor operates at 9600 baud, 8 data bits, 1 stop bit, no parity.
 */
#define MHZ14A_UART_CONFIG_DEFAULT()   { \
    .baud_rate = CONFIG_MHZ14A_UART_BAUD_RATE, \
    .data_bits = UART_DATA_8_BITS, \
    .stop_bits = UART_STOP_BITS_1, \
    .parity = UART_PARITY_DISABLE, \
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, \
    .source_clk = UART_SCLK_APB, \
}

/**
 * @brief Default PWM configuration for the MH-Z14A sensor.
 */
#define MHZ14A_PWM_PIN_CONFIG_DEFAULT() { \
    .pin_bit_mask = BIT(CONFIG_MHZ14A_PWM_PIN), \
    .mode = GPIO_MODE_INPUT, \
    .pull_up_en = GPIO_PULLUP_ENABLE, \
    .pull_down_en = GPIO_PULLDOWN_DISABLE, \
    .intr_type = GPIO_INTR_ANYEDGE, \
}

/**
 * @brief Initialize the PWM capture for reading MH-Z14A.
 * 
 * @param pwm_pin GPIO pin number for PWM input.
 * @return ESP_OK on success
 */
esp_err_t mhz14a_init_pwm(int pwm_pin);

/**
 * @brief Read the CO2 concentration from the sensor via PWM.
 * 
 * @param[out] co2_ppm Pointer to store the concentration in ppm.
 * @return ESP_OK on success
 */
esp_err_t mhz14a_get_data_pwm(uint32_t *co2_ppm);

/**
 * @brief Structure to hold the data read from the MH-Z14A sensor.
 */
typedef struct {
    uint32_t co2_ppm;       /*!< CO2 concentration in parts per million (ppm) */
    int8_t   temperature;   /*!< Temperature in degrees Celsius */
} mhz14a_data_t;

/**
 * @brief Initialize the UART port for communicating with the MH-Z14A sensor.
 * 
 * @param uart_num The UART port number to use.
 * @param tx_pin GPIO pin number for UART TX.
 * @param rx_pin GPIO pin number for UART RX.
 * @param uart_config Pointer to the UART configuration structure.
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL if mutex creation fails
 *      - Other esp_err_t codes if UART initialization fails
 */
esp_err_t mhz14a_init_uart(uart_port_t uart_num, int tx_pin, int rx_pin, const uart_config_t *uart_config);

/**
 * @brief Read the CO2 concentration and temperature from the sensor via UART.
 * 
 * @param[out] data Pointer to the structure to hold the sensor data.
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL on null pointer or other errors
 */
esp_err_t mhz14a_get_data_uart(mhz14a_data_t *data);

/**
 * @brief Set the detection range of the sensor.
 * 
 * @param co2_range The desired CO2 detection range (e.g., 2000, 5000, 10000).
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t mhz14a_set_detection_range(mhz14a_co2_range_t co2_range);

/**
 * @brief Perform a zero-point calibration.
 *        Note: Ensure the sensor has been in a 400ppm environment for at least 20 minutes.
 * 
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t mhz14a_zero_point_calibration(void);

/**
 * @brief Perform a span-point calibration.
 *        Note: This is usually done in a controlled environment.
 * 
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t mhz14a_span_point_calibration(void);

/**
 * @brief Enable or disable the self-calibration feature of the sensor.
 * 
 * @param enable True to enable, false to disable.
 * @return 
 *      - ESP_OK on success
 *      - ESP_FAIL on failure
 */
esp_err_t mhz14a_set_self_calibration(bool enable);

#ifdef CONFIG_HD_PIN
/**
 * @brief Trigger auto-calibration using the HD pin.
 *        Pulls the HD pin LOW for 7 seconds to initiate zero-point calibration.
 * 
 * @return ESP_OK on success
 */
esp_err_t mhz14a_auto_calibration_hd_pin(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MHZ14A_H__ */
