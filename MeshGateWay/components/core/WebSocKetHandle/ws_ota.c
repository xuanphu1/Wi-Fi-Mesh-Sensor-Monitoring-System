/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ws_ota.h"
#include "ws_transport.h"
#include "FOTAManager.h"
#include "UartToNode.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

static volatile bool s_node_ota_pending = false;
static TickType_t s_node_ota_start_tick = 0;
static char s_node_ota_job_id[64] = {0};
static char s_node_ota_target[32] = {0};
static char s_node_ota_target_detail[64] = {0};
static uint32_t s_node_ota_expected_size = 0;
static char s_gateway_ota_version[32] = {0};

void ws_ota_progress_callback(const char *job_id, const char *status,
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

void ws_send_ota_gateway_progress(void) {
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

bool ws_json_type_matches(const cJSON *type) {
  if (!cJSON_IsString(type) || type->valuestring == NULL) {
    return false;
  }
  return strcmp(type->valuestring, "ota_start") == 0 ||
         strcmp(type->valuestring, "ota") == 0 ||
         strcmp(type->valuestring, "ota_gateway") == 0 ||
         strcmp(type->valuestring, "gateway_ota") == 0;
}

const char *ws_json_get_ota_url(cJSON *root) {
  static const char *keys[] = {"url", "ota_url", "firmware_url", "line"};
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    cJSON *item = cJSON_GetObjectItem(root, keys[i]);
    if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
      return item->valuestring;
    }
  }
  return NULL;
}

void ws_normalize_ota_url(const char *raw_url, char *out_url, size_t out_len) {
  if (!raw_url || !out_url || out_len == 0) {
    return;
  }

  char temp_url[256] = {0};
  bool is_relative = (raw_url[0] == '/');
  bool is_loopback = (strstr(raw_url, "localhost") != NULL || strstr(raw_url, "127.0.0.1") != NULL);

  // Nếu là relative path HOẶC URL chứa localhost/127.0.0.1 -> thay thế bằng Host thực tế theo quy luật
  if (is_relative || is_loopback) {
    const char *path = ws_transport_get_url_path(raw_url);
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
  const char *resolved = ws_transport_resolve_local_url(temp_url);
  if (resolved != NULL) {
    strncpy(out_url, resolved, out_len - 1);
  } else {
    strncpy(out_url, temp_url, out_len - 1);
  }
  out_url[out_len - 1] = '\0';
}

void ws_handle_ota_command(cJSON *root) {
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
                             "Starting Gateway OTA...");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Với TFT thuần, RAM trống >100KB nên giữ nguyên kết nối WebSocket để truyền tiến độ OTA liên tục về Cloud Web UI
    s_gateway_ota_stop_ws = false;

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

void ws_ota_handle_node_progress_from_uart(const char *payload) {
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

void ws_ota_check_node_timeout(void) {
  if (s_node_ota_pending && ws_internal_client != NULL && esp_websocket_client_is_connected(ws_internal_client)) {
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
}
