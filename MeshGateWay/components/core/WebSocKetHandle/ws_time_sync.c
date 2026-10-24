/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ws_time_sync.h"
#include "ws_transport.h"
#include "UartToNode.h"
#include "ds3231.h"
#include "esp_log.h"
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static volatile bool s_ws_time_synced = false;

/*
 * Hàm ws_handle_sync_time: Xử lý bản tin đồng bộ thời gian từ Server
 * và cập nhật cho cả ESP32 System Time và chip RTC DS3231
 */
void ws_handle_sync_time(cJSON *root) {
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
  if (ws_internal_client == NULL || !esp_websocket_client_is_connected(ws_internal_client)) {
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

bool websocket_is_time_synced(void) {
  return s_ws_time_synced;
}
