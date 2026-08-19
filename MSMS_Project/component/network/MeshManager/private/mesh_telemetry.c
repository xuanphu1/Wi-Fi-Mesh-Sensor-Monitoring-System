#include "mesh_telemetry.h"

#include "ErrorCodes.h"
#include "SensorRegistry.h"
#include "TimeManager.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_mesh_lite.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "MeshTelemetry";

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

  size_t offset = 0;
  if (!append_format(buffer, capacity, &offset,
                     "{\"v\":%d,\"seq\":%" PRIu32
                     ",\"n\":%d,\"M\":\"%s\",\"t\":\"%s\","
                     "\"ver\":\"%u.%u.%u\",\"err\":[",
                     MESH_TELEMETRY_JSON_SCHEMA, sequence,
                     esp_mesh_lite_get_level(), mac_string, timestamp,
                     (unsigned)data->version[0], (unsigned)data->version[1],
                     (unsigned)data->version[2])) {
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
    SensorType_t type = data->selectedSensor[port];
    if (type == SENSOR_NONE) {
      continue;
    }

    sensor_driver_t *driver = sensor_registry_get_driver(type);
    if (driver == NULL || driver->read == NULL) {
      ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY,
                           MRS_ERR_CORE_INVALID_PARAM);
      continue;
    }

    SensorData_t sensor_data = {0};
    system_err_t result = driver->read(&sensor_data);
    if (result != MRS_OK) {
      ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY,
                           result);
      ESP_LOGW(TAG, "Port %d read failed: %s", (int)port,
               system_err_to_name(result));
      continue;
    }
    data->port_data[port] = sensor_data;

    if (!append_format(buffer, capacity, &offset, "%s[%d,%d",
                       first_port ? "" : ",", (int)port + 1, (int)type)) {
      return -1;
    }
    first_port = false;

    uint8_t field_count = driver->unit_count > 5 ? 5 : driver->unit_count;
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
