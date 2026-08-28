#include "JsonHandle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

bool checkChangeStatus = false;

MessageType getMessageType(const char *Json)
{
    if (!Json) {
        return TYPE_UNKNOWN;
    }
    if (strstr(Json, "type\":\"ota\"")) {
        return TYPE_OTA;
    }
    return TYPE_UNKNOWN;
}

bool parseDeviceStatusJson(const char *Json, deviceManager_t *data)
{
    if (!Json || !data) {
        return false;
    }

    cJSON *root = cJSON_Parse(Json);
    if (!root) {
        printf("JSON parse failed\n");
        return false;
    }

    const cJSON *type = cJSON_GetObjectItem(root, "type");
    const cJSON *device = cJSON_GetObjectItem(root, "device");
    const cJSON *status = cJSON_GetObjectItem(root, "status");

    if (!cJSON_IsString(type) || strcmp(type->valuestring, "device-status") != 0 ||
        !cJSON_IsString(device) || !status) {
        printf("Invalid JSON format\n");
        cJSON_Delete(root);
        return false;
    }

    checkChangeStatus = false;

    const char *dev = device->valuestring;
    bool new_status = false;
    if (cJSON_IsBool(status)) {
        new_status = cJSON_IsTrue(status);
    } else if (cJSON_IsNumber(status)) {
        new_status = status->valueint ? true : false;
    } else {
        printf("status field is not boolean/number\n");
        cJSON_Delete(root);
        return false;
    }

    enum {
        DEVICE_UNKNOWN,
        DEVICE_OTA
    } deviceType = DEVICE_UNKNOWN;

    if (strcmp(dev, "OTA") == 0) {
        deviceType = DEVICE_OTA;
    }

    switch (deviceType) {
    case DEVICE_OTA:
        break;
    default:
        printf("Unknown device: %s\n", dev);
        cJSON_Delete(root);
        return false;
    }

    printf("Updated device: %s -> %d\n", dev, new_status);
    cJSON_Delete(root);
    return true;
}

bool parseOTAJson(const char *json, OTAData *data)
{
    if (!json || !data) {
        printf("Null parameter\n");
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        printf("Failed to parse JSON\n");
        return false;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *index = cJSON_GetObjectItem(root, "index");
    cJSON *percent = cJSON_GetObjectItem(root, "Percent");
    cJSON *length = cJSON_GetObjectItem(root, "length");
    cJSON *line = cJSON_GetObjectItem(root, "line");

    if (!cJSON_IsString(type) || strcmp(type->valuestring, "ota") != 0 ||
        !cJSON_IsNumber(index) || !cJSON_IsNumber(percent) || !cJSON_IsString(line)) {
        printf("Missing or invalid ota fields\n");
        cJSON_Delete(root);
        return false;
    }

    memset(data, 0, sizeof(*data));

    size_t type_len = strlen(type->valuestring);
    data->type = malloc(type_len + 1);
    if (!data->type) {
        printf("malloc failed for OTA type\n");
        cJSON_Delete(root);
        return false;
    }
    strcpy(data->type, type->valuestring);

    data->index = (uint16_t)index->valueint;
    data->percent = (uint8_t)percent->valueint;

    size_t line_len = strlen(line->valuestring);
    data->length = cJSON_IsNumber(length) ? (uint16_t)length->valueint : (uint16_t)line_len;
    data->line = malloc(line_len + 1);
    if (!data->line) {
        printf("malloc failed for OTA line\n");
        free(data->type);
        data->type = NULL;
        cJSON_Delete(root);
        return false;
    }
    strcpy(data->line, line->valuestring);

    cJSON_Delete(root);
    return true;
}
