#ifndef MESH_NETWORK_H
#define MESH_NETWORK_H

#include "DataManager.h"
#include "esp_err.h"

esp_err_t mesh_network_start(mesh_role_t role);
esp_err_t mesh_network_set_role(mesh_role_t role);
void mesh_network_stop_runtime(void);

#endif /* MESH_NETWORK_H */
