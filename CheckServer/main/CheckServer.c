#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "esp_websocket_client.h"
#include "driver/i2c.h"
#include "ssd1306.h"

#define TAG "CheckServer"

// Cấu hình I2C cho màn hình OLED SSD1306
#define I2C_MASTER_SCL_IO           22     /*!< GPIO cho I2C SCL */
#define I2C_MASTER_SDA_IO           21     /*!< GPIO cho I2C SDA */
#define I2C_MASTER_NUM              I2C_NUM_0 /*!< Cổng I2C master */
#define I2C_MASTER_FREQ_HZ          400000 /*!< Tần số xung I2C master */

// Cấu hình Wi-Fi (Thay đổi SSID và PASSWORD phù hợp với mạng của bạn)
#define EXAMPLE_ESP_WIFI_SSID      "XR"
#define EXAMPLE_ESP_WIFI_PASS      "Vht@2024"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

// URL WebSocket WSS
#define WEBSOCKET_URI              "wss://systemmsems.msems.click/ws"
#define WS_RECONNECT_TIMEOUT_MS    15000 // Tự động reconnect sau 15 giây nếu mất kết nối

/* Event group dùng để quản lý sự kiện kết nối Wi-Fi */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
static esp_websocket_client_handle_t client = NULL;
static bool s_is_ws_connected = false;

/* Thống kê cho màn hình OLED */
static ssd1306_handle_t oled_dev = NULL;
static uint32_t s_ws_reconnect_count = 0;      // Số lần kết nối lại với WebSocket Server
static uint32_t s_bytes_in_last_sec = 0;        // Bytes truyền/nhận trong chu kỳ 1s
static float s_network_speed_kbps = 0.0f;       // Tốc độ mạng (KB/s)

/* Event handler xử lý sự kiện Wi-Fi và IP */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Đang thử kết nối lại Wi-Fi... (lần %d)", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGE(TAG, "Kết nối Wi-Fi thất bại");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Đã nhận được IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* Khởi tạo Wi-Fi ở chế độ Station */
static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Hoàn tất khởi tạo Wi-Fi STA.");

    /* Chờ cho đến khi kết nối thành công hoặc thất bại */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Đã kết nối thành công tới AP SSID: %s", EXAMPLE_ESP_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Không thể kết nối tới AP SSID: %s", EXAMPLE_ESP_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Sự kiện Wi-Fi không xác định");
    }
}

/* Event handler xử lý các sự kiện của WebSocket Client */
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED: Đã kết nối thành công tới WSS Server!");
        s_is_ws_connected = true;
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        s_ws_reconnect_count++; // Tăng số lần kết nối lại với WebSocket Server
        ESP_LOGW(TAG, "WEBSOCKET_EVENT_DISCONNECTED: Mất kết nối tới Server. Đã kết nối lại %" PRIu32 " lần. Sẽ tự động kết nối lại sau %d ms...", s_ws_reconnect_count, WS_RECONNECT_TIMEOUT_MS);
        s_is_ws_connected = false;
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA: Nhận bản tin từ Server (độ dài: %d bytes)", data->data_len);
        if (data->data_len > 0) {
            s_bytes_in_last_sec += data->data_len;
        }
        if (data->data_ptr != NULL && data->data_len > 0) {
            ESP_LOGI(TAG, "Nội dung nhận: %.*s", data->data_len, (char *)data->data_ptr);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR: Đã xảy ra lỗi WebSocket!");
        s_is_ws_connected = false;
        break;
    default:
        break;
    }
}

/* Task gửi dữ liệu định kỳ lên WebSocket Server */
static void websocket_send_task(void *pvParameters)
{
    uint32_t message_count = 0;

    while (1) {
        if (client != NULL && s_is_ws_connected && esp_websocket_client_is_connected(client)) {
            message_count++;

            // Tạo dữ liệu JSON bất kỳ để gửi lên Server
            cJSON *root = cJSON_CreateObject();
            if (root != NULL) {
                cJSON_AddStringToObject(root, "type", "sensor_data");
                cJSON_AddStringToObject(root, "client", "ESP32_CheckServer");
                cJSON_AddNumberToObject(root, "msg_id", message_count);
                cJSON_AddNumberToObject(root, "temperature", 25.5 + (rand() % 50) / 10.0);
                cJSON_AddNumberToObject(root, "humidity", 60.0 + (rand() % 100) / 10.0);
                cJSON_AddNumberToObject(root, "uptime_s", esp_log_timestamp() / 1000);

                char *json_string = cJSON_PrintUnformatted(root);
                cJSON_Delete(root);

                if (json_string != NULL) {
                    size_t len = strlen(json_string);
                    ESP_LOGI(TAG, "Gửi dữ liệu lên WebSocket (%d bytes): %s", len, json_string);
                    int ret = esp_websocket_client_send_text(client, json_string, len, portMAX_DELAY);
                    if (ret >= 0) {
                        s_bytes_in_last_sec += len;
                    } else {
                        ESP_LOGE(TAG, "Gửi bản tin thất bại (ret = %d)", ret);
                    }
                    free(json_string);
                }
            }
        } else {
            ESP_LOGD(TAG, "Chờ WebSocket kết nối để gửi dữ liệu...");
        }

        // Gửi dữ liệu định kỳ mỗi 5 giây
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* Khởi tạo OLED SSD1306 */
static esp_err_t oled_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config thất bại: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install thất bại: %s", esp_err_to_name(ret));
        return ret;
    }

    oled_dev = ssd1306_create(I2C_MASTER_NUM, SSD1306_I2C_ADDRESS);
    if (oled_dev == NULL) {
        ESP_LOGE(TAG, "Khởi tạo ssd1306_create thất bại!");
        return ESP_FAIL;
    }

    ssd1306_clear_screen(oled_dev, 0);
    ssd1306_draw_string(oled_dev, 0, 0, (const uint8_t *)"=== CHECK SERVER ===", 12, 1);
    ssd1306_draw_string(oled_dev, 0, 16, (const uint8_t *)"OLED Init OK", 12, 1);
    ssd1306_refresh_gram(oled_dev);
    ESP_LOGI(TAG, "Khởi tạo OLED SSD1306 thành công (SDA: GPIO %d, SCL: GPIO %d)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    return ESP_OK;
}

/* Task cập nhật màn hình OLED định kỳ mỗi 1 giây */
static void oled_update_task(void *pvParameters)
{
    char str_buf[32];
    wifi_ap_record_t ap_info;

    while (1) {
        // Tốc độ truyền/nhận mạng theo chu kỳ 1s (KB/s)
        s_network_speed_kbps = (float)s_bytes_in_last_sec / 1024.0f;
        s_bytes_in_last_sec = 0;

        int rssi = 0;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            rssi = ap_info.rssi;
        }

        if (oled_dev != NULL) {
            ssd1306_clear_screen(oled_dev, 0);

            // Dòng 1: Tiêu đề
            ssd1306_draw_string(oled_dev, 0, 0, (const uint8_t *)"=== CHECK SERVER ===", 12, 1);

            // Dòng 2: Trạng thái Server WebSocket
            if (s_is_ws_connected) {
                ssd1306_draw_string(oled_dev, 0, 13, (const uint8_t *)"WS Status: CONNECTED", 12, 1);
            } else {
                ssd1306_draw_string(oled_dev, 0, 13, (const uint8_t *)"WS Status: DISCONN  ", 12, 1);
            }

            // Dòng 3: Số lần kết nối lại với Server
            snprintf(str_buf, sizeof(str_buf), "Reconn: %" PRIu32 " times", s_ws_reconnect_count);
            ssd1306_draw_string(oled_dev, 0, 26, (const uint8_t *)str_buf, 12, 1);

            // Dòng 4: Wi-Fi RSSI
            snprintf(str_buf, sizeof(str_buf), "WiFi RSSI: %d dBm", rssi);
            ssd1306_draw_string(oled_dev, 0, 39, (const uint8_t *)str_buf, 12, 1);

            // Dòng 5: Tốc độ mạng (B/s hoặc KB/s)
            if (s_network_speed_kbps >= 1.0f) {
                snprintf(str_buf, sizeof(str_buf), "Speed: %.2f KB/s", s_network_speed_kbps);
            } else {
                snprintf(str_buf, sizeof(str_buf), "Speed: %.0f B/s", s_network_speed_kbps * 1024.0f);
            }
            ssd1306_draw_string(oled_dev, 0, 52, (const uint8_t *)str_buf, 12, 1);

            ssd1306_refresh_gram(oled_dev);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Khởi tạo WebSocket Client */
static void websocket_app_start(void)
{
    esp_websocket_client_config_t websocket_cfg = {
        .uri = WEBSOCKET_URI,
        .reconnect_timeout_ms = WS_RECONNECT_TIMEOUT_MS, // Tự kết nối lại sau 15 giây khi mất kết nối
        .network_timeout_ms = 10000,
        .task_stack = 4096,
        .buffer_size = 1024,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach, // Tự động load Certificate Bundle xử lý WSS SSL/TLS
    };

    ESP_LOGI(TAG, "Khởi tạo WebSocket Client tới URI: %s (reconnect delay: %d ms)", WEBSOCKET_URI, WS_RECONNECT_TIMEOUT_MS);
    client = esp_websocket_client_init(&websocket_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Khởi tạo esp_websocket_client thất bại!");
        return;
    }

    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    esp_websocket_client_start(client);

    // Tạo Task gửi dữ liệu
    xTaskCreate(websocket_send_task, "ws_send_task", 4096, NULL, 5, NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // Khởi tạo NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Khởi tạo OLED SSD1306 (SDA = GPIO 21, SCL = GPIO 22)
    if (oled_init() == ESP_OK) {
        xTaskCreate(oled_update_task, "oled_update_task", 3072, NULL, 4, NULL);
    }

    // Khởi tạo Wi-Fi
    wifi_init_sta();

    // Khởi tạo và khởi chạy WebSocket
    websocket_app_start();
}

