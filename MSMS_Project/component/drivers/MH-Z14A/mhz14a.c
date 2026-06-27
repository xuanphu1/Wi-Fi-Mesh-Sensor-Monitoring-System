#include "mhz14a.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "soc/rtc.h"

static const char *TAG = "MHZ14a";

#define MHZ14A_UART_CMD_GET_CONCENTRATION                   0x86
#define MHZ14A_UART_CMD_CALIBRATION_ZERO_POINT              0x87
#define MHZ14A_UART_CMD_CALIBRATION_SPAN_POINT              0x88
#define MHZ14A_UART_CMD_SET_SELF_CALIBRATION_ZERO_POINT     0x79
#define MHZ14A_UART_CMD_ON_SELF_CALIBRATION_ZERO_POINT      0xA0
#define MHZ14A_UART_CMD_OFF_SELF_CALIBRATION_ZERO_POINT     0x00
#define MHZ14A_UART_CMD_DETECTION_RANGE_SETTING             0x99

#define UART_DATA_RECEIVE_START_CHARACTER_1                 0xFF
#define UART_DATA_RECEIVE_START_CHARACTER_2                 0x86

#define MHZ14A_UART_RX_BUFFER_SIZE                          256

static const uint8_t mhz14a_command_template[9] = {0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

static SemaphoreHandle_t mhz14a_uart_mutex = NULL;
static uart_port_t mhz14a_uart_num = UART_NUM_MAX;

static uint8_t mhz14a_get_checksum(const uint8_t *packet)
{
    uint8_t checksum = 0;
    for (uint8_t i = 1; i < 8; i++) {
        checksum += packet[i];
    }
    checksum = 0xFF - checksum;
    checksum += 1;
    return checksum;
}

static esp_err_t mhz14a_send_command(const uint32_t uart_command, bool on_off_function)
{
    uint8_t array_cmd[9] = {0};
    memcpy(array_cmd, mhz14a_command_template, sizeof(mhz14a_command_template));
    
    array_cmd[2] = uart_command;
    if (uart_command == MHZ14A_UART_CMD_SET_SELF_CALIBRATION_ZERO_POINT) {
        array_cmd[3] = on_off_function ? MHZ14A_UART_CMD_ON_SELF_CALIBRATION_ZERO_POINT : MHZ14A_UART_CMD_OFF_SELF_CALIBRATION_ZERO_POINT;
    }
    array_cmd[8] = mhz14a_get_checksum(array_cmd);

    if (mhz14a_uart_num == UART_NUM_MAX) return ESP_FAIL;

    int len = uart_write_bytes(mhz14a_uart_num, (const char*)array_cmd, sizeof(array_cmd));
    if (len < 0) {
        ESP_LOGE(TAG, "Sending command to MHZ14a sensor failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t mhz14a_init_uart(uart_port_t uart_num, int tx_pin, int rx_pin, const uart_config_t *uart_config)
{
    esp_err_t err;
    
    if (uart_is_driver_installed(uart_num)) {
        uart_driver_delete(uart_num);
    }
    err = uart_driver_install(uart_num, MHZ14A_UART_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;

    err = uart_param_config(uart_num, uart_config);
    if (err != ESP_OK) return err;

    err = uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    mhz14a_uart_num = uart_num;

    if (mhz14a_uart_mutex == NULL) {
        mhz14a_uart_mutex = xSemaphoreCreateMutex();
        if (mhz14a_uart_mutex == NULL) {
            ESP_LOGE(TAG, "Mutex creation failed");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "MHZ14A UART port initialized successfully");
    return ESP_OK;
}

esp_err_t mhz14a_get_data_uart(mhz14a_data_t *data)
{
    if (data == NULL || mhz14a_uart_mutex == NULL) return ESP_FAIL;

    data->co2_ppm = 0;
    data->temperature = 0;

    if (mhz14a_uart_num == UART_NUM_MAX) return ESP_FAIL;

    xSemaphoreTake(mhz14a_uart_mutex, portMAX_DELAY);
    uart_flush_input(mhz14a_uart_num);

    if (mhz14a_send_command(MHZ14A_UART_CMD_GET_CONCENTRATION, false) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send read command");
        xSemaphoreGive(mhz14a_uart_mutex);
        return ESP_FAIL;
    }

    uint8_t raw_data[9] = {0};
    int len = uart_read_bytes(mhz14a_uart_num, raw_data, sizeof(raw_data), pdMS_TO_TICKS(500));
    xSemaphoreGive(mhz14a_uart_mutex);

    if (len != 9) {
        ESP_LOGE(TAG, "Read data timeout or incomplete length: %d", len);
        return ESP_ERR_TIMEOUT;
    }

    if (raw_data[0] == UART_DATA_RECEIVE_START_CHARACTER_1 &&
        raw_data[1] == UART_DATA_RECEIVE_START_CHARACTER_2 &&
        raw_data[8] == mhz14a_get_checksum(raw_data)) 
    {
        data->co2_ppm = (raw_data[2] << 8) | raw_data[3];
        data->temperature = raw_data[4] - 40;
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Checksum failed or invalid start bytes");
        return ESP_ERR_INVALID_RESPONSE;
    }
}

esp_err_t mhz14a_set_detection_range(mhz14a_co2_range_t co2_range)
{
    if (mhz14a_uart_mutex == NULL) return ESP_FAIL;

    uint8_t array_cmd[9] = {0};
    memcpy(array_cmd, mhz14a_command_template, sizeof(mhz14a_command_template));
    
    array_cmd[2] = MHZ14A_UART_CMD_DETECTION_RANGE_SETTING;
    array_cmd[6] = (co2_range >> 8) & 0xFF;
    array_cmd[7] = co2_range & 0xFF;
    array_cmd[8] = mhz14a_get_checksum(array_cmd);

    if (mhz14a_uart_num == UART_NUM_MAX) return ESP_FAIL;

    xSemaphoreTake(mhz14a_uart_mutex, portMAX_DELAY);
    int len = uart_write_bytes(mhz14a_uart_num, (const char*)array_cmd, sizeof(array_cmd));
    xSemaphoreGive(mhz14a_uart_mutex);

    if (len < 0) {
        ESP_LOGE(TAG, "Failed to send set range command");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Set detection range command sent successfully");
    return ESP_OK;
}

esp_err_t mhz14a_zero_point_calibration(void)
{
    if (mhz14a_uart_mutex == NULL) return ESP_FAIL;

    xSemaphoreTake(mhz14a_uart_mutex, portMAX_DELAY);
    esp_err_t err = mhz14a_send_command(MHZ14A_UART_CMD_CALIBRATION_ZERO_POINT, false);
    xSemaphoreGive(mhz14a_uart_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send zero point calibration command");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Zero point calibration started. Waiting 7 seconds...");
    vTaskDelay(pdMS_TO_TICKS(MHZ14A_TIME_FOR_CALIBRATION_MS));
    ESP_LOGI(TAG, "Zero point calibration finished.");
    
    return ESP_OK;
}

esp_err_t mhz14a_span_point_calibration(void)
{
    if (mhz14a_uart_mutex == NULL) return ESP_FAIL;

    xSemaphoreTake(mhz14a_uart_mutex, portMAX_DELAY);
    esp_err_t err = mhz14a_send_command(MHZ14A_UART_CMD_CALIBRATION_SPAN_POINT, false);
    xSemaphoreGive(mhz14a_uart_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send span point calibration command");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Span point calibration started. Waiting 7 seconds...");
    vTaskDelay(pdMS_TO_TICKS(MHZ14A_TIME_FOR_CALIBRATION_MS));
    ESP_LOGI(TAG, "Span point calibration finished.");
    
    return ESP_OK;
}

esp_err_t mhz14a_set_self_calibration(bool enable)
{
    if (mhz14a_uart_mutex == NULL) return ESP_FAIL;

    xSemaphoreTake(mhz14a_uart_mutex, portMAX_DELAY);
    esp_err_t err = mhz14a_send_command(MHZ14A_UART_CMD_SET_SELF_CALIBRATION_ZERO_POINT, enable);
    xSemaphoreGive(mhz14a_uart_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set self calibration");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Self-calibration function %s successfully", enable ? "enabled" : "disabled");
    return ESP_OK;
}

#ifdef CONFIG_HD_PIN
esp_err_t mhz14a_auto_calibration_hd_pin(void)
{
    gpio_reset_pin(CONFIG_HD_PIN);
    gpio_set_direction(CONFIG_HD_PIN, GPIO_MODE_OUTPUT);
    
    ESP_LOGI(TAG, "MHZ14a sensor is starting auto calibration via HD pin...");
    gpio_set_level(CONFIG_HD_PIN, 0);
    
    ESP_LOGI(TAG, "Waiting about 7 seconds for auto calibration function...");
    vTaskDelay(pdMS_TO_TICKS(MHZ14A_TIME_FOR_CALIBRATION_MS));
    
    gpio_set_level(CONFIG_HD_PIN, 1);
    ESP_LOGI(TAG, "MHZ14a finished calibration via HD pin!");
    
    gpio_reset_pin(CONFIG_HD_PIN);
    return ESP_OK;
}
#endif

// PWM variables
static uint32_t cap_val_begin_of_sample = 0;
static uint32_t cap_val_end_of_sample = 0;
static QueueHandle_t cap_queue = NULL;

static bool IRAM_ATTR mhz_isr_handler(mcpwm_unit_t mcpwm, mcpwm_capture_channel_id_t cap_sig, const cap_event_data_t *edata, void *arg) {
    BaseType_t high_task_wakeup = pdFALSE;
    if (edata->cap_edge == MCPWM_POS_EDGE) {
        cap_val_begin_of_sample = edata->cap_value;
        cap_val_end_of_sample = cap_val_begin_of_sample;
    } else {
        cap_val_end_of_sample = edata->cap_value;
        uint32_t pulse_count = cap_val_end_of_sample - cap_val_begin_of_sample;
        xQueueSendFromISR(cap_queue, &pulse_count, &high_task_wakeup);
    }
    return high_task_wakeup == pdTRUE;
}

esp_err_t mhz14a_init_pwm(int pwm_pin)
{
    cap_queue = xQueueCreate(1, sizeof(uint32_t));
    if (cap_queue == NULL) {
        ESP_LOGE(TAG, "Failed to allocate cap_queue");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM_CAP_0, pwm_pin));
    ESP_ERROR_CHECK(gpio_pulldown_en(pwm_pin));
    
    mcpwm_capture_config_t conf = {
        .cap_edge = MCPWM_BOTH_EDGE,
        .cap_prescale = 1,
        .capture_cb = mhz_isr_handler,
        .user_data = NULL
    };
    ESP_ERROR_CHECK(mcpwm_capture_enable_channel(MCPWM_UNIT_0, MCPWM_SELECT_CAP0, &conf));
    ESP_LOGI(TAG, "MHZ14A PWM pin configured");

    uint64_t time_start_warm_up = esp_timer_get_time();
    while (esp_timer_get_time() - time_start_warm_up < 100000) 
    {
        ESP_LOGI(TAG, "MHZ14A warming up...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "MHZ14A PWM initialized successfully");
    return ESP_OK;
}

esp_err_t mhz14a_get_data_pwm(uint32_t *co2_ppm)
{
    if (cap_queue == NULL || co2_ppm == NULL) return ESP_FAIL;

    uint32_t pulse_count;
    if (xQueueReceive(cap_queue, &pulse_count, portMAX_DELAY) == pdTRUE) {
        // rtc_clk_apb_freq_get() is deprecated in IDF 5+, but keeping original logic
        uint32_t pulse_width_us = pulse_count * (1000000.0 / rtc_clk_apb_freq_get());
        uint32_t time_high = pulse_width_us / 1000;
        uint32_t time_low = 1004 - time_high;
        *co2_ppm = 5000 * (time_high - 2) / (time_high + time_low - 4);
        return ESP_OK;
    }
    return ESP_FAIL;
}
