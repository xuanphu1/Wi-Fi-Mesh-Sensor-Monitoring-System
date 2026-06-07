#include <stdio.h>
#include "SD_Card.h"
#include "Datamanager.h"
#include "PowerManager.h"
#include "SystemMonitor.h"
#include "ScreenManager.h"
#include "UartToNode.h"
#include "WifiManager.h"
#include "WSHandle.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

static const char *TAG = "app";

static dm_hw_t g_hw = {0};
static dm_cpu_t g_cpu = {0};
static dm_lvgl_t g_lvgl = {0};
static dm_metrics_t g_metrics = {0};
static dm_wifi_t g_wifi = {0};
static dm_ws_t g_ws = {0};
static dm_telemetry_t g_telemetry = {0};
static dm_uart_t g_uart = {0};
static ws_handler_ctx_t g_ws_ctx = {0};

static void app_gpio2_high_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(GPIO_NUM_2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_2, 1));
}

/** Mount partition `spiffs` tại `/spiffs` — captive portal WifiManager đọc index.html / redirect.html. */
static esp_err_t app_mount_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 8,
        .format_if_mount_failed = false,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "SPIFFS mount/format failed");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Không thấy partition spiffs (kiểm tra partitions.csv & flash)");
        } else {
            ESP_LOGE(TAG, "SPIFFS: %s", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info("spiffs", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: used %u / total %u bytes", (unsigned)used, (unsigned)total);
    }
    return ESP_OK;
}

void vApplicationIdleHook(void)
{
    // Required when CONFIG_FREERTOS_USE_IDLE_HOOK=y
}

void app_main(void)
{
    app_gpio2_high_init();

    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    if (app_mount_spiffs() != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS không mount được — captive portal có thể 404 index.html");
    }

    websocket_attach_state(&g_ws);
    wifi_manager_attach_state(&g_wifi);

    g_uart.uplink_queue =
        xQueueCreate(UART_NODE_RX_QUEUE_DEPTH, sizeof(uart_node_rx_item_t));
    if (g_uart.uplink_queue == NULL) {
        ESP_LOGE(TAG, "uart uplink queue create failed");
    }

    g_metrics.mutex = xSemaphoreCreateMutex();
    g_lvgl.loop_counter = 0;
    g_lvgl.prev_loop_counter = 0;

    screen_manager_start(&g_metrics, &g_lvgl, 4, 1);

    esp_err_t sd_ret = initSDCard();
    if (sd_ret != ESP_OK)
        printf("[SD ] initSDCard() failed: %s (0x%x)\n", esp_err_to_name(sd_ret), (unsigned)sd_ret);

    esp_err_t rtc_ret = ds3231_init_default(&g_hw.rtc_dev);
    g_hw.rtc_ready = (rtc_ret == ESP_OK);

    power_manager_battery_adc_init(&g_hw);
    system_monitor_start(&g_hw, &g_cpu, &g_lvgl, &g_metrics, &g_telemetry, 5, 0);

    wifi_init_sta();

    const uint32_t ws_stack = 8192;
    g_ws_ctx.ws = &g_ws;
    g_ws_ctx.telemetry = &g_telemetry;
    g_ws_ctx.uart = &g_uart;
    g_ws_ctx.metrics = &g_metrics;
    xTaskCreate(WebSocket_Handler, "ws_hdl", ws_stack, &g_ws_ctx, 5, NULL);

    uart_to_node_attach_uplink_queue(g_uart.uplink_queue);
    uart_to_node_start();

    vTaskDelete(NULL);
}
