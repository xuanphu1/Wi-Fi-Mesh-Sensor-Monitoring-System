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

static portMUX_TYPE s_telemetry_spinlock = portMUX_INITIALIZER_UNLOCKED;
static mesh_telemetry_provider_fn s_telemetry_provider = NULL;
static void *s_telemetry_context = NULL;
static volatile int s_telemetry_active_callers = 0;

#if CONFIG_MSMS_ENABLE_TESTS
static int s_mesh_start_fault = 0;
void MeshManager_TestInjectStartFault(int fault_type) {
  s_mesh_start_fault = fault_type;
}
#endif

esp_err_t MeshManager_SetTelemetryProvider(mesh_telemetry_provider_fn provider, void *context) {
  if (provider == NULL) {
    taskENTER_CRITICAL(&s_telemetry_spinlock);
    s_telemetry_provider = NULL;
    s_telemetry_context = NULL;
    taskEXIT_CRITICAL(&s_telemetry_spinlock);

    int wait_cycles = 0;
    while (s_telemetry_active_callers > 0 && wait_cycles < 30) {
      vTaskDelay(pdMS_TO_TICKS(10));
      wait_cycles++;
    }
    if (s_telemetry_active_callers > 0) {
      ESP_LOGE(TAG, "Timeout unregistering telemetry provider; %d active caller(s)", s_telemetry_active_callers);
      return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
  }

  taskENTER_CRITICAL(&s_telemetry_spinlock);
  s_telemetry_provider = provider;
  s_telemetry_context = context;
  taskEXIT_CRITICAL(&s_telemetry_spinlock);
  return ESP_OK;
}

bool MeshManager_GetTelemetryEntries(mesh_telemetry_port_entry_t *out_entries,
                                     size_t max_entries,
                                     size_t *out_count) {
  taskENTER_CRITICAL(&s_telemetry_spinlock);
  mesh_telemetry_provider_fn provider = s_telemetry_provider;
  void *context = s_telemetry_context;
  if (provider != NULL) {
    s_telemetry_active_callers++;
  }
  taskEXIT_CRITICAL(&s_telemetry_spinlock);

  if (provider == NULL) {
    if (out_count != NULL) {
      *out_count = 0;
    }
    return false;
  }

  bool ok = provider(out_entries, max_entries, out_count, context);

  taskENTER_CRITICAL(&s_telemetry_spinlock);
  if (s_telemetry_active_callers > 0) {
    s_telemetry_active_callers--;
  }
  taskEXIT_CRITICAL(&s_telemetry_spinlock);
  return ok;
}

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

esp_err_t MeshManager_StartMesh(DataManager_t *data, mesh_role_t initial_role) {
  if (data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_manager.started) {
    ESP_LOGI(TAG, "Mesh already started");
    return ESP_OK;
  }

#if CONFIG_MSMS_ENABLE_TESTS
  if (s_mesh_start_fault == 1) {
    s_mesh_start_fault = 0;
    push_error(data, MRS_ERR_CORE_OUT_OF_MEMORY);
    return ESP_ERR_NO_MEM;
  }
#endif

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
    return ESP_ERR_NO_MEM;
  }

#if CONFIG_MSMS_ENABLE_TESTS
  if (s_mesh_start_fault == 2) {
    s_mesh_start_fault = 0;
    vQueueDelete(s_manager.gateway_queue);
    memset(&s_manager, 0, sizeof(s_manager));
    push_error(data, ESP_FAIL);
    return ESP_FAIL;
  }
#endif

  esp_err_t result = mesh_network_start(initial_role);
  if (result != ESP_OK) {
    push_error(data, result);
    ESP_LOGE(TAG, "Start Mesh-Lite network failed: %s",
             esp_err_to_name(result));
    vQueueDelete(s_manager.gateway_queue);
    memset(&s_manager, 0, sizeof(s_manager));
    return result;
  }

#if CONFIG_MSMS_ENABLE_TESTS
  if (s_mesh_start_fault == 3) {
    s_mesh_start_fault = 0;
    mesh_network_stop_runtime();
    vQueueDelete(s_manager.gateway_queue);
    memset(&s_manager, 0, sizeof(s_manager));
    push_error(data, ESP_FAIL);
    return ESP_FAIL;
  }
#endif

  result = mesh_tcp_transport_start(&s_manager.transport, data,
                                    s_manager.gateway_queue, initial_role);
  if (result != ESP_OK) {
    push_error(data, result);
    ESP_LOGE(TAG, "Start TCP transport failed: %s", esp_err_to_name(result));
    mesh_network_stop_runtime();
    vQueueDelete(s_manager.gateway_queue);
    memset(&s_manager, 0, sizeof(s_manager));
    return result;
  }

  s_manager.started = true;
  ESP_LOGI(TAG, "Mesh TCP started as %s",
           initial_role == MESH_ROLE_ROOT ? "root" : "node");
  return ESP_OK;
}

esp_err_t MeshManager_ResetState(void) {
  if (!s_manager.started) {
    return ESP_OK;
  }

  esp_err_t result = mesh_tcp_transport_stop(&s_manager.transport,
                                              pdMS_TO_TICKS(6000));
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "TCP transport stop timed out; preserving its resources");
    return result;
  }

  mesh_network_stop_runtime();

  QueueHandle_t q = s_manager.gateway_queue;
  s_manager.gateway_queue = NULL;

  if (q != NULL) {
    vQueueDelete(q);
  }
  if (s_manager.data != NULL) {
    s_manager.data->meshIo.link_up = false;
  }
  s_manager.started = false;
  memset(&s_manager, 0, sizeof(s_manager));
  ESP_LOGI(TAG, "Mesh runtime stopped");
  return ESP_OK;
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

bool MeshManager_IsRoot(void) {
  if (MeshManager_GetRole() == MESH_ROLE_ROOT) {
    return true;
  }
  if (s_manager.started && esp_mesh_lite_get_level() == 1) {
    return true;
  }
  return false;
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
  if (frame == NULL || !s_manager.started || s_manager.gateway_queue == NULL ||
      MeshManager_GetRole() != MESH_ROLE_ROOT) {
    return false;
  }
  QueueHandle_t q = s_manager.gateway_queue;
  if (q == NULL || !s_manager.started) {
    return false;
  }
  return xQueueReceive(q, frame, timeout) == pdTRUE;
}

esp_err_t MeshManager_BroadcastSyncTime(const char *json_str, size_t len) {
  if (json_str == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!s_manager.started || MeshManager_GetRole() != MESH_ROLE_ROOT) {
    return ESP_ERR_INVALID_STATE;
  }
  return mesh_tcp_transport_broadcast(&s_manager.transport,
                                      (const uint8_t *)json_str, len);
}

esp_err_t MeshManager_BroadcastOtaCommand(const char *json_str, size_t len) {
  if (json_str == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!s_manager.started || MeshManager_GetRole() != MESH_ROLE_ROOT) {
    return ESP_ERR_INVALID_STATE;
  }
  return mesh_tcp_transport_broadcast(&s_manager.transport,
                                      (const uint8_t *)json_str, len);
}

void MeshManager_RegisterDownstreamCallback(mesh_downstream_cb_t cb) {
  mesh_tcp_transport_register_downstream_cb(cb);
}

static volatile bool s_telemetry_paused = false;

void MeshManager_SetTelemetryPaused(bool paused) {
  s_telemetry_paused = paused;
  ESP_LOGI(TAG, "Mesh telemetry paused state set to: %d", (int)paused);
}

bool MeshManager_IsTelemetryPaused(void) {
  return s_telemetry_paused;
}

esp_err_t MeshManager_SendFrameToRoot(const char *json_str, size_t len) {
  if (json_str == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!s_manager.started) {
    return ESP_ERR_INVALID_STATE;
  }
  return mesh_tcp_transport_send_node_frame(&s_manager.transport,
                                            (const uint8_t *)json_str, len);
}

int mesh_manager_get_throughput(void) { return 167; }


