/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "WSHandle.h"

#include "FOTAManager.h"
#include "WifiManager.h"
#include "UartToNode.h"
#include "ds3231.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "cJSON.h"

extern void screen_manager_set_ota_progress(bool active, uint8_t percent, const char *target, const char *version, const char *detail) __attribute__((weak));

static esp_websocket_client_handle_t client = NULL;

static ws_handler_ctx_t *s_ws_ctx = NULL;
static dm_ws_t *s_ws_state = NULL;
static dm_telemetry_t *s_ws_telemetry = NULL;
static char ws_last_type[32] = {0};
static char ws_last_version[32] = {0};
static char ws_url[128] = {0};
static volatile bool s_ws_restart_requested = false;
#if defined(CONFIG_WS_TARGET_SERVER)
static websocket_target_t s_ws_selected_target = WEBSOCKET_TARGET_SERVER;
#elif defined(CONFIG_WS_TARGET_LOCAL)
static websocket_target_t s_ws_selected_target = WEBSOCKET_TARGET_LOCAL;
#else
static websocket_target_t s_ws_selected_target = WEBSOCKET_TARGET_LOCAL;
#endif
static volatile uint32_t s_ws_reconnect_count = 0;
static volatile bool s_ws_connected_once = false;
static volatile bool s_ws_reconnect_pending = false;
static volatile bool s_ws_time_synced = false;
static volatile bool s_gateway_ota_stop_ws = false;

static volatile bool s_node_ota_pending = false;
static TickType_t s_node_ota_start_tick = 0;
static char s_node_ota_job_id[64] = {0};
static char s_node_ota_target[32] = {0};
static char s_node_ota_target_detail[64] = {0};
static uint32_t s_node_ota_expected_size = 0;
static char s_gateway_ota_version[32] = {0};

#define TAG_WEBSOCKET "WebSocket Handler"
#define GATEWAY_STATUS_INTERVAL_MS 5000
#define WS_RECONNECT_TIMEOUT_MS 15000
#define WS_NETWORK_TIMEOUT_MS 15000
#define WS_MDNS_QUERY_TIMEOUT_MS 3000
#define WS_OTA_PROGRESS_INTERVAL_MS 1000
#define WS_NODE_OTA_TIMEOUT_MS 10000

static SemaphoreHandle_t s_ws_send_mutex = NULL;
static void websocket_client_destroy_current(void);

static int ws_send_text_locked(const char *text, size_t len, TickType_t timeout) {
  if (text == NULL || len == 0 || client == NULL || !esp_websocket_client_is_connected(client) || !is_wifi_connected()) {
    return -1;
  }
  if (s_ws_send_mutex == NULL) {
    s_ws_send_mutex = xSemaphoreCreateMutex();
  }
  if (s_ws_send_mutex == NULL || xSemaphoreTake(s_ws_send_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return -1;
  }
  int ret = -1;
  if (client != NULL && esp_websocket_client_is_connected(client) && is_wifi_connected()) {
    ret = esp_websocket_client_send_text(client, text, (int)len, timeout);
    if (ret >= 0 && s_ws_telemetry) {
      s_ws_telemetry->tx_packet_count++;
      s_ws_telemetry->tx_byte_count += (uint32_t)len;
    }
  }
  xSemaphoreGive(s_ws_send_mutex);
  return ret;
}

static bool ws_client_can_send(void) {
  return client != NULL && esp_websocket_client_is_connected(client) &&
         is_wifi_connected();
}

static void ws_ota_progress_callback(const char *job_id, const char *status,
                                     uint8_t percent, uint32_t bytes_read,
                                     uint32_t total_bytes,
                                     const char *running_part,
                                     const char *target_part,
                                     const char *msg) {
  if (screen_manager_set_ota_progress) {
    screen_manager_set_ota_progress(true, percent, "Gateway",
                                    s_gateway_ota_version[0] ? s_gateway_ota_version : "",
                                    status ? status : target_part);
  }

  if (!ws_client_can_send()) {
    return;
  }

  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return;
  }

  cJSON_AddStringToObject(root, "type", "ota_progress");
  cJSON_AddStringToObject(root, "target", "gateway");
  cJSON_AddStringToObject(root, "targetDetail", "gateway");
  if (job_id && job_id[0]) {
    cJSON_AddStringToObject(root, "jobId", job_id);
  }
  cJSON_AddStringToObject(root, "status", status ? status : "");
  cJSON_AddNumberToObject(root, "percent", (double)percent);
  cJSON_AddNumberToObject(root, "bytes_read", (double)bytes_read);
  cJSON_AddNumberToObject(root, "total_bytes", (double)total_bytes);
  cJSON_AddStringToObject(root, "running_partition", running_part ? running_part : "");
  cJSON_AddStringToObject(root, "target_partition", target_part ? target_part : "");
  if (msg && msg[0]) {
    cJSON_AddStringToObject(root, "message", msg);
  }

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json_str != NULL) {
    ws_send_text_locked(json_str, strlen(json_str), pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG_WEBSOCKET, "Reported OTA Progress: [%s %u%%] -> %s",
             status ? status : "", (unsigned)percent, target_part ? target_part : "");
    free(json_str);
  }
}

static void ws_send_ota_gateway_progress(void) {
  if (!ws_client_can_send()) {
    return;
  }

  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    return;
  }

  uint8_t percent = fota_get_progress_percent();
  bool running = fota_is_running();
  esp_err_t result = fota_get_last_result();

  cJSON_AddStringToObject(root, "type", "ota_gateway_progress");
  cJSON_AddStringToObject(root, "clientType", "esp32");
  cJSON_AddNumberToObject(root, "percent", (double)percent);
  cJSON_AddBoolToObject(root, "running", running ? 1 : 0);
  cJSON_AddNumberToObject(root, "result", (double)result);
  cJSON_AddStringToObject(root, "result_name", esp_err_to_name(result));

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json_str != NULL) {
    ws_send_text_locked(json_str, strlen(json_str), pdMS_TO_TICKS(1000));
    free(json_str);
  }
}

static bool ws_json_type_matches(const cJSON *type) {
  if (!cJSON_IsString(type) || type->valuestring == NULL) {
    return false;
  }
  return strcmp(type->valuestring, "ota_start") == 0 ||
         strcmp(type->valuestring, "ota") == 0 ||
         strcmp(type->valuestring, "ota_gateway") == 0 ||
         strcmp(type->valuestring, "gateway_ota") == 0;
}

static const char *ws_json_get_ota_url(cJSON *root) {
  static const char *keys[] = {"url", "ota_url", "firmware_url", "line"};
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    cJSON *item = cJSON_GetObjectItem(root, keys[i]);
    if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
      return item->valuestring;
    }
  }
  return NULL;
}

static const char *resolve_local_url(const char *url) {
  if (url == NULL || url[0] == '\0') {
    return url;
  }

  const char *scheme = NULL;
  size_t scheme_len = 0;
  if (strncmp(url, "ws://", 5) == 0) {
    scheme = "ws://";
    scheme_len = 5;
  } else if (strncmp(url, "wss://", 6) == 0) {
    scheme = "wss://";
    scheme_len = 6;
  } else if (strncmp(url, "http://", 7) == 0) {
    scheme = "http://";
    scheme_len = 7;
  } else if (strncmp(url, "https://", 8) == 0) {
    scheme = "https://";
    scheme_len = 8;
  } else {
    return url;
  }

  const char *local_suffix = ".local";
  const char *host_begin = url + scheme_len;
  const char *suffix = strstr(host_begin, local_suffix);
  if (suffix == NULL) {
    return url;
  }

  const char *tail = suffix + strlen(local_suffix);
  if (*tail != '\0' && *tail != ':' && *tail != '/') {
    return url;
  }

  const size_t hostname_len = (size_t)(suffix - host_begin);
  if (hostname_len == 0 || hostname_len >= 64) {
    ESP_LOGE(TAG_WEBSOCKET, "Invalid mDNS hostname in URL: %s", url);
    return NULL;
  }

  char hostname[64];
  memcpy(hostname, host_begin, hostname_len);
  hostname[hostname_len] = '\0';

  esp_ip4_addr_t address = {0};
  esp_err_t err = mdns_query_a(hostname, WS_MDNS_QUERY_TIMEOUT_MS, &address);
  if (err != ESP_OK) {
    ESP_LOGW(TAG_WEBSOCKET, "mDNS lookup failed for %s.local: %s", hostname,
             esp_err_to_name(err));
    return NULL;
  }

  static char s_resolved_url[256];
  int written = snprintf(s_resolved_url, sizeof(s_resolved_url),
                         "%s" IPSTR "%s", scheme, IP2STR(&address), tail);
  if (written < 0 || (size_t)written >= sizeof(s_resolved_url)) {
    ESP_LOGE(TAG_WEBSOCKET, "Resolved URL is too long");
    return NULL;
  }

  ESP_LOGI(TAG_WEBSOCKET, "mDNS resolved %s.local -> " IPSTR " (URL: %s)",
           hostname, IP2STR(&address), s_resolved_url);
  return s_resolved_url;
}

static const char *get_url_path(const char *url) {
  if (!url) return "/";
  if (url[0] == '/') return url;
  const char *p = strstr(url, "://");
  if (p) {
    p += 3;
    const char *slash = strchr(p, '/');
    if (slash) return slash;
  }
  return "/";
}

static void ws_normalize_ota_url(const char *raw_url, char *out_url, size_t out_len) {
  if (!raw_url || !out_url || out_len == 0) {
    return;
  }

  char temp_url[256] = {0};
  bool is_relative = (raw_url[0] == '/');
  bool is_loopback = (strstr(raw_url, "localhost") != NULL || strstr(raw_url, "127.0.0.1") != NULL);

  // Nếu là relative path HOẶC URL chứa localhost/127.0.0.1 -> thay thế bằng Host thực tế theo quy luật
  if (is_relative || is_loopback) {
    const char *path = get_url_path(raw_url);
    websocket_target_t target = websocket_get_selected_target();
    if (target == WEBSOCKET_TARGET_LOCAL) {
      snprintf(temp_url, sizeof(temp_url), "http://systemmsems.local:9090%s", path);
    } else if (target == WEBSOCKET_TARGET_SERVER) {
      snprintf(temp_url, sizeof(temp_url), "https://systemmsems.msems.click%s", path);
    } else {
      const char *cur_ws = get_ws_url();
      if (strncmp(cur_ws, "wss://", 6) == 0) {
        snprintf(temp_url, sizeof(temp_url), "https://%s%s", cur_ws + 6, path);
      } else if (strncmp(cur_ws, "ws://", 5) == 0) {
        snprintf(temp_url, sizeof(temp_url), "http://%s%s", cur_ws + 5, path);
      } else {
        snprintf(temp_url, sizeof(temp_url), "http://systemmsems.local:9090%s", path);
      }
      char *slash = strchr(temp_url + 8, '/');
      if (slash && slash != (temp_url + strlen(temp_url) - strlen(path))) {
        char final_buf[256];
        *slash = '\0';
        snprintf(final_buf, sizeof(final_buf), "%s%s", temp_url, path);
        strncpy(temp_url, final_buf, sizeof(temp_url) - 1);
      }
    }
  } else {
    // Đã là URL tuyệt đối -> chuyển đổi ws:// thành http:// hoặc wss:// thành https:// nếu có
    if (strncmp(raw_url, "ws://", 5) == 0) {
      snprintf(temp_url, sizeof(temp_url), "http://%s", raw_url + 5);
    } else if (strncmp(raw_url, "wss://", 6) == 0) {
      snprintf(temp_url, sizeof(temp_url), "https://%s", raw_url + 6);
    } else {
      strncpy(temp_url, raw_url, sizeof(temp_url) - 1);
    }
  }

  // Phân giải mDNS nếu URL chứa .local
  const char *resolved = resolve_local_url(temp_url);
  if (resolved != NULL) {
    strncpy(out_url, resolved, out_len - 1);
  } else {
    strncpy(out_url, temp_url, out_len - 1);
  }
  out_url[out_len - 1] = '\0';
}

static void ws_handle_ota_command(cJSON *root) {
  cJSON *j_target = cJSON_GetObjectItem(root, "target");
  const char *target_str = (cJSON_IsString(j_target) && j_target->valuestring) ? j_target->valuestring : "gateway";

  cJSON *j_target_detail = cJSON_GetObjectItem(root, "targetDetail");
  if (!j_target_detail) j_target_detail = cJSON_GetObjectItem(root, "target_detail");
  if (!j_target_detail) j_target_detail = cJSON_GetObjectItem(root, "targetMac");
  if (!j_target_detail) j_target_detail = cJSON_GetObjectItem(root, "mac");
  const char *target_detail = (cJSON_IsString(j_target_detail) && j_target_detail->valuestring)
                                  ? j_target_detail->valuestring
                                  : (strcasecmp(target_str, "gateway") == 0 ? "gateway" : "all");

  const char *raw_url = ws_json_get_ota_url(root);
  char final_url[256] = {0};
  if (raw_url != NULL && raw_url[0] != '\0') {
    ws_normalize_ota_url(raw_url, final_url, sizeof(final_url));
  }

  cJSON *j_job_id = cJSON_GetObjectItem(root, "jobId");
  if (!j_job_id) j_job_id = cJSON_GetObjectItem(root, "job_id");
  const char *job_id = (cJSON_IsString(j_job_id) && j_job_id->valuestring) ? j_job_id->valuestring : "";

  cJSON *j_ver = cJSON_GetObjectItem(root, "ver");
  if (!j_ver) j_ver = cJSON_GetObjectItem(root, "version");
  const char *version = (cJSON_IsString(j_ver) && j_ver->valuestring) ? j_ver->valuestring : "";

  uint32_t size = 0;
  cJSON *j_size = cJSON_GetObjectItem(root, "size");
  if (cJSON_IsNumber(j_size)) {
    size = (uint32_t)j_size->valuedouble;
  }

  cJSON *j_md5 = cJSON_GetObjectItem(root, "md5");
  const char *md5 = (cJSON_IsString(j_md5) && j_md5->valuestring) ? j_md5->valuestring : "";

  // 1. Trường hợp OTA cho GATEWAY (target == "gateway" hoặc targetDetail == "gateway")
  if (strcasecmp(target_str, "gateway") == 0 || strcasecmp(target_detail, "gateway") == 0) {
    fota_job_info_t job = {0};
    strncpy(job.url, final_url, sizeof(job.url) - 1);
    strncpy(job.job_id, job_id, sizeof(job.job_id) - 1);
    strncpy(job.version, version, sizeof(job.version) - 1);
    strncpy(job.target, "gateway", sizeof(job.target) - 1);
    strncpy(job.target_detail, "gateway", sizeof(job.target_detail) - 1);
    job.size = size;
    strncpy(job.md5, md5, sizeof(job.md5) - 1);

    strncpy(s_gateway_ota_version, version, sizeof(s_gateway_ota_version) - 1);
    s_gateway_ota_version[sizeof(s_gateway_ota_version) - 1] = '\0';

    if (screen_manager_set_ota_progress) {
      screen_manager_set_ota_progress(true, 0, "Gateway", version, "Starting...");
    }

    // Báo cho Server biết Gateway bắt đầu OTA
    ws_ota_progress_callback(job.job_id, "Downloading", 0, 0, job.size, "", "",
                             "Starting Gateway OTA, freeing TLS RAM...");
    vTaskDelay(pdMS_TO_TICKS(300));

    // Yêu cầu task ws_hdl đóng WebSocket client an toàn để giải phóng RAM TLS cho OTA
    s_gateway_ota_stop_ws = true;

    esp_err_t ret = fota_start_gateway_ota_with_info(&job);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG_WEBSOCKET, "Gateway OTA task launched successfully!");
    } else {
      ESP_LOGE(TAG_WEBSOCKET, "Gateway OTA task launch failed: %s",
               esp_err_to_name(ret));
      if (screen_manager_set_ota_progress) {
        screen_manager_set_ota_progress(false, 0, "Gateway", version, "Launch Failed");
      }
    }
    return;
  }

  // 2. Trường hợp OTA cho NODE / ROOT / MESH (chuyển tiếp lệnh xuống Root qua UART)
  ESP_LOGW(TAG_WEBSOCKET,
           "Forwarding OTA command to Root Node via UART: target=%s, targetDetail=%s, jobId=%s, url=%s",
           target_str, target_detail, job_id, final_url[0] ? final_url : "none (Mesh internal)");

  // Thiết lập timeout 10s theo dõi phản hồi OTA của Node
  s_node_ota_pending = true;
  s_node_ota_start_tick = xTaskGetTickCount();
  strncpy(s_node_ota_job_id, job_id, sizeof(s_node_ota_job_id) - 1);
  s_node_ota_job_id[sizeof(s_node_ota_job_id) - 1] = '\0';
  strncpy(s_node_ota_target, target_str, sizeof(s_node_ota_target) - 1);
  s_node_ota_target[sizeof(s_node_ota_target) - 1] = '\0';
  strncpy(s_node_ota_target_detail, target_detail, sizeof(s_node_ota_target_detail) - 1);
  s_node_ota_target_detail[sizeof(s_node_ota_target_detail) - 1] = '\0';
  s_node_ota_expected_size = size;

  if (screen_manager_set_ota_progress) {
    screen_manager_set_ota_progress(true, 0, target_str, version, target_detail);
  }

  esp_err_t ret = uart_to_node_send_ota_start(target_str, target_detail, job_id,
                                              (final_url[0] ? final_url : NULL),
                                              version, size, md5);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG_WEBSOCKET, "Failed to send OTA command to Root via UART: %s", esp_err_to_name(ret));
    ws_ota_progress_callback(job_id, "Failed", 0, 0, size, "", "", "UART send failed");
    s_node_ota_pending = false;
    if (screen_manager_set_ota_progress) {
      screen_manager_set_ota_progress(false, 0, target_str, version, "Failed");
    }
  }
}

static void websocket_client_destroy_current(void) {
  if (client == NULL) {
    return;
  }
  if (s_ws_send_mutex != NULL) {
    xSemaphoreTake(s_ws_send_mutex, pdMS_TO_TICKS(500));
  }
  esp_websocket_client_handle_t temp_client = client;
  client = NULL;
  if (s_ws_send_mutex != NULL) {
    xSemaphoreGive(s_ws_send_mutex);
  }
  esp_err_t err = esp_websocket_client_destroy(temp_client);
  if (err != ESP_OK) {
    ESP_LOGW(TAG_WEBSOCKET, "WebSocket destroy failed: %s",
             esp_err_to_name(err));
  }
}

static const char *gateway_power_source_str(uint32_t pack_mv,
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

static void ws_send_gateway_status(ws_handler_ctx_t *ctx) {
  if (client == NULL || !esp_websocket_client_is_connected(client) ||
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
   *   "wifi_rssi":          number, RSSI dBm (âm), từ esp_wifi_sta_get_ap_info;
   * 0 nếu không có AP. "sta_ip":             string, IPv4 STA "x.x.x.x"; rỗng
   * nếu chưa có. "sta_gateway":        string, gateway IPv4 STA; rỗng nếu không
   * đọc được.
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

  char *json_str = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json_str != NULL) {
    // ESP_LOGI(TAG_WEBSOCKET, "gateway_status send: %s", json_str);
    ws_send_text_locked(json_str, strlen(json_str), pdMS_TO_TICKS(2000));
    free(json_str);
  }
}

void websocket_attach_state(dm_ws_t *ws_state) { s_ws_state = ws_state; }

/** Chỉ RAM — không ghi NVS/flash; mất khi reset. */
esp_err_t save_ws_url(const char *url) {
  if (url == NULL || url[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }

  strncpy(ws_url, url, sizeof(ws_url) - 1);
  ws_url[sizeof(ws_url) - 1] = '\0';
  s_ws_selected_target = WEBSOCKET_TARGET_CUSTOM;
  ESP_LOGI(TAG_WEBSOCKET, "WebSocket URL (RAM): %s", ws_url);

  s_ws_restart_requested = true;

  if (s_ws_state) {
    strncpy(s_ws_state->url_cached, ws_url, sizeof(s_ws_state->url_cached) - 1);
    s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
  }

  return ESP_OK;
}

esp_err_t websocket_select_target(websocket_target_t target) {
  if (target != WEBSOCKET_TARGET_CUSTOM && target != WEBSOCKET_TARGET_LOCAL &&
      target != WEBSOCKET_TARGET_SERVER) {
    return ESP_ERR_INVALID_ARG;
  }

  s_ws_selected_target = target;
  s_ws_restart_requested = true;

  ESP_LOGI(TAG_WEBSOCKET, "WebSocket target selected (RAM): %s",
           target == WEBSOCKET_TARGET_LOCAL
               ? "local"
               : (target == WEBSOCKET_TARGET_SERVER ? "server" : "custom"));

  if (s_ws_state) {
    const char *url = get_ws_url();
    strncpy(s_ws_state->url_cached, url, sizeof(s_ws_state->url_cached) - 1);
    s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
  }

  return ESP_OK;
}

websocket_target_t websocket_get_selected_target(void) {
  return s_ws_selected_target;
}

const char *get_ws_url(void) {
  if (s_ws_selected_target == WEBSOCKET_TARGET_LOCAL) {
#if defined(CONFIG_WS_LOCAL_URL)
    return CONFIG_WS_LOCAL_URL;
#else
    return "ws://systemmsems.local:9090/ws";
#endif
  }

  if (s_ws_selected_target == WEBSOCKET_TARGET_SERVER) {
#if defined(CONFIG_WS_SERVER_URL)
    return CONFIG_WS_SERVER_URL;
#else
    return "wss://systemmsems.msems.click/ws";
#endif
  }

  if (ws_url[0] != '\0') {
    return ws_url;
  }
#if defined(CONFIG_WS_TARGET_LOCAL) && defined(CONFIG_WS_LOCAL_URL)
  return CONFIG_WS_LOCAL_URL;
#elif defined(CONFIG_WS_TARGET_SERVER) && defined(CONFIG_WS_SERVER_URL)
  return CONFIG_WS_SERVER_URL;
#elif defined(CONFIG_WS_URL)
  /* Backward compatibility with an existing sdkconfig. */
  return CONFIG_WS_URL;
#else
  return "wss://systemmsems.msems.click/ws";
#endif
}

/*
 * Hàm ws_handle_sync_time: Xử lý bản tin đồng bộ thời gian từ Server
 * và cập nhật cho cả ESP32 System Time và chip RTC DS3231
 */
static void ws_handle_sync_time(cJSON *root) {
  if (root == NULL) {
    return;
  }

  time_t sec = 0;
  bool have_time = false;
  struct tm tm_info = {0};

  cJSON *j_ts = cJSON_GetObjectItem(root, "timestamp");
  if (!j_ts)
    j_ts = cJSON_GetObjectItem(root, "timestamp_ms");
  if (!j_ts)
    j_ts = cJSON_GetObjectItem(root, "time");
  if (!j_ts)
    j_ts = cJSON_GetObjectItem(root, "epoch");
  if (!j_ts)
    j_ts = cJSON_GetObjectItem(root, "timer");
  if (!j_ts)
    j_ts = cJSON_GetObjectItem(root, "datetime");

  // Kiểm tra nếu payload nằm trong object con "data" hoặc "payload"
  if (!j_ts) {
    cJSON *nested = cJSON_GetObjectItem(root, "data");
    if (!nested)
      nested = cJSON_GetObjectItem(root, "payload");
    if (nested && cJSON_IsObject(nested)) {
      j_ts = cJSON_GetObjectItem(nested, "timestamp");
      if (!j_ts)
        j_ts = cJSON_GetObjectItem(nested, "timestamp_ms");
      if (!j_ts)
        j_ts = cJSON_GetObjectItem(nested, "time");
      if (!j_ts)
        j_ts = cJSON_GetObjectItem(nested, "epoch");
      if (!j_ts)
        j_ts = cJSON_GetObjectItem(nested, "timer");
      if (!j_ts)
        j_ts = cJSON_GetObjectItem(nested, "datetime");
    }
  }

  if (cJSON_IsNumber(j_ts)) {
    double val = j_ts->valuedouble;
    if (val > 1e11) {
      sec = (time_t)(val / 1000.0);
    } else {
      sec = (time_t)val;
    }
    have_time = true;
  } else if (cJSON_IsString(j_ts) && j_ts->valuestring) {
    const char *str = j_ts->valuestring;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    if (sscanf(str, "%d-%d-%d%*c%d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6 ||
        sscanf(str, "%d/%d/%d%*c%d:%d:%d", &d, &mo, &y, &h, &mi, &s) == 6) {
      tm_info.tm_year = (y >= 1900) ? y - 1900 : y + 100;
      tm_info.tm_mon = mo - 1;
      tm_info.tm_mday = d;
      tm_info.tm_hour = h;
      tm_info.tm_min = mi;
      tm_info.tm_sec = s;
      sec = mktime(&tm_info);
      have_time = true;
    } else {
      double val = atof(str);
      if (val > 1e11) {
        sec = (time_t)(val / 1000.0);
      } else {
        sec = (time_t)val;
      }
      if (sec > 1000000000) {
        have_time = true;
      }
    }
  }

  cJSON *j_year = cJSON_GetObjectItem(root, "year");
  cJSON *j_month = cJSON_GetObjectItem(root, "month");
  cJSON *j_day = cJSON_GetObjectItem(root, "day");
  cJSON *j_hour = cJSON_GetObjectItem(root, "hour");
  cJSON *j_min = cJSON_GetObjectItem(root, "min");
  if (!j_min)
    j_min = cJSON_GetObjectItem(root, "minute");
  cJSON *j_sec = cJSON_GetObjectItem(root, "sec");
  if (!j_sec)
    j_sec = cJSON_GetObjectItem(root, "second");

  if (!have_time && cJSON_IsNumber(j_year) && cJSON_IsNumber(j_month) &&
      cJSON_IsNumber(j_day)) {
    tm_info.tm_year = j_year->valueint - 1900;
    tm_info.tm_mon = j_month->valueint - 1;
    tm_info.tm_mday = j_day->valueint;
    tm_info.tm_hour = cJSON_IsNumber(j_hour) ? j_hour->valueint : 0;
    tm_info.tm_min = cJSON_IsNumber(j_min) ? j_min->valueint : 0;
    tm_info.tm_sec = cJSON_IsNumber(j_sec) ? j_sec->valueint : 0;
    sec = mktime(&tm_info);
    have_time = true;
  }

  if (!have_time || sec <= 1000000000) {
    ESP_LOGW(TAG_WEBSOCKET,
             "Time sync payload received but timestamp is invalid");
    return;
  }

  // 1. Cập nhật ESP32 System Time
  struct timeval tv = {.tv_sec = sec, .tv_usec = 0};
  settimeofday(&tv, NULL);

  localtime_r(&sec, &tm_info);

  char time_buf[64];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);
  ESP_LOGI(TAG_WEBSOCKET,
           "System time synchronized via WebSocket: %s (epoch: %ld)",
           time_buf, (long)sec);

  // 2. Cập nhật phần cứng chip RTC DS3231
  if (s_ws_ctx && s_ws_ctx->hw && s_ws_ctx->hw->rtc_ready) {
    esp_err_t err = ds3231_set_time(&s_ws_ctx->hw->rtc_dev, &tm_info);
    if (err == ESP_OK) {
      ESP_LOGI(TAG_WEBSOCKET,
               "DS3231 RTC hardware updated from WebSocket time sync: %s",
               time_buf);
    } else {
      ESP_LOGW(TAG_WEBSOCKET, "Failed to update DS3231 RTC: %s",
               esp_err_to_name(err));
    }
  }

  // 3. Tự động gửi thời gian vừa đồng bộ xuống cho Root Node qua UART
  uart_to_node_send_sync_time();

  s_ws_time_synced = true;
}

void ws_send_time_sync_request(void) {
  if (client == NULL || !esp_websocket_client_is_connected(client)) {
    return;
  }

  cJSON *time_req = cJSON_CreateObject();
  cJSON_AddStringToObject(time_req, "type", "sync_time");
  cJSON_AddStringToObject(time_req, "clientType", "esp32");
  cJSON_AddStringToObject(time_req, "action", "get_time");
  char *time_req_str = cJSON_PrintUnformatted(time_req);
  if (time_req_str) {
    ws_send_text_locked(time_req_str, strlen(time_req_str), pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG_WEBSOCKET, "Sent time sync request to server: %s",
             time_req_str);
    free(time_req_str);
  }
  cJSON_Delete(time_req);
}

void SendSignalRegister(void) {
  if (client == NULL) {
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

static void websocket_event_handler(void *arg, esp_event_base_t base,
                                    int32_t event_id, void *event_data) {
  (void)arg;
  (void)base;
  esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

  switch (event_id) {

  case WEBSOCKET_EVENT_CONNECTED:
    ESP_LOGI(TAG_WEBSOCKET, "Connected to server");
    if (s_ws_connected_once && s_ws_reconnect_pending) {
      s_ws_reconnect_count++;
    }
    s_ws_connected_once = true;
    s_ws_reconnect_pending = false;
    if (s_ws_state) {
      s_ws_state->connected = true;
    }
    SendSignalRegister();
    break;

  case WEBSOCKET_EVENT_DISCONNECTED:
    ESP_LOGW(TAG_WEBSOCKET, "Disconnected from server");
    if (s_ws_connected_once) {
      s_ws_reconnect_pending = true;
    }
    if (s_ws_state) {
      s_ws_state->connected = false;
    }
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
          if (ws_json_type_matches(j_type)) {
            ws_handle_ota_command(root);
          }
          if (cJSON_IsString(j_type)) {
            const char *t_str = j_type->valuestring;
            strncpy(ws_last_type, t_str, sizeof(ws_last_type) - 1);
            ws_last_type[sizeof(ws_last_type) - 1] = '\0';

            if (strcmp(t_str, "sync_time") == 0 ||
                strcmp(t_str, "time_sync") == 0 || strcmp(t_str, "time") == 0 ||
                strcmp(t_str, "timer") == 0 || strcmp(t_str, "set_time") == 0 ||
                strcmp(t_str, "get_time") == 0 ||
                strcmp(t_str, "time_response") == 0 ||
                strcmp(t_str, "request_time_sync") == 0) {
              ws_handle_sync_time(root);
            }
          }
          if (cJSON_IsString(j_version)) {
            strncpy(ws_last_version, j_version->valuestring,
                    sizeof(ws_last_version) - 1);
            ws_last_version[sizeof(ws_last_version) - 1] = '\0';
            ESP_LOGI(TAG_WEBSOCKET,
                     "Server version: %s (FOTA stub — không gọi OTA)",
                     ws_last_version);
          }
          cJSON_Delete(root);
        }
        free(json_buf);
      }
    }
    break;

  case WEBSOCKET_EVENT_ERROR:
    ESP_LOGE(TAG_WEBSOCKET, "WebSocket error occurred");
    if (s_ws_connected_once) {
      s_ws_reconnect_pending = true;
    }
    if (s_ws_state) {
      s_ws_state->connected = false;
    }
    break;
  }
}

static void websocket_app_start(void) {
  const char *configured_url = get_ws_url();
  const char *url_to_use = resolve_local_url(configured_url);
  if (url_to_use == NULL) {
    return;
  }
  const bool use_tls = strncmp(url_to_use, "wss://", 6) == 0;

  esp_websocket_client_config_t websocket_cfg = {
      .uri = url_to_use,
      .reconnect_timeout_ms = WS_RECONNECT_TIMEOUT_MS,
      .network_timeout_ms = WS_NETWORK_TIMEOUT_MS,
      .task_stack = 6144,
      .buffer_size = 2048,
      .keep_alive_enable = true,
      .crt_bundle_attach = use_tls ? esp_crt_bundle_attach : NULL,
  };

  ESP_LOGI(TAG_WEBSOCKET, "Starting WebSocket (%s) with URL: %s",
           use_tls ? "TLS" : "plain", url_to_use);

  if (s_ws_state) {
    strncpy(s_ws_state->url_cached, url_to_use,
            sizeof(s_ws_state->url_cached) - 1);
    s_ws_state->url_cached[sizeof(s_ws_state->url_cached) - 1] = '\0';
  }

  fota_register_progress_callback(ws_ota_progress_callback);

  client = esp_websocket_client_init(&websocket_cfg);
  if (client == NULL) {
    ESP_LOGE(TAG_WEBSOCKET, "esp_websocket_client_init failed");
    return;
  }
  esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY,
                                websocket_event_handler, NULL);
  esp_websocket_client_start(client);
  ESP_LOGI(TAG_WEBSOCKET, "WebSocket client start issued");
}

/** Gửi một dòng UART đã ghép hoàn chỉnh lên WS. */
static void ws_send_uart_rx_payload(const char *payload, size_t payload_len) {
  if (!client || !payload || payload_len == 0) {
    return;
  }
  if (!esp_websocket_client_is_connected(client)) {
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
      if (s_node_ota_pending) {
        if (strstr(payload, "\"Success\"") != NULL || strstr(payload, "\"Failed\"") != NULL ||
            strstr(payload, "\"status\":\"Success\"") != NULL || strstr(payload, "\"status\":\"Failed\"") != NULL) {
          s_node_ota_pending = false;
          ESP_LOGI(TAG_WEBSOCKET, "Node OTA completion status received from UART");
        } else {
          s_node_ota_start_tick = xTaskGetTickCount();
        }
      }

      cJSON *j_prog = cJSON_Parse(payload);
      if (j_prog) {
        cJSON *j_p = cJSON_GetObjectItem(j_prog, "percent");
        cJSON *j_t = cJSON_GetObjectItem(j_prog, "target");
        cJSON *j_td = cJSON_GetObjectItem(j_prog, "targetDetail");
        cJSON *j_s = cJSON_GetObjectItem(j_prog, "status");
        cJSON *j_v = cJSON_GetObjectItem(j_prog, "ver");
        uint8_t pct = (j_p && cJSON_IsNumber(j_p)) ? (uint8_t)j_p->valueint : 0;
        const char *t = (j_t && j_t->valuestring) ? j_t->valuestring : (s_node_ota_target[0] ? s_node_ota_target : "Node");
        const char *v = (j_v && j_v->valuestring) ? j_v->valuestring : "";
        const char *s = (j_s && j_s->valuestring) ? j_s->valuestring : "";
        const char *td = (j_td && j_td->valuestring) ? j_td->valuestring : (s_node_ota_target_detail[0] ? s_node_ota_target_detail : "");
        char detail_buf[128] = {0};
        if (s[0] && td[0]) {
          snprintf(detail_buf, sizeof(detail_buf), "%s (%s)", s, td);
        } else if (s[0]) {
          snprintf(detail_buf, sizeof(detail_buf), "%s", s);
        } else {
          snprintf(detail_buf, sizeof(detail_buf), "%s", td);
        }
        if (screen_manager_set_ota_progress) {
          screen_manager_set_ota_progress(true, pct, t, v, detail_buf);
        }
        cJSON_Delete(j_prog);
      }
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
    // ESP_LOGI(TAG_WEBSOCKET, "WebSocket send UART RX item: %s", json_str);
    free(json_str);
  }
}

/** Nhận chunk UART từ queue, ghép theo '\n' rồi mới gửi WS để tránh cắt bản
 * tin. */
static void ws_send_uart_rx_item(const uart_node_rx_item_t *item) {
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

void WebSocket_Handler(void *pvParameter) {
  ws_handler_ctx_t *ctx = (ws_handler_ctx_t *)pvParameter;
  s_ws_ctx = ctx;
  if (ctx && ctx->ws) {
    s_ws_state = ctx->ws;
  }
  if (ctx && ctx->telemetry) {
    s_ws_telemetry = ctx->telemetry;
  }

  TickType_t last_wifi_tick = xTaskGetTickCount();
  TickType_t last_gateway_tick = xTaskGetTickCount();
  TickType_t last_time_sync_tick = 0;
  TickType_t last_ota_progress_tick = 0;
  uint8_t last_ota_percent_sent = 255;
  bool last_ota_running_sent = false;

  for (;;) {
    if (s_gateway_ota_stop_ws) {
      s_gateway_ota_stop_ws = false;
      if (client != NULL) {
        ESP_LOGI(TAG_WEBSOCKET, "Safely stopping WebSocket client to free TLS RAM for Gateway OTA");
        websocket_client_destroy_current();
        if (s_ws_state) {
          s_ws_state->connected = false;
        }
      }
    }

    uart_node_rx_item_t uart_rx;

    if (!fota_is_running() && ctx && ctx->uart && ctx->uart->uplink_queue) {
      TickType_t qwait = pdMS_TO_TICKS(100);
      if (xQueueReceive(ctx->uart->uplink_queue, &uart_rx, qwait) == pdTRUE) {
        do {
          if (is_wifi_connected()) {
            ws_send_uart_rx_item(&uart_rx);
          }
        } while (xQueueReceive(ctx->uart->uplink_queue, &uart_rx, 0) == pdTRUE);
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    bool ota_running = fota_is_running();
    uint8_t ota_percent = fota_get_progress_percent();
    if ((ota_running || last_ota_running_sent ||
         ota_percent != last_ota_percent_sent) &&
        (xTaskGetTickCount() - last_ota_progress_tick) >=
            pdMS_TO_TICKS(WS_OTA_PROGRESS_INTERVAL_MS)) {
      last_ota_progress_tick = xTaskGetTickCount();
      ws_send_ota_gateway_progress();
      last_ota_percent_sent = ota_percent;
      last_ota_running_sent = ota_running;
    }

    if ((xTaskGetTickCount() - last_wifi_tick) >= pdMS_TO_TICKS(1000)) {
      last_wifi_tick = xTaskGetTickCount();

      if (s_ws_restart_requested) {
        s_ws_restart_requested = false;
        if (client != NULL) {
          ESP_LOGI(TAG_WEBSOCKET,
                   "Restarting WebSocket client to apply new URL");
          if (s_ws_connected_once) {
            s_ws_reconnect_pending = true;
          }
          websocket_client_destroy_current();
        }
        if (s_ws_state) {
          s_ws_state->connected = false;
        }
      }

      if (is_wifi_connected()) {
        if (client == NULL && !fota_is_running()) {
          websocket_app_start();
        }
      } else {
        if (client != NULL) {
          ESP_LOGW(TAG_WEBSOCKET, "WiFi not connected — stop WebSocket");
          websocket_client_destroy_current();
          if (s_ws_state) {
            s_ws_state->connected = false;
          }
        }
      }
    }

    if (ctx != NULL && (xTaskGetTickCount() - last_gateway_tick) >=
                           pdMS_TO_TICKS(GATEWAY_STATUS_INTERVAL_MS)) {
      last_gateway_tick = xTaskGetTickCount();
      if (!fota_is_running() && client != NULL &&
          esp_websocket_client_is_connected(client) && is_wifi_connected()) {
        ws_send_gateway_status(ctx);
      }
    }

    // Kiểm tra timeout 10s cho Node/Root OTA nếu không nhận được phản hồi tiến trình
    if (s_node_ota_pending && client != NULL && esp_websocket_client_is_connected(client)) {
      TickType_t now_tick = xTaskGetTickCount();
      if ((now_tick - s_node_ota_start_tick) >= pdMS_TO_TICKS(WS_NODE_OTA_TIMEOUT_MS)) {
        s_node_ota_pending = false;
        ESP_LOGE(TAG_WEBSOCKET, "Node/Root OTA timeout (10s) without response -> Sending Failed progress");

        if (screen_manager_set_ota_progress) {
          screen_manager_set_ota_progress(false, 0, s_node_ota_target[0] ? s_node_ota_target : "node", "", "Timeout (10s)");
        }

        cJSON *root = cJSON_CreateObject();
        if (root != NULL) {
          cJSON_AddStringToObject(root, "type", "ota_progress");
          cJSON_AddStringToObject(root, "target", s_node_ota_target[0] ? s_node_ota_target : "node");
          cJSON_AddStringToObject(root, "targetDetail", s_node_ota_target_detail[0] ? s_node_ota_target_detail : "all");
          if (s_node_ota_job_id[0] != '\0') {
            cJSON_AddStringToObject(root, "jobId", s_node_ota_job_id);
          }
          cJSON_AddStringToObject(root, "status", "Failed");
          cJSON_AddNumberToObject(root, "percent", 0.0);
          cJSON_AddNumberToObject(root, "bytes_read", 0.0);
          cJSON_AddNumberToObject(root, "total_bytes", (double)s_node_ota_expected_size);
          cJSON_AddStringToObject(root, "running_partition", "");
          cJSON_AddStringToObject(root, "target_partition", "");
          cJSON_AddStringToObject(root, "message", "Timeout: Node/Root did not respond within 10s");

          char *json_str = cJSON_PrintUnformatted(root);
          cJSON_Delete(root);
          if (json_str != NULL) {
            ws_send_text_locked(json_str, strlen(json_str), pdMS_TO_TICKS(2000));
            free(json_str);
          }
        }
      }
    }

    // Định kỳ gửi lại yêu cầu đồng bộ thời gian (30s nếu chưa sync, 1h nếu đã sync)
    uint32_t sync_interval_ms = s_ws_time_synced ? (3600 * 1000) : 30000;
    if ((xTaskGetTickCount() - last_time_sync_tick) >= pdMS_TO_TICKS(sync_interval_ms)) {
      last_time_sync_tick = xTaskGetTickCount();
      if (!fota_is_running() && client != NULL &&
          esp_websocket_client_is_connected(client) && is_wifi_connected()) {
        ws_send_time_sync_request();
      }
    }
  }
}

bool websocket_is_connected(void) {
  return s_ws_state != NULL && s_ws_state->connected;
}

uint32_t websocket_get_reconnect_count(void) { return s_ws_reconnect_count; }

bool websocket_is_time_synced(void) { return s_ws_time_synced; }
