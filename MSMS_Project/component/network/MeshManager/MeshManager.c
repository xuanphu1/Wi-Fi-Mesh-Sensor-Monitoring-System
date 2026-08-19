#include "MeshManager.h"

#include "esp_log.h"
#include "esp_mesh_lite.h"
#include "freertos/queue.h"
#include "mesh_network.h"
#include "mesh_tcp_transport.h"
#include "sdkconfig.h"
#include <string.h>

#ifndef CONFIG_MESH_ROOT_FALLBACK_IP
#define CONFIG_MESH_ROOT_FALLBACK_IP "192.168.5.1"
#endif

static const char *TAG = "MeshManager";

typedef struct {
  DataManager_t *data;
  QueueHandle_t gateway_queue;
  mesh_tcp_transport_t transport;
  bool started;
} mesh_manager_context_t;

static mesh_manager_context_t s_manager;

static void push_error(DataManager_t *data, system_err_t error) {
  if (data != NULL && error != MRS_OK) {
    ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, error);
  }
}

void MeshManager_StartMesh(DataManager_t *data, mesh_role_t initial_role) {
  if (data == NULL) {
    return;
  }
  if (s_manager.started) {
    ESP_LOGI(TAG, "Mesh already started");
    return;
  }

  memset(&s_manager, 0, sizeof(s_manager));
  s_manager.data = data;
  data->meshIo.role = initial_role;
  data->meshIo.link_up = false;
  data->objectInfo.meshInfo.ipRoot = CONFIG_MESH_ROOT_FALLBACK_IP;

  s_manager.gateway_queue =
      xQueueCreate(MESH_GATEWAY_QUEUE_DEPTH, sizeof(mesh_gateway_frame_t));
  if (s_manager.gateway_queue == NULL) {
    push_error(data, MRS_ERR_CORE_OUT_OF_MEMORY);
    ESP_LOGE(TAG, "Create gateway queue failed");
    return;
  }

  esp_err_t result = mesh_network_start(initial_role);
  if (result != ESP_OK) {
    push_error(data, result);
    ESP_LOGE(TAG, "Start Mesh-Lite network failed: %s",
             esp_err_to_name(result));
    vQueueDelete(s_manager.gateway_queue);
    memset(&s_manager, 0, sizeof(s_manager));
    return;
  }

  result = mesh_tcp_transport_start(&s_manager.transport, data,
                                    s_manager.gateway_queue, initial_role);
  if (result != ESP_OK) {
    push_error(data, result);
    ESP_LOGE(TAG, "Start TCP transport failed: %s", esp_err_to_name(result));
    mesh_network_stop_runtime();
    vQueueDelete(s_manager.gateway_queue);
    memset(&s_manager, 0, sizeof(s_manager));
    return;
  }

  s_manager.started = true;
  ESP_LOGI(TAG, "Mesh TCP started as %s",
           initial_role == MESH_ROLE_ROOT ? "root" : "node");
}

void MeshManager_ResetState(void) {
  if (!s_manager.started) {
    return;
  }

  esp_err_t result = mesh_tcp_transport_stop(&s_manager.transport,
                                              pdMS_TO_TICKS(6000));
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "TCP transport stop timed out; preserving its resources");
    return;
  }

  mesh_network_stop_runtime();
  if (s_manager.gateway_queue != NULL) {
    vQueueDelete(s_manager.gateway_queue);
  }
  if (s_manager.data != NULL) {
    s_manager.data->meshIo.link_up = false;
  }
  memset(&s_manager, 0, sizeof(s_manager));
  ESP_LOGI(TAG, "Mesh runtime stopped");
}

bool MeshManager_SwitchRole(mesh_role_t role) {
  if (!s_manager.started || s_manager.data == NULL) {
    ESP_LOGW(TAG, "Role switch ignored: mesh is not started");
    return false;
  }
  if (role != MESH_ROLE_ROOT && role != MESH_ROLE_NODE) {
    return false;
  }
  if (s_manager.data->meshIo.role == role) {
    return true;
  }

  esp_err_t result = mesh_network_set_role(role);
  if (result != ESP_OK) {
    push_error(s_manager.data, result);
    ESP_LOGE(TAG, "Set Mesh-Lite role failed: %s", esp_err_to_name(result));
    return false;
  }

  s_manager.data->meshIo.role = role;
  mesh_tcp_transport_set_role(&s_manager.transport, role);
  ESP_LOGI(TAG, "Requested role %s",
           role == MESH_ROLE_ROOT ? "root" : "node");
  return true;
}

bool MeshManager_IsStarted(void) { return s_manager.started; }

bool MeshManager_IsConnected(void) {
  return s_manager.started && esp_mesh_lite_get_level() > 0;
}

mesh_role_t MeshManager_GetRole(void) {
  if (s_manager.data == NULL) {
    return MESH_ROLE_NODE;
  }
  return s_manager.data->meshIo.role;
}

uint8_t MeshManager_GetConnectedNodeCount(void) {
  if (!s_manager.started || MeshManager_GetRole() != MESH_ROLE_ROOT) {
    return 0;
  }
  return mesh_tcp_transport_client_count(&s_manager.transport);
}

void MeshManager_GetGatewayStats(mesh_gateway_stats_t *out_stats) {
  mesh_tcp_transport_get_stats(s_manager.started ? &s_manager.transport : NULL,
                               out_stats);
}

void MeshManager_GetGatewayQueueUsage(UBaseType_t *used, UBaseType_t *total) {
  UBaseType_t local_used = 0;
  UBaseType_t local_total = 0;
  if (s_manager.gateway_queue != NULL) {
    local_used = uxQueueMessagesWaiting(s_manager.gateway_queue);
    local_total = local_used + uxQueueSpacesAvailable(s_manager.gateway_queue);
  }
  if (used != NULL) {
    *used = local_used;
  }
  if (total != NULL) {
    *total = local_total;
  }
}

bool MeshManager_ReceiveGatewayFrame(mesh_gateway_frame_t *frame,
                                     TickType_t timeout) {
  if (frame == NULL || s_manager.gateway_queue == NULL ||
      MeshManager_GetRole() != MESH_ROLE_ROOT) {
    return false;
  }
  return xQueueReceive(s_manager.gateway_queue, frame, timeout) == pdTRUE;
}

int mesh_manager_get_throughput(void) { return 167; }
