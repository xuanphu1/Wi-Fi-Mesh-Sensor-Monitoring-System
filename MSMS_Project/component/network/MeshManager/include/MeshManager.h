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

/** Callback for downstream frames received on Node from Root */
typedef void (*mesh_downstream_cb_t)(DataManager_t *data, const char *json_str, size_t len);

void MeshManager_RegisterDownstreamCallback(mesh_downstream_cb_t cb);

/**
 * @brief Broadcast a JSON sync_time frame from Root to all connected TCP Nodes.
 * @param json_str JSON string payload (without trailing newline).
 * @param len Length of the JSON string.
 * @return ESP_OK on success.
 */
esp_err_t MeshManager_BroadcastSyncTime(const char *json_str, size_t len);

/**
 * @brief Broadcast a JSON OTA command (ota_start) from Root to all connected TCP Nodes.
 * @param json_str JSON string payload.
 * @param len Length of the JSON string.
 * @return ESP_OK on success.
 */
esp_err_t MeshManager_BroadcastOtaCommand(const char *json_str, size_t len);

int mesh_manager_get_throughput(void);

/**
 * @brief Pause or resume sensor telemetry transmissions over Mesh TCP.
 *        Used during FOTA/LAN OTA to reserve 100% bandwidth and CPU for OTA transfer.
 */
void MeshManager_SetTelemetryPaused(bool paused);
bool MeshManager_IsTelemetryPaused(void);

/**
 * @brief Send a raw JSON frame from a Child Node to Root over Mesh TCP.
 * @param json_str JSON string payload.
 * @param len Length of the JSON string.
 * @return ESP_OK on success.
 */
esp_err_t MeshManager_SendFrameToRoot(const char *json_str, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MESH_MANAGER_H */
