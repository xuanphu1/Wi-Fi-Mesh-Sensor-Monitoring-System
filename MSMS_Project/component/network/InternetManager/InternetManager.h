#ifndef INTERNET_MANAGER_H
#define INTERNET_MANAGER_H

#include <stdbool.h>
#include "DataManager.h"
#include "ErrorCodes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  INTERNET_MODE_NONE = 0,
  INTERNET_MODE_WIFI,
  INTERNET_MODE_MESH,
} internet_mode_t;

typedef struct {
  internet_mode_t mode;
  bool transitioning;
  bool mesh_started;
  bool mesh_connected;
  status wifi_status;
  status mesh_status;
  system_err_t last_error;
} InternetManagerStatus_t;

system_err_t InternetManager_Init(DataManager_t *data, internet_mode_t mode);
system_err_t InternetManager_SwitchMode(DataManager_t *data, internet_mode_t mode);
system_err_t InternetManager_Clean(DataManager_t *data, bool destroy_default_netifs);
system_err_t InternetManager_GetStatus(DataManager_t *data, InternetManagerStatus_t *out);

internet_mode_t InternetManager_GetMode(void);
bool InternetManager_IsTransitioning(void);
const char *InternetManager_ModeName(internet_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* INTERNET_MANAGER_H */
