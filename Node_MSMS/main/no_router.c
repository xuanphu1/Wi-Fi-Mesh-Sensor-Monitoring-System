/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_wifi.h"
#include "nvs_flash.h"
#include <sys/socket.h>
#include <arpa/inet.h>

#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_bridge.h"
#include "esp_mesh_lite.h"
#include "led_strip.h"
#include "esp_idf_version.h"
#include "driver/i2c.h"

static const char *TAG = "no_router_child";

#define MESH_UDP_ROOT_IP  "192.168.5.1"
#define MESH_UDP_PORT  12345
/** Đủ chỗ cho {"v","seq","n","i":"<ipv4>","t":"<ds3231_iso>","p":[...]} */
#define MESH_SENSOR_PAYLOAD_MAX  384
#define MESH_UDP_JSON_SCHEMA  1
#define MESH_NODE_VERSION  "1.0.0"
/*
 * Compact frame để đồng bộ parser:
 *   {"v":<schema>,"seq":<counter>,"n":<mesh_level>,"i":"<sta_ipv4>","t":"<rtc_iso>","ver":"<x.x.x>","err":["0xNN",...],"p":[[port,type,v0,...], ...]}
 * Thứ tự key giữ cố định: v -> seq -> n -> i -> t -> ver -> err -> p
 *
 * Cấu trúc cụ thể:
 * - v: schema version của bản tin JSON, hiện là 1.
 * - seq: số thứ tự gói UDP telemetry, bắt đầu từ 1 ở gói đầu tiên sau khi boot.
 * - n: mesh level hiện tại của node.
 * - i: IPv4 STA mesh của node, ví dụ "192.168.5.2".
 * - t: thời gian RTC DS3231 dạng ISO local, ví dụ "2026-04-26T10:08:00".
 * - ver: version firmware node, ví dụ "1.0.0".
 * - err: danh sách mã lỗi khác 0, mỗi mã dạng chuỗi "0xNN".
 * - p: danh sách port sensor; mỗi phần tử có dạng [port_1based, sensor_type, value0, value1, ...].
 *
 * Ví dụ:
 *   {"v":1,"seq":1,"n":2,"i":"192.168.5.2","t":"2026-04-26T10:08:00","ver":"1.0.0","err":["0x2C","0xF6","0x19"],"p":[[1,0,25.125,62.500,1008.750],[3,13,27.250,70.000]]}
 */
#define DS3231_I2C_PORT  I2C_NUM_0
#define DS3231_I2C_SDA_GPIO  GPIO_NUM_13
#define DS3231_I2C_SCL_GPIO  GPIO_NUM_12
#define DS3231_I2C_FREQ_HZ  100000
#define DS3231_I2C_ADDR  0x68

static bool s_ds3231_ready = false;

/** WS2812 RGB trên ESP32-C6-DevKitC-1 (theo datasheet kit: GPIO8) */
#if CONFIG_IDF_TARGET_ESP32C6
#define RGB_LED_GPIO  8
#else
#define RGB_LED_GPIO  8
#endif

/** Độ sáng ~50% (WS2812 8-bit) */
#define RGB_LED_HALF  128

static led_strip_handle_t s_rgb_strip = NULL;

static esp_err_t rgb_led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_LED_GPIO,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
    };
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
#else
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
#endif
    return led_strip_new_rmt_device(&strip_config, &rmt_config, &s_rgb_strip);
}

static void rgb_led_off(void)
{
    if (s_rgb_strip == NULL) {
        return;
    }
    led_strip_clear(s_rgb_strip);
    led_strip_refresh(s_rgb_strip);
}

/** Mỗi lần đang “quét” chờ root trong mesh list: nháy đỏ ~50%. */
static void rgb_led_blink_red_while_scanning_root(void)
{
    if (s_rgb_strip == NULL) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        led_strip_set_pixel(s_rgb_strip, 0, RGB_LED_HALF, 0, 0);
        led_strip_refresh(s_rgb_strip);
        vTaskDelay(pdMS_TO_TICKS(120));
        rgb_led_off();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/** Sau khi đã có route gửi được: mỗi lần gửi xong — nháy xanh ~50%, không đỏ. */
static void rgb_led_blink_green_on_send_ok(void)
{
    if (s_rgb_strip == NULL) {
        return;
    }
    rgb_led_off();
    led_strip_set_pixel(s_rgb_strip, 0, 0, RGB_LED_HALF, 0);
    led_strip_refresh(s_rgb_strip);
    vTaskDelay(pdMS_TO_TICKS(180));
    rgb_led_off();
}

/** Giá trị float giả trong [min, max] từ esp_random() */
static float mesh_fake_float(float min_v, float max_v)
{
    uint32_t r = esp_random();
    double t = (double)(r % 10000) / 9999.0;
    return (float)(min_v + t * (max_v - min_v));
}

/**
 * UDP telemetry JSON compact (schema MESH_UDP_JSON_SCHEMA):
 *   {"v":<schema>,"seq":<counter>,"n":<mesh_level>,"i":"<sta_ipv4>","t":"<ds3231_iso>","ver":"<x.x.x>","err":["0xNN",...],"p":[[port,type,v0,...], ...]}
 *
 * Wire schema:
 * - Root object key order is stable: v, seq, n, i, t, ver, err, p.
 * - Numeric values in p are emitted with 3 decimal places.
 * - Each p item starts with port_1based and SensorType_t wire ID, followed by sensor-specific float values.
 * - Example p item for BME280: [1,0,temp_c,humidity_pct,pressure_hpa].
 * - Example p item for AHT10: [3,13,temp_c,humidity_pct].
 *
 * - seq: bộ đếm gói UDP telemetry, bắt đầu từ 1 ở gói đầu tiên sau khi boot.
 * - i: IPv4 STA mesh (esp_netif "WIFI_STA_DEF"), dotted-quad.
 * - t: thời gian đọc từ DS3231 (ISO-8601 local, ví dụ 2026-04-26T09:10:00).
 * - ver: version node dạng x.x.x.
 * - err: chỉ gửi các mã lỗi khác "0x00", định dạng "0xNN" (2 ký tự hex).
 * - p: port 1-based, type SensorType_t, v* float %.3f (fake demo).
 */
static size_t mesh_build_error_codes_json_array(char *out, size_t out_sz)
{
    if (out_sz == 0) {
        return 0;
    }

    size_t used = 0;
    int n = snprintf(out + used, out_sz - used, "[");
    if (n < 0 || (size_t)n >= out_sz - used) {
        return 0;
    }
    used += (size_t)n;

    for (size_t i = 0; i < 3; i++) {
        uint8_t code = (uint8_t)(esp_random() & 0xFF);
        if (code == 0) {
            code = 1;
        }
        n = snprintf(out + used, out_sz - used,
                     (i == 0) ? "\"0x%02X\"" : ",\"0x%02X\"",
                     code);
        if (n < 0 || (size_t)n >= out_sz - used) {
            return 0;
        }
        used += (size_t)n;
    }

    n = snprintf(out + used, out_sz - used, "]");
    if (n < 0 || (size_t)n >= out_sz - used) {
        return 0;
    }
    used += (size_t)n;
    return used;
}

static void mesh_fill_sta_ipv4_str(char *out, size_t out_sz)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ipi;
    if (sta == NULL || esp_netif_get_ip_info(sta, &ipi) != ESP_OK) {
        strlcpy(out, "0.0.0.0", out_sz);
        return;
    }
    struct in_addr ia = { .s_addr = ipi.ip.addr };
    const char *s = inet_ntoa(ia);
    strlcpy(out, s ? s : "0.0.0.0", out_sz);
}

static void mesh_fill_ds3231_time_str(char *out, size_t out_sz)
{
    if (out_sz == 0) {
        return;
    }
    strlcpy(out, "1970-01-01T00:00:00", out_sz);

    if (!s_ds3231_ready) {
        return;
    }

    uint8_t start_reg = 0x00;
    uint8_t raw[7] = {0};
    esp_err_t err = i2c_master_write_read_device(
        DS3231_I2C_PORT,
        DS3231_I2C_ADDR,
        &start_reg, sizeof(start_reg),
        raw, sizeof(raw),
        pdMS_TO_TICKS(50)
    );
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DS3231 read failed: %s", esp_err_to_name(err));
        return;
    }

    int sec = ((raw[0] >> 4) & 0x7) * 10 + (raw[0] & 0x0F);
    int min = ((raw[1] >> 4) & 0x7) * 10 + (raw[1] & 0x0F);
    int hour = ((raw[2] >> 4) & 0x3) * 10 + (raw[2] & 0x0F);
    int day = ((raw[4] >> 4) & 0x3) * 10 + (raw[4] & 0x0F);
    int mon = ((raw[5] >> 4) & 0x1) * 10 + (raw[5] & 0x0F);
    int year = 2000 + (((raw[6] >> 4) & 0xF) * 10 + (raw[6] & 0x0F));
    snprintf(out, out_sz, "%04d-%02d-%02dT%02d:%02d:%02d",
             year,
             mon, day,
             hour, min, sec);
}

static size_t mesh_build_udp_telemetry_json(char *payload, size_t payload_sz, uint32_t seq)
{
    int node_level = esp_mesh_lite_get_level();
    if (node_level < 0) {
        node_level = 0;
    }

    char sta_ip[16];
    mesh_fill_sta_ipv4_str(sta_ip, sizeof(sta_ip));

    char rtc_time[24];
    mesh_fill_ds3231_time_str(rtc_time, sizeof(rtc_time));
    char err_codes_json[128];
    if (mesh_build_error_codes_json_array(err_codes_json, sizeof(err_codes_json)) == 0) {
        return 0;
    }

    float bme[3] = {
        mesh_fake_float(18.0f, 32.0f),
        mesh_fake_float(35.0f, 85.0f),
        mesh_fake_float(990.0f, 1030.0f),
    };
    float aht10[2] = {
        mesh_fake_float(20.0f, 34.0f),
        mesh_fake_float(40.0f, 85.0f),
    };

    /*
     * Sensor type (SensorType_t wire):
     *   BME280 = 0, AHT10 = 13
     *
     * JSON phát ra để đồng bộ:
     *   {"v":1,"seq":1,"n":2,"i":"192.168.5.2","t":"2026-04-26T10:08:00","ver":"1.0.0","err":["0x2C","0xF6","0x19"],"p":[[1,0,...],[3,13,...]]}
     * - t: thời gian lấy từ DS3231 (fallback "1970-01-01T00:00:00" nếu chưa đọc được RTC)
     */
    int written = snprintf(
        payload, payload_sz,
        "{\"v\":%d,\"seq\":%" PRIu32 ",\"n\":%d,\"i\":\"%s\",\"t\":\"%s\",\"ver\":\"%s\",\"err\":%s,\"p\":[[1,0,%.3f,%.3f,%.3f],[3,13,%.3f,%.3f]]}",
        MESH_UDP_JSON_SCHEMA, seq, node_level, sta_ip, rtc_time, MESH_NODE_VERSION, err_codes_json,
        bme[0], bme[1], bme[2],
        aht10[0], aht10[1]
    );

    if (written < 0 || (size_t)written >= payload_sz) {
        return 0;
    }
    return (size_t)written;
}

static void mesh_udp_client_task(void *pvParameters)
{
    uint32_t udp_seq = 0;

    while (1) {
        rgb_led_off();

        int mesh_level = esp_mesh_lite_get_level();
        if (mesh_level <= 1) {
            ESP_LOGW(TAG, "Mesh is not joined yet (layer=%d), skip UDP telemetry", mesh_level);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(MESH_UDP_PORT);
        if (inet_pton(AF_INET, MESH_UDP_ROOT_IP, &dest_addr.sin_addr) != 1) {
            ESP_LOGE(TAG, "Invalid root UDP IP: %s", MESH_UDP_ROOT_IP);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        char payload[MESH_SENSOR_PAYLOAD_MAX];
        uint32_t next_seq = udp_seq + 1;
        size_t plen = mesh_build_udp_telemetry_json(payload, sizeof(payload), next_seq);
        if (plen == 0) {
            ESP_LOGE(TAG, "JSON sensor frame build failed");
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        udp_seq = next_seq;

        ESP_LOGI(TAG, "Sending JSON %u B to %s:%d | %s", (unsigned)plen,
                 inet_ntoa(dest_addr.sin_addr), MESH_UDP_PORT, payload);
        int err = sendto(sock, payload, plen, 0,
                         (struct sockaddr *)&dest_addr, sizeof(dest_addr));

        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        } else {
            ESP_LOGI(TAG, "Message sent successfully (%d bytes)", err);
            rgb_led_blink_green_on_send_ok();
        }

        close(sock);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Timed printing system information
 */
static void print_system_info_timercb(TimerHandle_t timer)
{
    uint8_t primary                 = 0;
    uint8_t sta_mac[6]              = {0};
    wifi_ap_record_t ap_info        = {0};
    wifi_second_chan_t second       = 0;
    wifi_sta_list_t wifi_sta_list   = {0x0};

    if (esp_mesh_lite_get_level() > 1) {
        esp_wifi_sta_get_ap_info(&ap_info);
    }
    esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    esp_wifi_get_channel(&primary, &second);

    ESP_LOGI(TAG, "System information, channel: %d, layer: %d, self mac: " MACSTR ", parent bssid: " MACSTR
             ", parent rssi: %d, free heap: %"PRIu32"", primary,
             esp_mesh_lite_get_level(), MAC2STR(sta_mac), MAC2STR(ap_info.bssid),
             (ap_info.rssi != 0 ? ap_info.rssi : -120), esp_get_free_heap_size());

    for (int i = 0; i < wifi_sta_list.num; i++) {
        ESP_LOGI(TAG, "Child mac: " MACSTR, MAC2STR(wifi_sta_list.sta[i].mac));
    }

    uint32_t size = 0;
    const node_info_list_t *node = esp_mesh_lite_get_nodes_list(&size);
    printf("MeshLite nodes %u:\r\n", (unsigned int)size);
    for (uint32_t loop = 0; (loop < size) && (node != NULL); loop++) {
        struct in_addr ip_struct;
        ip_struct.s_addr = node->node->ip_addr;
        printf("%u: %d, "MACSTR", %s\r\n",
               (unsigned int)(loop + 1),
               node->node->level,
               MAC2STR(node->node->mac_addr),
               inet_ntoa(ip_struct));
        node = node->next;
    }
}

static esp_err_t esp_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    return ret;
}

static void wifi_init(void)
{
    // Station
    wifi_config_t wifi_config;
    memset(&wifi_config, 0x0, sizeof(wifi_config_t));
    esp_bridge_wifi_set_config(WIFI_IF_STA, &wifi_config);

    // Softap
    wifi_config_t wifi_softap_config = {
                                           .ap = {
                                                     .ssid = CONFIG_BRIDGE_SOFTAP_SSID,
                                                     .password = CONFIG_BRIDGE_SOFTAP_PASSWORD,
                                                     .channel = CONFIG_MESH_CHANNEL,
                                                 },
                                       };
    esp_bridge_wifi_set_config(WIFI_IF_AP, &wifi_softap_config);
}

void app_wifi_set_softap_info(void)
{
    char softap_ssid[33];
    char softap_psw[64];
    uint8_t softap_mac[6];
    size_t ssid_size = sizeof(softap_ssid);
    size_t psw_size = sizeof(softap_psw);
    esp_wifi_get_mac(WIFI_IF_AP, softap_mac);
    memset(softap_ssid, 0x0, sizeof(softap_ssid));
    memset(softap_psw, 0x0, sizeof(softap_psw));

    if (esp_mesh_lite_get_softap_ssid_from_nvs(softap_ssid, &ssid_size) == ESP_OK) {
        ESP_LOGI(TAG, "Get ssid from nvs: %s", softap_ssid);
    } else {
#ifdef CONFIG_BRIDGE_SOFTAP_SSID_END_WITH_THE_MAC
        snprintf(softap_ssid, sizeof(softap_ssid), "%.25s_%02x%02x%02x", CONFIG_BRIDGE_SOFTAP_SSID, softap_mac[3], softap_mac[4], softap_mac[5]);
#else
        snprintf(softap_ssid, sizeof(softap_ssid), "%.32s", CONFIG_BRIDGE_SOFTAP_SSID);
#endif
        ESP_LOGI(TAG, "Get ssid from nvs failed, set ssid: %s", softap_ssid);
    }

    if (esp_mesh_lite_get_softap_psw_from_nvs(softap_psw, &psw_size) == ESP_OK) {
        ESP_LOGI(TAG, "Get psw from nvs: [HIDDEN]");
    } else {
        strlcpy(softap_psw, CONFIG_BRIDGE_SOFTAP_PASSWORD, sizeof(softap_psw));
        ESP_LOGI(TAG, "Get psw from nvs failed, set psw: [HIDDEN]");
    }

    esp_mesh_lite_set_softap_info(softap_ssid, softap_psw);
}

void app_main()
{
    // Set the log level for serial port printing.
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_storage_init();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_bridge_create_all_netif();

    wifi_init();

    esp_mesh_lite_config_t mesh_lite_config = ESP_MESH_LITE_DEFAULT_INIT();
    mesh_lite_config.join_mesh_ignore_router_status = true;
    mesh_lite_config.join_mesh_without_configured_wifi = true;
    esp_mesh_lite_init(&mesh_lite_config);

    app_wifi_set_softap_info();

    ESP_LOGI(TAG, "Child node");
    esp_mesh_lite_set_disallowed_level(1);

    esp_mesh_lite_start();

    i2c_config_t ds3231_i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = DS3231_I2C_SDA_GPIO,
        .scl_io_num = DS3231_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = DS3231_I2C_FREQ_HZ,
    };
    esp_err_t i2c_err = i2c_param_config(DS3231_I2C_PORT, &ds3231_i2c_cfg);
    if (i2c_err != ESP_OK) {
        ESP_LOGW(TAG, "i2c_param_config failed (%s), skip DS3231", esp_err_to_name(i2c_err));
    } else {
        i2c_err = i2c_driver_install(DS3231_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
        if (i2c_err != ESP_OK) {
            ESP_LOGW(TAG, "i2c_driver_install failed (%s), skip DS3231", esp_err_to_name(i2c_err));
        } else {
            s_ds3231_ready = true;
            ESP_LOGI(TAG, "DS3231 ready on I2C port %d (SDA=%d, SCL=%d)",
                     DS3231_I2C_PORT, DS3231_I2C_SDA_GPIO, DS3231_I2C_SCL_GPIO);
        }
    }

    esp_err_t led_err = rgb_led_init();
    if (led_err != ESP_OK) {
        ESP_LOGW(TAG, "Không khởi tạo được LED RGB (%s) — tiếp tục không báo hiệu đèn",
                 esp_err_to_name(led_err));
        s_rgb_strip = NULL;
    }

    /* UDP client task for child */
    xTaskCreate(mesh_udp_client_task, "mesh_udp_client", 4096, NULL, 5, NULL);

    TimerHandle_t timer = xTimerCreate("print_system_info", 10000 / portTICK_PERIOD_MS,
                                       true, NULL, print_system_info_timercb);
    if (xTimerStart(timer, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "xTimerStart(print_system_info) failed");
    }
}
