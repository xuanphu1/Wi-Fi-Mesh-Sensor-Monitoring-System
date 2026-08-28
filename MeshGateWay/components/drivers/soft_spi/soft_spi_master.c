#include "soft_spi_master.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "freertos/semphr.h"

typedef struct {
    soft_spi_bus_config_t config;
    SemaphoreHandle_t mutex;
    unsigned device_count;
} soft_spi_bus_t;

typedef struct {
    soft_spi_bus_t *bus;
    soft_spi_device_interface_config_t config;
} soft_spi_device_t;

static const char *TAG = "soft_spi";
static soft_spi_bus_t *s_buses[SOFT_SPI_HOST_MAX];

static bool host_valid(soft_spi_host_device_t host)
{
    return host == SOFT_SPI2_HOST || host == SOFT_SPI3_HOST;
}

static inline void half_cycle_delay(const soft_spi_device_t *dev)
{
    uint32_t us = 500000U / (uint32_t)dev->config.clock_speed_hz;
    if (us) esp_rom_delay_us(us);
}

static inline bool tx_lsb_first(const soft_spi_device_t *dev)
{
    return (dev->config.flags & (SOFT_SPI_DEVICE_BIT_LSBFIRST |
                                 SOFT_SPI_DEVICE_TXBIT_LSBFIRST)) != 0;
}

static inline bool rx_lsb_first(const soft_spi_device_t *dev)
{
    return (dev->config.flags & (SOFT_SPI_DEVICE_BIT_LSBFIRST |
                                 SOFT_SPI_DEVICE_RXBIT_LSBFIRST)) != 0;
}

static int buffer_get_bit(const uint8_t *buffer, size_t bit, bool lsb_first)
{
    unsigned position = lsb_first ? bit % 8 : 7 - bit % 8;
    return (buffer[bit / 8] >> position) & 1U;
}

static void buffer_set_bit(uint8_t *buffer, size_t bit, bool lsb_first, int value)
{
    unsigned position = lsb_first ? bit % 8 : 7 - bit % 8;
    if (value) buffer[bit / 8] |= 1U << position;
}

static int integer_get_bit(uint64_t value, size_t bit_count, size_t bit,
                           bool lsb_first)
{
    size_t position = lsb_first ? bit : bit_count - 1 - bit;
    return (value >> position) & 1U;
}

static void set_data_direction(const soft_spi_device_t *dev, bool output)
{
    if (!(dev->config.flags & SOFT_SPI_DEVICE_3WIRE)) return;
    gpio_set_direction(dev->bus->config.mosi_io_num,
                       output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
}

static int clock_bit(const soft_spi_device_t *dev, int tx_bit, bool drive_tx)
{
    const bool cpol = (dev->config.mode & 2U) != 0;
    const bool cpha = (dev->config.mode & 1U) != 0;
    int data_pin = (dev->config.flags & SOFT_SPI_DEVICE_3WIRE) ?
                   dev->bus->config.mosi_io_num : dev->bus->config.miso_io_num;

    if (!cpha && drive_tx && dev->bus->config.mosi_io_num >= 0)
        gpio_set_level(dev->bus->config.mosi_io_num, tx_bit);
    if (cpha) {
        gpio_set_level(dev->bus->config.sclk_io_num, !cpol);
        if (drive_tx && dev->bus->config.mosi_io_num >= 0)
            gpio_set_level(dev->bus->config.mosi_io_num, tx_bit);
        half_cycle_delay(dev);
        gpio_set_level(dev->bus->config.sclk_io_num, cpol);
    } else {
        half_cycle_delay(dev);
        gpio_set_level(dev->bus->config.sclk_io_num, !cpol);
    }
    int received = data_pin >= 0 ? gpio_get_level(data_pin) : 0;
    half_cycle_delay(dev);
    if (!cpha) gpio_set_level(dev->bus->config.sclk_io_num, cpol);
    return received;
}

static void send_integer(const soft_spi_device_t *dev, uint64_t value,
                         size_t bits)
{
    for (size_t i = 0; i < bits; ++i)
        clock_bit(dev, integer_get_bit(value, bits, i, tx_lsb_first(dev)), true);
}

static void clock_buffers(const soft_spi_device_t *dev, const uint8_t *tx,
                          size_t tx_bits, uint8_t *rx, size_t rx_bits,
                          bool simultaneous)
{
    size_t clocks = simultaneous ? (tx_bits > rx_bits ? tx_bits : rx_bits) : tx_bits;
    for (size_t i = 0; i < clocks; ++i) {
        int tx_bit = tx && i < tx_bits ? buffer_get_bit(tx, i, tx_lsb_first(dev)) : 1;
        int rx_bit = clock_bit(dev, tx_bit, i < tx_bits);
        if (simultaneous && rx && i < rx_bits)
            buffer_set_bit(rx, i, rx_lsb_first(dev), rx_bit);
    }
}

static esp_err_t validate_transaction(const soft_spi_device_t *dev,
                                      const soft_spi_transaction_t *trans)
{
    ESP_RETURN_ON_FALSE(trans, ESP_ERR_INVALID_ARG, TAG, "transaction required");
    ESP_RETURN_ON_FALSE(!((trans->flags & SOFT_SPI_TRANS_USE_TXDATA) &&
                          trans->tx_buffer), ESP_ERR_INVALID_ARG, TAG,
                        "choose tx_data or tx_buffer");
    ESP_RETURN_ON_FALSE(!((trans->flags & SOFT_SPI_TRANS_USE_RXDATA) &&
                          trans->rx_buffer), ESP_ERR_INVALID_ARG, TAG,
                        "choose rx_data or rx_buffer");
    ESP_RETURN_ON_FALSE(!(trans->flags & SOFT_SPI_TRANS_USE_TXDATA) ||
                        trans->length <= 32, ESP_ERR_INVALID_ARG, TAG,
                        "tx_data supports at most 32 bits");
    size_t rx_bits = trans->rxlength ? trans->rxlength : trans->length;
    ESP_RETURN_ON_FALSE(!(trans->flags & SOFT_SPI_TRANS_USE_RXDATA) ||
                        rx_bits <= 32, ESP_ERR_INVALID_ARG, TAG,
                        "rx_data supports at most 32 bits");
    size_t bytes = ((trans->length > rx_bits ? trans->length : rx_bits) + 7) / 8;
    ESP_RETURN_ON_FALSE(dev->bus->config.max_transfer_sz <= 0 ||
                        bytes <= (size_t)dev->bus->config.max_transfer_sz,
                        ESP_ERR_INVALID_SIZE, TAG, "transaction too large");
    return ESP_OK;
}

esp_err_t soft_spi_bus_initialize(soft_spi_host_device_t host_id,
                                  const soft_spi_bus_config_t *cfg, int dma_chan)
{
    ESP_RETURN_ON_FALSE(host_valid(host_id) && cfg, ESP_ERR_INVALID_ARG, TAG,
                        "invalid host or config");
    ESP_RETURN_ON_FALSE(dma_chan == 0, ESP_ERR_NOT_SUPPORTED, TAG,
                        "software SPI does not use DMA");
    ESP_RETURN_ON_FALSE(!s_buses[host_id], ESP_ERR_INVALID_STATE, TAG,
                        "bus already initialized");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(cfg->sclk_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid SCLK GPIO");
    ESP_RETURN_ON_FALSE(cfg->mosi_io_num < 0 || GPIO_IS_VALID_OUTPUT_GPIO(cfg->mosi_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid MOSI GPIO");
    ESP_RETURN_ON_FALSE(cfg->miso_io_num < 0 || GPIO_IS_VALID_GPIO(cfg->miso_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid MISO GPIO");

    soft_spi_bus_t *bus = calloc(1, sizeof(*bus));
    ESP_RETURN_ON_FALSE(bus, ESP_ERR_NO_MEM, TAG, "no memory for bus");
    bus->config = *cfg;
    bus->mutex = xSemaphoreCreateRecursiveMutex();
    if (!bus->mutex) { free(bus); return ESP_ERR_NO_MEM; }

    gpio_config_t out = {
        .pin_bit_mask = (1ULL << cfg->sclk_io_num) |
                        (cfg->mosi_io_num < 0 ? 0 : (1ULL << cfg->mosi_io_num)),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&out);
    if (err == ESP_OK && cfg->miso_io_num >= 0) {
        gpio_config_t in = {
            .pin_bit_mask = 1ULL << cfg->miso_io_num,
            .mode = GPIO_MODE_INPUT,
            .intr_type = GPIO_INTR_DISABLE,
        };
        err = gpio_config(&in);
    }
    if (err != ESP_OK) { vSemaphoreDelete(bus->mutex); free(bus); return err; }
    gpio_set_level(cfg->sclk_io_num, 0);
    s_buses[host_id] = bus;
    return ESP_OK;
}

esp_err_t soft_spi_bus_free(soft_spi_host_device_t host_id)
{
    ESP_RETURN_ON_FALSE(host_valid(host_id) && s_buses[host_id],
                        ESP_ERR_INVALID_STATE, TAG, "bus not initialized");
    soft_spi_bus_t *bus = s_buses[host_id];
    ESP_RETURN_ON_FALSE(bus->device_count == 0, ESP_ERR_INVALID_STATE, TAG,
                        "remove all devices first");
    gpio_reset_pin(bus->config.sclk_io_num);
    if (bus->config.mosi_io_num >= 0) gpio_reset_pin(bus->config.mosi_io_num);
    if (bus->config.miso_io_num >= 0) gpio_reset_pin(bus->config.miso_io_num);
    vSemaphoreDelete(bus->mutex);
    free(bus);
    s_buses[host_id] = NULL;
    return ESP_OK;
}

esp_err_t soft_spi_bus_add_device(soft_spi_host_device_t host_id,
                                  const soft_spi_device_interface_config_t *cfg,
                                  soft_spi_device_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(host_valid(host_id) && s_buses[host_id] && cfg && handle,
                        ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(cfg->mode <= 3 && cfg->clock_speed_hz > 0,
                        ESP_ERR_INVALID_ARG, TAG, "invalid mode or clock");
    ESP_RETURN_ON_FALSE(cfg->command_bits <= 16 && cfg->address_bits <= 64,
                        ESP_ERR_INVALID_ARG, TAG, "command/address is too wide");
    ESP_RETURN_ON_FALSE(cfg->spics_io_num < 0 ||
                        GPIO_IS_VALID_OUTPUT_GPIO(cfg->spics_io_num),
                        ESP_ERR_INVALID_ARG, TAG, "invalid CS GPIO");
    ESP_RETURN_ON_FALSE(!(cfg->flags & SOFT_SPI_DEVICE_3WIRE) ||
                        s_buses[host_id]->config.mosi_io_num >= 0,
                        ESP_ERR_INVALID_ARG, TAG, "3-wire requires MOSI pin");

    soft_spi_device_t *dev = calloc(1, sizeof(*dev));
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_NO_MEM, TAG, "no memory for device");
    dev->bus = s_buses[host_id];
    dev->config = *cfg;
    if (cfg->spics_io_num >= 0) {
        gpio_config_t cs = {
            .pin_bit_mask = 1ULL << cfg->spics_io_num,
            .mode = GPIO_MODE_OUTPUT,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&cs);
        if (err != ESP_OK) { free(dev); return err; }
        gpio_set_level(cfg->spics_io_num,
                       !(cfg->flags & SOFT_SPI_DEVICE_POSITIVE_CS));
    }
    dev->bus->device_count++;
    *handle = dev;
    return ESP_OK;
}

esp_err_t soft_spi_bus_remove_device(soft_spi_device_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "device required");
    soft_spi_device_t *dev = handle;
    xSemaphoreTakeRecursive(dev->bus->mutex, portMAX_DELAY);
    if (dev->config.spics_io_num >= 0) gpio_reset_pin(dev->config.spics_io_num);
    dev->bus->device_count--;
    soft_spi_bus_t *bus = dev->bus;
    free(dev);
    xSemaphoreGiveRecursive(bus->mutex);
    return ESP_OK;
}

esp_err_t soft_spi_device_polling_transmit(soft_spi_device_handle_t handle,
                                           soft_spi_transaction_t *trans)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "device required");
    soft_spi_device_t *dev = handle;
    ESP_RETURN_ON_ERROR(validate_transaction(dev, trans), TAG, "invalid transaction");
    if (xSemaphoreTakeRecursive(dev->bus->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_ERR_TIMEOUT;

    const bool positive_cs = (dev->config.flags & SOFT_SPI_DEVICE_POSITIVE_CS) != 0;
    const bool half_duplex = (dev->config.flags & SOFT_SPI_DEVICE_HALFDUPLEX) != 0;
    const uint8_t *tx = (trans->flags & SOFT_SPI_TRANS_USE_TXDATA) ?
                        trans->tx_data : trans->tx_buffer;
    uint8_t *rx = (trans->flags & SOFT_SPI_TRANS_USE_RXDATA) ?
                  trans->rx_data : trans->rx_buffer;
    size_t rx_bits = trans->rxlength ? trans->rxlength : trans->length;
    if (rx) memset(rx, 0, (rx_bits + 7) / 8);

    gpio_set_level(dev->bus->config.sclk_io_num, (dev->config.mode & 2U) != 0);
    if (dev->config.spics_io_num >= 0)
        gpio_set_level(dev->config.spics_io_num, positive_cs);
    for (unsigned i = 0; i < dev->config.cs_ena_pretrans; ++i) {
        half_cycle_delay(dev); half_cycle_delay(dev);
    }
    if (dev->config.pre_cb) dev->config.pre_cb(trans);

    set_data_direction(dev, true);
    send_integer(dev, trans->cmd, dev->config.command_bits);
    send_integer(dev, trans->addr, dev->config.address_bits);
    if (half_duplex) {
        clock_buffers(dev, tx, trans->length, NULL, 0, false);
        if (!(dev->config.flags & SOFT_SPI_DEVICE_NO_DUMMY))
            for (unsigned i = 0; i < dev->config.dummy_bits; ++i) clock_bit(dev, 1, true);
        set_data_direction(dev, false);
        clock_buffers(dev, NULL, 0, rx, rx_bits, true);
        set_data_direction(dev, true);
    } else {
        clock_buffers(dev, tx, trans->length, rx, rx_bits, true);
    }

    if (dev->config.post_cb) dev->config.post_cb(trans);
    for (unsigned i = 0; i < dev->config.cs_ena_posttrans; ++i) {
        half_cycle_delay(dev); half_cycle_delay(dev);
    }
    if (dev->config.spics_io_num >= 0)
        gpio_set_level(dev->config.spics_io_num, !positive_cs);
    xSemaphoreGiveRecursive(dev->bus->mutex);
    return ESP_OK;
}

esp_err_t soft_spi_device_transmit(soft_spi_device_handle_t handle,
                                   soft_spi_transaction_t *trans)
{
    return soft_spi_device_polling_transmit(handle, trans);
}

esp_err_t soft_spi_device_acquire_bus(soft_spi_device_handle_t handle, TickType_t wait)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "device required");
    soft_spi_device_t *dev = handle;
    return xSemaphoreTakeRecursive(dev->bus->mutex, wait) == pdTRUE ?
           ESP_OK : ESP_ERR_TIMEOUT;
}

void soft_spi_device_release_bus(soft_spi_device_handle_t handle)
{
    if (handle) xSemaphoreGiveRecursive(((soft_spi_device_t *)handle)->bus->mutex);
}

esp_err_t soft_spi_device_queue_trans(soft_spi_device_handle_t handle,
                                      soft_spi_transaction_t *trans, TickType_t wait)
{
    (void)handle; (void)trans; (void)wait;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t soft_spi_device_get_trans_result(soft_spi_device_handle_t handle,
                                           soft_spi_transaction_t **trans, TickType_t wait)
{
    (void)handle; (void)trans; (void)wait;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t soft_spi_device_polling_start(soft_spi_device_handle_t handle,
                                        soft_spi_transaction_t *trans, TickType_t wait)
{
    (void)wait;
    return soft_spi_device_polling_transmit(handle, trans);
}

esp_err_t soft_spi_device_polling_end(soft_spi_device_handle_t handle, TickType_t wait)
{
    (void)wait;
    return handle ? ESP_OK : ESP_ERR_INVALID_ARG;
}
