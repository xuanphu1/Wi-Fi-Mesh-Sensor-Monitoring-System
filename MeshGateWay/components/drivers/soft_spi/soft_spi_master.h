#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_bit_defs.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *soft_spi_device_handle_t;

typedef enum {
    SOFT_SPI2_HOST = 1,
    SOFT_SPI3_HOST = 2,
    SOFT_SPI_HOST_MAX,
} soft_spi_host_device_t;

typedef struct {
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int max_transfer_sz;
    uint32_t flags;
    int intr_flags;
} soft_spi_bus_config_t;

struct soft_spi_transaction_t;
typedef void (*soft_spi_transaction_cb_t)(struct soft_spi_transaction_t *trans);

typedef struct {
    uint8_t command_bits;
    uint8_t address_bits;
    uint8_t dummy_bits;
    uint8_t mode;
    uint16_t duty_cycle_pos;
    uint16_t cs_ena_pretrans;
    uint8_t cs_ena_posttrans;
    int clock_speed_hz;
    int input_delay_ns;
    int spics_io_num;
    uint32_t flags;
    int queue_size;
    soft_spi_transaction_cb_t pre_cb;
    soft_spi_transaction_cb_t post_cb;
} soft_spi_device_interface_config_t;

typedef struct soft_spi_transaction_t {
    uint32_t flags;
    uint16_t cmd;
    uint64_t addr;
    size_t length;       /* TX length in bits. */
    size_t rxlength;     /* RX length in bits; 0 means length. */
    void *user;
    const void *tx_buffer;
    void *rx_buffer;
    uint8_t tx_data[4];
    uint8_t rx_data[4];
} soft_spi_transaction_t;

#define SOFT_SPI_TRANS_USE_RXDATA          BIT(2)
#define SOFT_SPI_TRANS_USE_TXDATA          BIT(3)

#define SOFT_SPI_DEVICE_HALFDUPLEX          BIT(0)
#define SOFT_SPI_DEVICE_3WIRE               BIT(1)
#define SOFT_SPI_DEVICE_POSITIVE_CS         BIT(2)
#define SOFT_SPI_DEVICE_BIT_LSBFIRST        BIT(3)
#define SOFT_SPI_DEVICE_TXBIT_LSBFIRST      BIT(4)
#define SOFT_SPI_DEVICE_RXBIT_LSBFIRST      BIT(5)
#define SOFT_SPI_DEVICE_NO_DUMMY            BIT(6)

esp_err_t soft_spi_bus_initialize(soft_spi_host_device_t host_id,
                                  const soft_spi_bus_config_t *bus_config,
                                  int dma_chan);
esp_err_t soft_spi_bus_free(soft_spi_host_device_t host_id);
esp_err_t soft_spi_bus_add_device(soft_spi_host_device_t host_id,
                                  const soft_spi_device_interface_config_t *dev_config,
                                  soft_spi_device_handle_t *handle);
esp_err_t soft_spi_bus_remove_device(soft_spi_device_handle_t handle);
esp_err_t soft_spi_device_transmit(soft_spi_device_handle_t handle,
                                   soft_spi_transaction_t *trans_desc);
esp_err_t soft_spi_device_polling_transmit(soft_spi_device_handle_t handle,
                                           soft_spi_transaction_t *trans_desc);
esp_err_t soft_spi_device_acquire_bus(soft_spi_device_handle_t device,
                                      TickType_t wait);
void soft_spi_device_release_bus(soft_spi_device_handle_t dev);
esp_err_t soft_spi_device_queue_trans(soft_spi_device_handle_t handle,
                                      soft_spi_transaction_t *trans_desc,
                                      TickType_t ticks_to_wait);
esp_err_t soft_spi_device_get_trans_result(soft_spi_device_handle_t handle,
                                           soft_spi_transaction_t **trans_desc,
                                           TickType_t ticks_to_wait);
esp_err_t soft_spi_device_polling_start(soft_spi_device_handle_t handle,
                                        soft_spi_transaction_t *trans_desc,
                                        TickType_t ticks_to_wait);
esp_err_t soft_spi_device_polling_end(soft_spi_device_handle_t handle,
                                      TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif
