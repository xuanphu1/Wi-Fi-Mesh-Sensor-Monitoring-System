/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "WSHandle.h"

#include "ScreenManager.h"
#include "WifiManager.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

static esp_websocket_client_handle_t client = NULL;

#define TAG_WEBSOCKET "WebSocket Handler"
#define GATEWAY_STATUS_INTERVAL_MS 5000
#define WS_RECONNECT_TIMEOUT_MS 15000
#define WS_NETWORK_TIMEOUT_MS 5000

static void websocket_client_destroy_current(void)
{
    if (client == NULL) {
        return;
    }

    esp_err_t err = esp_websocket_client_destroy(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_WEBSOCKET, "WebSocket destroy failed: %s", esp_err_to_name(err));
    }
    client = NULL;
}

static const char *gateway_power_source_str(uint32_t pack_mv, bool have_voltage)
{
    if (!have_voltage || pack_mv < 2000U) {
        return "unknown";
    }
    /* Heuristic: nguồn ngoài/USB thường đẩy điện áp gói cao hơn khi đang nạp — tinh chỉnh theo mạch thực tế. */
    if (pack_mv >= 4180U) {
        return "AC_or_USB";
    }
    return "battery";
}

static void ws_send_gateway_status(ws_handler_ctx_t *ctx)
{
    if (client == NULL || !esp_websocket_client_is_connected(client) || ctx == NULL || ctx->metrics == NULL) {
        return;
    }

    ui_metrics_t m = {0};
    if (ctx->metrics->mutex == NULL ||
        xSemaphoreTake(ctx->metrics->mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    m = ctx->metrics->value;
    xSemaphoreGive(ctx->metrics->mutex);

    /*
     * --- Payload WebSocket: gateway_status (JSON text, gửi định kỳ ~5s khi WS+STA OK) ---
     * Đồng bộ schema phía server/UI: number = JSON number, string = string, null = không có dữ liệu.
     *
     * {
     *   "type":               string, luôn "gateway_status".
     *   "clientType":         string, luôn "esp32".
     *
     *   "cpu_load_percent":   number, tải CPU 0..100 (%), suy ra từ cpu_load_permille / 10 (SystemMonitor).
     *   "ram_used_kb":        number, RAM đã dùng (kB, heap internal).
     *   "ram_used_percent":   number, % RAM đã dùng 0..100 (= 100 - ram_free_percent từ metrics).
     *
     *   "battery_voltage_v":  number, điện áp gói pin ước lượng (V), từ ADC chia áp (0 nếu chưa đo).
     *   "battery_percent":    number, % pin 0..100.
     *   "power_source":       string, "unknown" | "AC_or_USB" | "battery" (heuristic, gateway_power_source_str).
     *
     *   "chip_temp_c":        number | null, °C nếu chip_temp_valid; else null.
     *   "chip_temp_internal_supported": bool, true chỉ khi SoC có driver TSENS trong IDF
     *                         (CONFIG_SOC_TEMP_SENSOR_SUPPORTED). ESP32 classic = false
     *                         nên chip_temp_c luôn null dù firmware đúng.
     *
     *   "uptime_s":           number, giây từ boot/reset (SystemMonitor / esp_timer).
     *
     *   "wifi_ssid":          string, SSID AP đang bám; rỗng nếu không đọc được.
     *   "wifi_rssi":          number, RSSI dBm (âm), từ esp_wifi_sta_get_ap_info; 0 nếu không có AP.
     *   "sta_ip":             string, IPv4 STA "x.x.x.x"; rỗng nếu chưa có.
     *   "sta_gateway":        string, gateway IPv4 STA; rỗng nếu không đọc được.
     * }
     */
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return;
    }

    cJSON_AddStringToObject(root, "type", "gateway_status");
    cJSON_AddStringToObject(root, "clientType", "esp32");
    cJSON_AddNumberToObject(root, "cpu_load_percent", (double)m.cpu_load_permille / 10.0);

    cJSON_AddNumberToObject(root, "ram_used_kb", (double)m.ram_used_kb);
    {
        uint32_t ram_used_pct = 0U;
        if (m.ram_free_percent <= 100U) {
            ram_used_pct = 100U - m.ram_free_percent;
        }
        cJSON_AddNumberToObject(root, "ram_used_percent", (double)ram_used_pct);
    }

    const bool have_v = (m.battery_pack_mv > 0U);
    const double bat_v = have_v ? (double)m.battery_pack_mv / 1000.0 : 0.0;
    cJSON_AddNumberToObject(root, "battery_voltage_v", bat_v);
    cJSON_AddNumberToObject(root, "battery_percent", (double)m.battery_pct);
    cJSON_AddStringToObject(root, "power_source", gateway_power_source_str(m.battery_pack_mv, have_v));

    cJSON_AddBoolToObject(root, "chip_temp_internal_supported", m.chip_temp_internal_supported ? 1 : 0);
    if (m.chip_temp_valid) {
        cJSON_AddNumberToObject(root, "chip_temp_c", (double)m.chip_temp_c);
    } else {
        cJSON_AddNullToObject(root, "chip_temp_c");
    }

    cJSON_AddNumberToObject(root, "uptime_s", (double)m.uptime_s);

    wifi_ap_record_t ap = {0};
    const bool ap_ok = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);
    if (ap_ok) {
        cJSON_AddStringToObject(root, "wifi_ssid", (const char *)ap.ssid);
        cJSON_AddNumberToObject(root, "wifi_rssi", (double)ap.rssi);
    } else {
        cJSON_AddStringToObject(root, "wifi_ssid", "");
        cJSON_AddNumberToObject(root, "wifi_rssi", 0.0);
    }

    char ip_buf[20] = "";
    char gw_buf[20] = "";
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif != NULL) {
        esp_netif_ip_info_t ipi;
        if (esp_netif_get_ip_info(sta_netif, &ipi) == ESP_OK) {
            snprintf(ip_buf, sizeof(ip_buf), IPSTR, IP2STR(&ipi.ip));
            snprintf(gw_buf, sizeof(gw_buf), IPSTR, IP2STR(&ipi.gw));
        }
    }
    cJSON_AddStringToObject(root, "sta_ip", ip_buf);
    cJSON_AddStringToObject(root, "sta_gateway", gw_buf);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_str != NULL) {
        ESP_LOGI(TAG_WEBSOCKET, "gateway_status send: %s", json_str);
        esp_websocket_client_send_text(client, json_str, strlen(json_str), pdMS_TO_TICKS(2000));
        free(json_str);
    }
}

static char ws_last_type[32] = {0};
static char ws_last_version[32] = {0};

static char ws_url[128] = {0};
static dm_ws_t *s_ws_state = NULL;
static volatile bool s_ws_restart_requested = false;

void websocket_attach_state(dm_ws_t *ws_state)
{
    s_ws_state = ws_state;
}

/** Chỉ RAM — không ghi NVS/flash; mất khi reset. */
esp_err_t save_ws_url(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(ws_url, url, sizeof(ws_url) - 1);
    ws_url[sizeof(ws_url) - 1] = '\0';
    ESP_LOGI(TAG_WEBSOCKET, "WebSocket URL (RAM): %s", ws_url);

    s_ws_restart_requested = true;

    if (s_ws_state) {
        strncpy(s_ws_state->url_cached, ws_url, sizeof(s_ws_state->url_cached) - 1);
        s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
    }

    return ESP_OK;
}

const char *get_ws_url(void)
{
    if (ws_url[0] != '\0') {
        return ws_url;
    }
#if defined(CONFIG_WS_URL)
    return CONFIG_WS_URL;
#else
    return "ws://192.168.4.1:8080/ws";
#endif
}

void SendSignalRegister(void)
{
    if (client == NULL) {
        return;
    }
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "type", "register");
    cJSON_AddStringToObject(data, "clientType", "esp32");

    char *json_str = cJSON_PrintUnformatted(data);
    if (json_str) {
        esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);
        free(json_str);
    }
    cJSON_Delete(data);
}

void DataToSever(dm_telemetry_t *telemetry)
{
    if (client == NULL || telemetry == NULL) {
        return;
    }
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "type", "DataFromESP32");
    cJSON_AddStringToObject(data, "clientType", "esp32");

    cJSON_AddStringToObject(data, "Time", telemetry->timestamp);
    cJSON_AddNumberToObject(data, "Temperature", telemetry->temperature);
    cJSON_AddNumberToObject(data, "Humidity", telemetry->humidity);
    cJSON_AddNumberToObject(data, "Pressure", telemetry->pressure);
    cJSON_AddNumberToObject(data, "PM1_0", telemetry->pm1_0);
    cJSON_AddNumberToObject(data, "PM2_5", telemetry->pm2_5);
    cJSON_AddNumberToObject(data, "PM10", telemetry->pm10);

    char *json_str = cJSON_PrintUnformatted(data);
    if (json_str) {
        esp_websocket_client_send_text(client, json_str, strlen(json_str), portMAX_DELAY);
        free(json_str);
    }
    cJSON_Delete(data);
}

static void websocket_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)base;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {

    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG_WEBSOCKET, "Connected to server");
        if (s_ws_state) {
            s_ws_state->connected = true;
        }
        screen_manager_set_ws_status(true);
        SendSignalRegister();
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG_WEBSOCKET, "Disconnected from server");
        if (s_ws_state) {
            s_ws_state->connected = false;
        }
        screen_manager_set_ws_status(false);
        break;

    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG_WEBSOCKET, "WEBSOCKET_EVENT_DATA");
        if (data && data->data_ptr && data->data_len > 0) {
            size_t len = (size_t)data->data_len;
            char *json_buf = (char *)malloc(len + 1);
            if (json_buf) {
                memcpy(json_buf, data->data_ptr, len);
                json_buf[len] = '\0';

                cJSON *root = cJSON_Parse(json_buf);
                if (root) {
                    const cJSON *j_type = cJSON_GetObjectItem(root, "type");
                    const cJSON *j_version = cJSON_GetObjectItem(root, "version");
                    if (cJSON_IsString(j_type)) {
                        strncpy(ws_last_type, j_type->valuestring, sizeof(ws_last_type) - 1);
                        ws_last_type[sizeof(ws_last_type) - 1] = '\0';
                    }
                    if (cJSON_IsString(j_version)) {
                        strncpy(ws_last_version, j_version->valuestring, sizeof(ws_last_version) - 1);
                        ws_last_version[sizeof(ws_last_version) - 1] = '\0';
                        ESP_LOGI(TAG_WEBSOCKET, "Server version: %s (FOTA stub — không gọi OTA)", ws_last_version);
                    }
                    cJSON_Delete(root);
                }
                free(json_buf);
            }
        }
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG_WEBSOCKET, "WebSocket error occurred");
        if (s_ws_state) {
            s_ws_state->connected = false;
        }
        screen_manager_set_ws_status(false);
        break;

    default:
        break;
    }
}

static void websocket_app_start(void)
{
    const char *url_to_use = get_ws_url();

    esp_websocket_client_config_t websocket_cfg = {
        .uri = url_to_use,
        .reconnect_timeout_ms = WS_RECONNECT_TIMEOUT_MS,
        .network_timeout_ms = WS_NETWORK_TIMEOUT_MS,
        .task_stack = 6144,
        .buffer_size = 2048,
        .keep_alive_enable = true,
    };

    ESP_LOGI(TAG_WEBSOCKET, "Starting WebSocket with URL: %s", url_to_use);

    if (s_ws_state) {
        strncpy(s_ws_state->url_cached, url_to_use, sizeof(s_ws_state->url_cached) - 1);
        s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
    }

    client = esp_websocket_client_init(&websocket_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG_WEBSOCKET, "esp_websocket_client_init failed");
        return;
    }
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    esp_websocket_client_start(client);
    ESP_LOGI(TAG_WEBSOCKET, "WebSocket client start issued");
}

/** Gửi một dòng UART đã ghép hoàn chỉnh lên WS. */
static void ws_send_uart_rx_payload(const char *payload, size_t payload_len)
{
    if (!client || !payload || payload_len == 0) {
        return;
    }
    if (!esp_websocket_client_is_connected(client)) {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "uart_rx");
    cJSON_AddNumberToObject(root, "len", (double)payload_len);
    cJSON_AddStringToObject(root, "payload", payload);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_str) {
        esp_websocket_client_send_text(client, json_str, strlen(json_str),
                                       pdMS_TO_TICKS(2000));
        ESP_LOGI(TAG_WEBSOCKET, "WebSocket send UART RX item: %s", json_str);
        free(json_str);
    }
}

/** Nhận chunk UART từ queue, ghép theo '\n' rồi mới gửi WS để tránh cắt bản tin. */
static void ws_send_uart_rx_item(const uart_node_rx_item_t *item)
{
    enum { WS_UART_LINE_BUF_SZ = 2048 };
    static char s_ws_uart_line[WS_UART_LINE_BUF_SZ];
    static size_t s_ws_uart_line_len = 0;

    if (!item || item->len == 0) {
        return;
    }

    for (size_t i = 0; i < item->len; i++) {
        unsigned char c = item->data[i];

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            if (s_ws_uart_line_len > 0) {
                s_ws_uart_line[s_ws_uart_line_len] = '\0';
                ws_send_uart_rx_payload(s_ws_uart_line, s_ws_uart_line_len);
                s_ws_uart_line_len = 0;
            }
            continue;
        }

        if (!isprint(c) && c != '\t') {
            c = '.';
        }

        if (s_ws_uart_line_len >= (WS_UART_LINE_BUF_SZ - 1U)) {
            s_ws_uart_line[s_ws_uart_line_len] = '\0';
            ESP_LOGW(TAG_WEBSOCKET, "WS UART line too long, send truncated payload");
            ws_send_uart_rx_payload(s_ws_uart_line, s_ws_uart_line_len);
            s_ws_uart_line_len = 0;
        }

        s_ws_uart_line[s_ws_uart_line_len++] = (char)c;
    }
}

void WebSocket_Handler(void *pvParameter)
{
    ws_handler_ctx_t *ctx = (ws_handler_ctx_t *)pvParameter;
    if (ctx && ctx->ws) {
        s_ws_state = ctx->ws;
    }

    TickType_t last_wifi_tick = xTaskGetTickCount();
    TickType_t last_gateway_tick = xTaskGetTickCount();

    for (;;) {
        uart_node_rx_item_t uart_rx;

        if (ctx && ctx->uart && ctx->uart->uplink_queue) {
            TickType_t qwait = pdMS_TO_TICKS(100);
            if (xQueueReceive(ctx->uart->uplink_queue, &uart_rx, qwait) == pdTRUE) {
                do {
                    if (is_wifi_connected()) {
                        ws_send_uart_rx_item(&uart_rx);
                    }
                } while (xQueueReceive(ctx->uart->uplink_queue, &uart_rx, 0) ==
                         pdTRUE);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if ((xTaskGetTickCount() - last_wifi_tick) >= pdMS_TO_TICKS(1000)) {
            last_wifi_tick = xTaskGetTickCount();

            if (s_ws_restart_requested) {
                s_ws_restart_requested = false;
                if (client != NULL) {
                    ESP_LOGI(TAG_WEBSOCKET, "Restarting WebSocket client to apply new URL");
                    websocket_client_destroy_current();
                }
                if (s_ws_state) {
                    s_ws_state->connected = false;
                }
                screen_manager_set_ws_status(false);
            }

            if (is_wifi_connected()) {
                if (client == NULL) {
                    websocket_app_start();
                }
            } else {
                if (client != NULL) {
                    ESP_LOGW(TAG_WEBSOCKET,
                             "WiFi not connected — stop WebSocket");
                    websocket_client_destroy_current();
                    if (s_ws_state) {
                        s_ws_state->connected = false;
                    }
                }
            }
        }

        if (ctx != NULL &&
            (xTaskGetTickCount() - last_gateway_tick) >= pdMS_TO_TICKS(GATEWAY_STATUS_INTERVAL_MS)) {
            last_gateway_tick = xTaskGetTickCount();
            if (client != NULL && esp_websocket_client_is_connected(client) && is_wifi_connected()) {
                ws_send_gateway_status(ctx);
            }
        }
    }
}
