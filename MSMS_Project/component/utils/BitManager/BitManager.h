#ifndef BIT_MANAGER_H
#define BIT_MANAGER_H
#include "DataManager.h"


typedef enum {
  MESSAGE_NONE = -1,
  MESSAGE_SENSOR_USED_OTHER_PORT = 0,
  MESSAGE_SENSOR_NOT_INITIALIZED,
  MESSAGE_PORT_SELECTED,
  MESSAGE_PORT_NOT_SELECTED,
} Message_t;

typedef enum {
NETWORK_CONFIG_IMAGE = 0,
SENSOR_IMAGE,
ACTUATOR_IMAGE,
BATTERY_IMAGE,
INFORMATION_IMAGE,
} ImageType_t;

extern const uint8_t **imageManager[];
extern const char **ManagerText[];
extern const char *MessageText[];

#endif