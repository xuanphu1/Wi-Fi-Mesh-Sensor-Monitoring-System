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
  APPLICATION_IMAGE,
  ACTUATOR_IMAGE,
  BATTERY_IMAGE,
  INFORMATION_IMAGE,
  WIFI_PREVIEW_IMAGE,
  MESH_PREVIEW_IMAGE,
} ImageType_t;

typedef enum {
  IMAGE_CATEGORY_WIFI = 0,
  IMAGE_CATEGORY_BATTERY,
  IMAGE_CATEGORY_DIFFERENT,
  IMAGE_CATEGORY_PREVIEW,
} ImageCategory_t;

extern const uint8_t **imageManager[];
extern const char **ManagerText[];
extern const char *MessageText[];

#endif