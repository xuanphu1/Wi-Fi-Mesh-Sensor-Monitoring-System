/**
 * @file PinManager.h
 * @brief UART and I2C pins — PinManager Kconfig.
 */
#ifndef PIN_MANAGER_H
#define PIN_MANAGER_H

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"

/* UART — "PinManager — UART" (port selected via PIN_UART_NUM, not hardwired UART2) */
#define PIN_UART_TX ((gpio_num_t)CONFIG_PIN_UART_TX)
#define PIN_UART_RX ((gpio_num_t)CONFIG_PIN_UART_RX)
#define PIN_UART_NUM ((uart_port_t)CONFIG_PIN_UART_NUM)

/* I2C — menu "PinManager — I2C" */
#define PIN_I2C_SDA ((gpio_num_t)CONFIG_PIN_I2C_SDA)
#define PIN_I2C_SCL ((gpio_num_t)CONFIG_PIN_I2C_SCL)
#define PIN_I2C_PORT_NUM ((i2c_port_t)CONFIG_PIN_I2C_PORT)
#define PIN_I2C_CLK_HZ CONFIG_PIN_I2C_CLK_HZ

#endif /* PIN_MANAGER_H */
