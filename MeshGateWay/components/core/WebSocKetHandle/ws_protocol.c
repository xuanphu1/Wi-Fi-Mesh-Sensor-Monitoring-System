/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ws_protocol.h"
#include "ws_transport.h"
#include "ws_time_sync.h"
#include "ws_ota.h"
#include "WifiManager.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char *gateway_power_source_str(uint32_t pack_mv,
                                     bool have_voltage) {
  if (!have_voltage || pack_mv < 2000U) {
    return "unknown";
  }
  /* Heuristic: nguồn ngoài/USB thường đẩy điện áp gói cao hơn khi đang nạp —
   * tinh chỉnh theo mạch thực tế. */
  if (pack_mv >= 4180U) {
    return "AC_or_USB";
  }
  return "battery";
}

void ws_send_gateway_status(ws_handler_ctx_t *ctx) {
  if (ws_internal_client == NULL || !esp_websocket_client_is_connected(ws_internal_client) ||
      ctx == NULL || ctx->metrics == NULL) {
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
   * --- Payload WebSocket: gateway_status (JSON text, gửi định kỳ ~5s khi
   * WS+STA OK) --- Đồng bộ schema phía server/UI: number = JSON number, string
   * = string, null = không có dữ liệu.
   *
   * {
   *   "type":               string, luôn "gateway_status".
   *   "clientType":         string, luôn "esp32".
   *
   *   "cpu_load_percent":   number, tải CPU 0..100 (%), suy ra từ
   * cpu_load_permille / 10 (SystemMonitor). "ram_used_kb":        number, RAM
   * đã dùng (kB, heap internal). "ram_used_percent":   number, % RAM đã dùng
   * 0..100 (= 100 - ram_free_percent từ metrics).
   *
   *   "battery_voltage_v":  number, điện áp gói pin ước lượng (V), từ ADC chia
   * áp (0 nếu chưa đo). "battery_percent":    number, % pin 0..100.
   *   "power_source":       string, "unknown" | "AC_or_USB" | "battery"
   * (heuristic, gateway_power_source_str).
   *
   *   "chip_temp_c":        number | null, °C nếu chip_temp_valid; else null.
   *   "chip_temp_internal_supported": bool, true chỉ khi SoC có driver TSENS
   * trong IDF (CONFIG_SOC_TEMP_SENSOR_SUPPORTED). ESP32 classic = false nên
   * chip_temp_c luôn null dù firmware đúng.
   *
   *   "uptime_s":           number, giây từ boot/reset (SystemMonitor /
   * esp_timer).
   *
   *   "wifi_ssid":          string, SSID AP đang bám; rỗng nếu không đọc được.
   *   "wifi_rssi":          number, RSSI dBm (âm), từ esp_wifi_sta_get_ap_info; 0 nếu không có AP.
   *   "sta_ip":             string, IPv4 STA "x.x.x.x"; rỗng nếu chưa có.
   *   "sta_gateway":        string, gateway IPv4 STA; rỗng nếu không đọc được.
   *   "mac":                string, địa chỉ MAC chuẩn STA "XX:XX:XX:XX:XX:XX".
   *   "sta_mac":            string, alias MAC STA (đồng bộ với backend/simulator).
   * }
   */
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return;
  }

  cJSON_AddStringToObject(root, "type", "gateway_status");
  cJSON_AddStringToObject(root, "clientType", "esp32");
  cJSON_AddNumberToObject(root, "cpu_load_percent",
                          (double)m.cpu_load_permille / 10.0);

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
  cJSON_AddStringToObject(root, "power_source",
                          gateway_power_source_str(m.battery_pack_mv, have_v));

  cJSON_AddBoolToObject(root, "chip_temp_internal_supported",
                        m.chip_temp_internal_supported ? 1 : 0);
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

  char mac_buf[20] = "";
  wifi_manager_get_mac_info(mac_buf, sizeof(mac_buf));
  cJSON_AddStringToObject(root, "mac", mac_buf);
  cJSON_AddStringToObject(root, "sta_mac", mac_buf);

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json_str != NULL) {
    ws_send_text_locked(json_str, strlen(json_str), pdMS_TO_TICKS(2000));
    free(json_str);
  }
}

void SendSignalRegister(void) {
  if (ws_internal_client == NULL) {
    return;
  }
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "type", "register");
  cJSON_AddStringToObject(data, "clientType", "esp32");

  char *json_str = cJSON_PrintUnformatted(data);
  if (json_str) {
    ws_send_text_locked(json_str, strlen(json_str), pdMS_TO_TICKS(2000));
    free(json_str);
  }
  cJSON_Delete(data);

  // Yêu cầu đồng bộ thời gian ngay sau khi đăng ký kết nối
  ws_send_time_sync_request();
}

/** Gửi một dòng UART đã ghép hoàn chỉnh lên WS. */
void ws_send_uart_rx_payload(const char *payload, size_t payload_len) {
  if (!ws_internal_client || !payload || payload_len == 0) {
    return;
  }
  if (!esp_websocket_client_is_connected(ws_internal_client)) {
    return;
  }

  // Trim khoảng trắng ở đầu và cuối nếu có
  while (payload_len > 0 && isspace((unsigned char)payload[0])) {
    payload++;
    payload_len--;
  }
  while (payload_len > 0 && isspace((unsigned char)payload[payload_len - 1])) {
    payload_len--;
  }
  if (payload_len == 0) {
    return;
  }

  // Nếu payload UART là chuỗi JSON (bản tin cảm biến, ota_progress từ node...), gửi trực tiếp
  if (payload[0] == '{' && payload[payload_len - 1] == '}') {
    // Nếu là bản tin ota_progress từ Node/Root, cập nhật trạng thái timeout và giao diện PanelOTA
    if (strstr(payload, "ota_progress") != NULL) {
      ws_ota_handle_node_progress_from_uart(payload);
    }

    ws_send_text_locked(payload, payload_len, pdMS_TO_TICKS(2000));
    return;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "type", "uart_rx");
  cJSON_AddNumberToObject(root, "len", (double)payload_len);
  cJSON_AddStringToObject(root, "payload", payload);

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json_str) {
    ws_send_text_locked(json_str, strlen(json_str), pdMS_TO_TICKS(2000));
    free(json_str);
  }
}

/** Nhận chunk UART từ queue, ghép theo '\n' rồi mới gửi WS để tránh cắt bản tin. */
void ws_send_uart_rx_item(const uart_node_rx_item_t *item) {
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
