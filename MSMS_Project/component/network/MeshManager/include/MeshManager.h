#ifndef MESH_MANAGER_H
#define MESH_MANAGER_H

#include "DataManager.h"
#include "ErrorCodes.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A complete JSON telemetry frame. The TCP newline delimiter is removed. */
typedef struct {
  uint32_t src_ip;
  uint16_t len;
  uint8_t data[MESH_TELEMETRY_FRAME_SIZE];
} mesh_gateway_frame_t;

typedef struct {
  uint32_t rx_frames;
  uint32_t queued_frames;
  uint32_t dropped_frames;
  uint32_t tx_frames;
  uint32_t reconnects;
} mesh_gateway_stats_t;

void MeshManager_StartMesh(DataManager_t *data, mesh_role_t initial_role);
void MeshManager_ResetState(void);
bool MeshManager_SwitchRole(mesh_role_t role);

bool MeshManager_IsStarted(void);
bool MeshManager_IsConnected(void);
mesh_role_t MeshManager_GetRole(void);
uint8_t MeshManager_GetConnectedNodeCount(void);

void MeshManager_GetGatewayStats(mesh_gateway_stats_t *out_stats);
void MeshManager_GetGatewayQueueUsage(UBaseType_t *used, UBaseType_t *total);
bool MeshManager_ReceiveGatewayFrame(mesh_gateway_frame_t *frame,
                                     TickType_t timeout);

int mesh_manager_get_throughput(void);

#ifdef __cplusplus
}
#endif

#endif /* MESH_MANAGER_H */
