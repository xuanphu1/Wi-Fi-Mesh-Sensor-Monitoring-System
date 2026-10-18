/**
 * @file mesh_telemetry.c
 * @brief Định dạng và đóng gói bản tin Telemetry (Mesh Telemetry Frame) gửi từ Node lên Root qua TCP.
 *
 * =========================================================================================
 * BẢN TIN TELEMETRY ĐỊNH KỲ (JSON FRAME):
 * Mỗi Node con định kỳ (CONFIG_MESH_TCP_SEND_INTERVAL_MS) sẽ đọc dữ liệu từ các cổng cảm biến,
 * dung lượng pin, trạng thái sạc nguồn và đóng gói thành chuỗi JSON kết thúc bằng '\n'.
 *
 * CÁC TRƯỜNG DỮ LIỆU RÚT GỌN (Tối ưu băng thông mạng Mesh):
 *   - "v"   : (int)     Phiên bản JSON schema (MESH_TELEMETRY_JSON_SCHEMA, mặc định = 1)
 *   - "seq" : (uint32)  Sequence number tăng dần của bản tin
 *   - "n"   : (int)     Cấp độ node trong Mesh (esp_mesh_lite_get_level(): 1=Root, 2=Level 2, ...)
 *   - "M"   : (string)  Địa chỉ MAC Wi-Fi STA của node gửi ("XX:XX:XX:XX:XX:XX")
 *   - "t"   : (string)  Thời gian lấy mẫu ISO-8601 ("YYYY-MM-DDTHH:MM:SS")
 *   - "ver" : (string)  Phiên bản firmware hiện hành của node ("X.Y.Z")
 *   - "b"   : (uint8)   Phần trăm pin còn lại (Battery Percent: 0 - 100%)
 *   - "c"   : (int)     Trạng thái sạc (Charging Status: 1 = Đang cắm sạc/nguồn ngoài, 0 = Dùng pin)
 *   - "err" : (array)   Mảng mã lỗi hex nếu có (ví dụ: ["0x0000"] hoặc rỗng [])
 *   - "p"   : (array)   Mảng dữ liệu các cổng cảm biến:
 *                       [ [port_id, sensor_type, val_1, val_2, ...], ... ]
 *                       + port_id: 1..4 (tương ứng PORT_1..PORT_4)
 *                       + sensor_type: ID của cảm biến (SensorType_t)
 *                       + val_1..val_n: Các giá trị float đo được (3 chữ số thập phân)
 *
 * VÍ DỤ BẢN TIN THỰC TẾ GỬI QUA TCP:
 * {
 *   "v": 1,
 *   "seq": 142,
 *   "n": 2,
 *   "M": "30:76:F5:A7:07:38",
 *   "t": "2026-09-23T23:22:29",
 *   "ver": "0.0.1",
 *   "b": 95,
 *   "c": 1,
 *   "err": ["0x0000"],
 *   "p": [
 *     [1, 2, 28.500, 65.200],
 *     [2, 1, 1013.250]
 *   ]
 * }
 * =========================================================================================
 */

#include "mesh_telemetry.h"

#include "ErrorCodes.h"
#include "SensorManager.h"
#include "SensorRegistry.h"
#include "TimeManager.h"
#include "esp_mac.h"
#include "esp_mesh_lite.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool append_format(char *buffer, size_t capacity, size_t *offset,
                          const char *format, ...) {
  if (buffer == NULL || offset == NULL || *offset >= capacity) {
    return false;
  }

  va_list args;
  va_start(args, format);
  int written = vsnprintf(buffer + *offset, capacity - *offset, format, args);
  va_end(args);
  if (written < 0 || (size_t)written >= capacity - *offset) {
    return false;
  }
  *offset += (size_t)written;
  return true;
}

static void get_timestamp(char value[20]) {
  snprintf(value, 20, "%s", "1970-01-01T00:00:00");
  if (TimeManager_GetTimestampStr(value, 20) == ESP_OK) {
    return;
  }

  time_t now = time(NULL);
  struct tm local_time;
  if (localtime_r(&now, &local_time) != NULL &&
      strftime(value, 20, "%Y-%m-%dT%H:%M:%S", &local_time) > 0) {
    return;
  }
  snprintf(value, 20, "%s", "1970-01-01T00:00:00");
}

int mesh_telemetry_build(char *buffer, size_t capacity, DataManager_t *data,
                         uint32_t sequence) {
  if (buffer == NULL || data == NULL || capacity < 64) {
    return -1;
  }

  char mac_string[18] = "00:00:00:00:00:00";
  char timestamp[20];
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    snprintf(mac_string, sizeof(mac_string),
             "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
  }
  get_timestamp(timestamp);

  uint8_t bat_percent = data->objectInfo.batteryInfo.levelPercent;
  if (bat_percent > 100) {
    bat_percent = 100;
  }
  int is_charging = data->objectInfo.batteryInfo.is_charging ? 1 : 0;

  size_t offset = 0;
  if (!append_format(buffer, capacity, &offset,
                     "{\"v\":%d,\"seq\":%" PRIu32
                     ",\"n\":%d,\"M\":\"%s\",\"t\":\"%s\","
                     "\"ver\":\"%u.%u.%u\","
                     "\"b\":%u,\"c\":%d,\"err\":[",
                     MESH_TELEMETRY_JSON_SCHEMA, sequence,
                     esp_mesh_lite_get_level(), mac_string, timestamp,
                     (unsigned)data->version[0], (unsigned)data->version[1],
                     (unsigned)data->version[2],
                     (unsigned)bat_percent, is_charging)) {
    return -1;
  }

  bool first_error = true;
  for (size_t i = 0; i < DATA_MANAGER_ERROR_CAPACITY; i++) {
    uint16_t code = data->error_code[i];
    if (code == MRS_OK) {
      continue;
    }
    if (!append_format(buffer, capacity, &offset, "%s\"0x%04X\"",
                       first_error ? "" : ",", (unsigned)code)) {
      return -1;
    }
    first_error = false;
  }

  if (!append_format(buffer, capacity, &offset, "],\"p\":[")) {
    return -1;
  }

  bool first_port = true;
  for (PortId_t port = PORT_1; port < NUM_PORTS; port++) {
    SensorType_t type = SENSOR_NONE;
    SensorData_t sensor_data = {0};
    system_err_t result =
        SensorManager_GetSnapshot(port, &type, &sensor_data);
    if (result != MRS_OK) {
      continue;
    }

    sensor_descriptor_t descriptor = {0};
    if (!sensor_registry_get_descriptor(type, &descriptor)) {
      continue;
    }

    if (!append_format(buffer, capacity, &offset, "%s[%d,%d",
                       first_port ? "" : ",", (int)port + 1, (int)type)) {
      return -1;
    }
    first_port = false;

    uint8_t field_count =
        descriptor.unit_count > 5 ? 5 : descriptor.unit_count;
    for (uint8_t field = 0; field < field_count; field++) {
      if (!append_format(buffer, capacity, &offset, ",%.3f",
                         sensor_data.data_fl[field])) {
        return -1;
      }
    }
    if (!append_format(buffer, capacity, &offset, "]")) {
      return -1;
    }
  }

  if (!append_format(buffer, capacity, &offset, "]}")) {
    return -1;
  }
  return (int)offset;
}

void mesh_telemetry_extract_origin(const uint8_t *frame, size_t length,
                                   char *mac, size_t mac_capacity,
                                   uint32_t *sequence, int *level) {
  if (mac != NULL && mac_capacity > 0) {
    mac[0] = '\0';
  }
  if (sequence != NULL) {
    *sequence = 0;
  }
  if (level != NULL) {
    *level = -1;
  }
  if (frame == NULL || length == 0) {
    return;
  }

  char json[MESH_TELEMETRY_FRAME_SIZE + 1];
  size_t copy_length = length > MESH_TELEMETRY_FRAME_SIZE
                           ? MESH_TELEMETRY_FRAME_SIZE
                           : length;
  memcpy(json, frame, copy_length);
  json[copy_length] = '\0';

  const char *mac_key = "\"M\":\"";
  char *position = strstr(json, mac_key);
  if (position != NULL && mac != NULL && mac_capacity > 0) {
    position += strlen(mac_key);
    size_t i = 0;
    while (position[i] != '\0' && position[i] != '"' &&
           i + 1 < mac_capacity) {
      mac[i] = position[i];
      i++;
    }
    mac[i] = '\0';
  }

  position = strstr(json, "\"seq\":");
  if (position != NULL && sequence != NULL) {
    *sequence = (uint32_t)strtoul(position + strlen("\"seq\":"), NULL, 10);
  }

  position = strstr(json, "\"n\":");
  if (position != NULL && level != NULL) {
    *level = (int)strtol(position + strlen("\"n\":"), NULL, 10);
  }
}
