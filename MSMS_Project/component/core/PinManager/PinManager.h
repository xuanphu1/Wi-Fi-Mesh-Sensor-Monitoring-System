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
#include "driver/spi_master.h"

/* UART — "PinManager — UART" (port selected via PIN_UART_NUM, not hardwired UART2) */
#define PIN_UART_TX ((gpio_num_t)CONFIG_PIN_UART_TX)
#define PIN_UART_RX ((gpio_num_t)CONFIG_PIN_UART_RX)
#define PIN_UART_NUM ((uart_port_t)CONFIG_PIN_UART_NUM)
#define PIN_UART_GATEWAY_BAUD_RATE CONFIG_UART_GATEWAY_BAUD_RATE
#define PIN_UART_PERIPHERAL_BAUD_RATE CONFIG_UART_PERIPHERAL_BAUD_RATE

/* I2C — menu "PinManager — I2C" */
#define PIN_I2C_SDA ((gpio_num_t)CONFIG_PIN_I2C_SDA)
#define PIN_I2C_SCL ((gpio_num_t)CONFIG_PIN_I2C_SCL)
#define PIN_I2C_PORT_NUM ((i2c_port_t)CONFIG_PIN_I2C_PORT)
#define PIN_I2C_CLK_HZ CONFIG_PIN_I2C_CLK_HZ

/* SPI — menu "PinManager — SPI" */
#define PIN_SPI_HOST ((spi_host_device_t)CONFIG_PIN_SPI_HOST)
#define PIN_SPI_MISO ((gpio_num_t)CONFIG_PIN_SPI_MISO)
#define PIN_SPI_MOSI ((gpio_num_t)CONFIG_PIN_SPI_MOSI)
#define PIN_SPI_SCLK ((gpio_num_t)CONFIG_PIN_SPI_SCLK)
#define PIN_SPI_CS ((gpio_num_t)CONFIG_PIN_SPI_CS)

/* Pulse — menu "PinManager — Pulse" */
#define PIN_PULSE_1 ((gpio_num_t)CONFIG_PIN_PULSE_1)
#define PIN_PULSE_2 ((gpio_num_t)CONFIG_PIN_PULSE_2)

#endif /* PIN_MANAGER_H */
