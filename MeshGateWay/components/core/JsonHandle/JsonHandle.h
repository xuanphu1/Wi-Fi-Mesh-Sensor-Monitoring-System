#ifndef JSON_HANDLE_H
#define JSON_HANDLE_H

#include <stdint.h>
#include <stdbool.h>
#include "Datamanager.h"

#include "LogicHandle.h"

typedef enum {
    TYPE_OTA = 0,
    TYPE_UNKNOWN = 1,
} MessageType;

typedef struct {
    int index;
    int percent;
    char *line;
} OTAData;

/** Trạng thái parse command điều khiển (reserved). */
typedef struct {
    bool reserved;
} deviceManager_t;

extern bool checkChangeStatus;

MessageType getMessageType(const char *Json);
bool parseOTAJson(const char *json, OTAData *data);
bool parseDeviceStatusJson(const char *Json, deviceManager_t *data);
#endif /* JSON_HANDLE_H */
