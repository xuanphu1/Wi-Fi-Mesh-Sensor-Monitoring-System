#ifndef MESH_TELEMETRY_H
#define MESH_TELEMETRY_H

#include "DataManager.h"
#include <stddef.h>
#include <stdint.h>

int mesh_telemetry_build(char *buffer, size_t capacity, DataManager_t *data,
                         uint32_t sequence);
void mesh_telemetry_extract_origin(const uint8_t *frame, size_t length,
                                   char *mac, size_t mac_capacity,
                                   uint32_t *sequence, int *level);

#endif /* MESH_TELEMETRY_H */
