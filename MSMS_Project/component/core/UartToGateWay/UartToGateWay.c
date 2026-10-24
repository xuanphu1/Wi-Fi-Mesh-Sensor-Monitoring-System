#include "UartToGateWay.h"

#include "DataManager.h"
#include "InternetManager.h"
#include "MeshManager.h"
#include "PinManager.h"
#include "ProcessingDataMesh.h"
#include "TimeManager.h"
#include "UartResourceManager.h"
#include "cJSON.h"
#include "driver/uart.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "soc/uart_reg.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifndef CONFIG_UART_GATEWAY_BAUD_RATE
#define CONFIG_UART_GATEWAY_BAUD_RATE 115200
#endif

#define TAG_UART_GATEWAY "UART_GATEWAY"
#define UART_RX_BUF_SIZE 256
#define UART_TX_BUF_SIZE 4096
#define UART_EVENT_QUEUE_LEN 16
#define UART_CMD_LINE_MAX 512
#define UART_RXFIFO_FULL_THRESH 64
#define UART_RX_TIMEOUT_THRESH 10
#define UART_GATEWAY_TX_RECV_WAIT_MS 50
#define UART_GATEWAY_TX_DRAIN_BATCH 64
#define UART_GATEWAY_TX_STATS_MS 1000

/* Event Group Bits for Quiesce / Resume / Stop Handshake */
#define UART_EVT_PAUSE_REQ    BIT0
#define UART_EVT_RX_QUIESCED  BIT1
#define UART_EVT_TX_QUIESCED  BIT2
#define UART_EVT_RESUME_REQ   BIT3
#define UART_EVT_STOP_REQ     BIT4
#define UART_EVT_RX_EXITED    BIT5
#define UART_EVT_TX_EXITED    BIT6

static QueueHandle_t s_uart_event_queue = NULL;
static DataManager_t *s_data = NULL;
static volatile uart_state_t s_uart_state = UART_STATE_UNINITIALIZED;
static bool s_uart_started = false;
static uart_port_t s_uart_num = UART_NUM_1;

static EventGroupHandle_t s_uart_event_group = NULL;
static SemaphoreHandle_t s_rx_exit_sem = NULL;
static SemaphoreHandle_t s_tx_exit_sem = NULL;
static TaskHandle_t s_rx_task_handle = NULL;
static TaskHandle_t s_tx_task_handle = NULL;
static uart_lease_t s_gateway_lease = {0};

uart_state_t UartToGateWay_GetState(void) {
  return s_uart_state;
}

#if CONFIG_MSMS_ENABLE_TESTS
static int s_uart_init_fault_stage = 0;
void UartToGateWay_TestInjectInitFault(int stage) {
  s_uart_init_fault_stage = stage;
}
#endif

/* Gateway heartbeat: when in ROOT mode, gateway must periodically send "Connected". */
static TickType_t s_last_connected_tick = 0;
static bool s_connected_watchdog_enabled = false;
/** Root: lần cuối gửi heartbeat "No node" khi queue mesh rỗng. */
static TickType_t s_mesh_gateway_no_node_tick = 0;
static TickType_t s_uart_gateway_last_stats_tick = 0;
static uint32_t s_uart_gateway_sent_frames = 0;
static uint32_t s_uart_gateway_sent_bytes = 0;
static uart_gateway_role_request_fn s_role_request_handler = NULL;
static void *s_role_request_context = NULL;
static uart_gateway_ota_handler_fn s_ota_handler = NULL;
static void *s_ota_context = NULL;

static bool uart_gateway_mesh_mode_active(void) {
  return InternetManager_GetMode() == INTERNET_MODE_MESH;
}

static bool equals_ignore_case(const char *a, const char *b) {
  while (*a != '\0' && *b != '\0') {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return false;
    }
    a++;
    b++;
  }
  return (*a == '\0') && (*b == '\0');
}

static void handle_gateway_line(char *line) {
  while (*line == ' ' || *line == '\t') line++;
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t' ||
                     line[len - 1] == '\r' || line[len - 1] == '\n')) {
    line[--len] = '\0';
  }
  if (len == 0) return;

  if (equals_ignore_case(line, "Connected")) {
    s_last_connected_tick = xTaskGetTickCount();
    s_connected_watchdog_enabled = true;
    ESP_LOGD(TAG_UART_GATEWAY, "Received Connected heartbeat from Gateway");
    return;
  }

  if (equals_ignore_case(line, "Start Root")) {
    s_last_connected_tick = xTaskGetTickCount();
    s_connected_watchdog_enabled = true;
    if (s_role_request_handler != NULL) {
      system_err_t err = s_role_request_handler(MESH_ROLE_ROOT, s_role_request_context);
      if (err != MRS_OK) {
        (void)UartToGateWay_Send("Role request rejected\r\n", 23);
        return;
      }
    } else {
      if (!MeshManager_SwitchRole(MESH_ROLE_ROOT)) {
        (void)UartToGateWay_Send("Mesh not ready\r\n", 16);
        return;
      }
    }
    (void)UartToGateWay_Send("Switching to root mode...\r\n", 27);
    return;
  }

  if (line[0] == '{') {
    cJSON *root = cJSON_Parse(line);
    if (root != NULL) {
      cJSON *type_item = cJSON_GetObjectItem(root, "type");
      if (type_item != NULL && cJSON_IsString(type_item) &&
          strcmp(type_item->valuestring, "time_sync") == 0) {
        s_last_connected_tick = xTaskGetTickCount();
        cJSON *time_item = cJSON_GetObjectItem(root, "time");
        if (time_item != NULL && cJSON_IsString(time_item)) {
          int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
          if (sscanf(time_item->valuestring, "%d-%d-%d %d:%d:%d", &year, &month,
                     &day, &hour, &minute, &second) == 6) {
            TimeManager_SetDateTime(year, month, day, hour, minute, second, 0);
            ESP_LOGI(TAG_UART_GATEWAY, "System time synced from Gateway: %s",
                     time_item->valuestring);
          }
          MeshManager_BroadcastSyncTime(line, len);
        }
        cJSON_Delete(root);
        return;
      } else if (type_item != NULL && cJSON_IsString(type_item) &&
                 strcmp(type_item->valuestring, "ota_start") == 0) {
        s_last_connected_tick = xTaskGetTickCount();
        ESP_LOGI(TAG_UART_GATEWAY, "Received ota_start from Gateway: %s", line);

        cJSON *ver_item = cJSON_GetObjectItem(root, "ver");
        const esp_app_desc_t *app_desc = esp_app_get_description();
        if (ver_item == NULL && app_desc != NULL) {
          cJSON_AddStringToObject(root, "ver", app_desc->version);
        }

        char *broadcast_json = cJSON_PrintUnformatted(root);
        if (broadcast_json != NULL) {
          size_t b_len = strlen(broadcast_json);
          MeshManager_BroadcastOtaCommand(broadcast_json, b_len);
          if (s_ota_handler != NULL) {
            s_ota_handler(s_data, broadcast_json, b_len, s_ota_context);
          }
          free(broadcast_json);
        } else {
          MeshManager_BroadcastOtaCommand(line, len);
          if (s_ota_handler != NULL) {
            s_ota_handler(s_data, line, len, s_ota_context);
          }
        }
        cJSON_Delete(root);
        return;
      }
      cJSON_Delete(root);
    }
  }

  ESP_LOGW(TAG_UART_GATEWAY, "Unknown UART cmd: '%s'", line);
}

static bool uart_gateway_send_mesh_msg(const mesh_gateway_frame_t *msg) {
  if (msg == NULL || msg->len == 0 || msg->len > sizeof(msg->data)) {
    return false;
  }

  char tx_buffer[1450];
  int sent = 0;
  ESP_LOGD(TAG_UART_GATEWAY, "UART TX payload: %.*s", (int)msg->len, msg->data);

  if (msg->len + 1U <= sizeof(tx_buffer)) {
    memcpy(tx_buffer, msg->data, msg->len);
    tx_buffer[msg->len] = '\n';
    sent = UartToGateWay_Send(tx_buffer, msg->len + 1U);
  } else {
    sent = UartToGateWay_Send(msg->data, msg->len);
    int newline_sent = UartToGateWay_Send("\n", 1);
    if (newline_sent > 0) {
      sent += newline_sent;
    }
  }
  if (sent > 0) {
    s_uart_gateway_sent_frames++;
    s_uart_gateway_sent_bytes += (uint32_t)sent;
  }
  return true;
}

static void uart_gateway_log_tx_stats(void) {
  TickType_t now = xTaskGetTickCount();
  if ((now - s_uart_gateway_last_stats_tick) <
      pdMS_TO_TICKS(UART_GATEWAY_TX_STATS_MS)) {
    return;
  }

  UBaseType_t queue_used = 0;
  UBaseType_t queue_total = 0;
  MeshManager_GetGatewayQueueUsage(&queue_used, &queue_total);

  ESP_LOGI(TAG_UART_GATEWAY,
           "UART gateway TX: frames=%" PRIu32 ", bytes=%" PRIu32
           ", queue=%u/%u",
           s_uart_gateway_sent_frames, s_uart_gateway_sent_bytes,
           (unsigned)queue_used, (unsigned)queue_total);
  s_uart_gateway_sent_frames = 0;
  s_uart_gateway_sent_bytes = 0;
  s_uart_gateway_last_stats_tick = now;
}

/** Nhận: queue sự kiện UART, đọc byte, ghép dòng lệnh gateway → ESP. */
static void uart_gateway_rx_task(void *pvParameter) {
  (void)pvParameter;

  uart_event_t event;
  uint8_t rx_data[UART_RX_BUF_SIZE];
  char line_buf[UART_CMD_LINE_MAX];
  size_t line_len = 0;

  for (;;) {
    EventBits_t bits = xEventGroupGetBits(s_uart_event_group);
    if (bits & UART_EVT_STOP_REQ) {
      break;
    }

    if (bits & UART_EVT_PAUSE_REQ) {
      line_len = 0;
      s_connected_watchdog_enabled = false;
      xEventGroupSetBits(s_uart_event_group, UART_EVT_RX_QUIESCED);

      EventBits_t wait_bits = xEventGroupWaitBits(
          s_uart_event_group,
          UART_EVT_RESUME_REQ | UART_EVT_STOP_REQ,
          pdFALSE,
          pdFALSE,
          portMAX_DELAY);

      if (wait_bits & UART_EVT_STOP_REQ) {
        break;
      }
      xEventGroupClearBits(s_uart_event_group, UART_EVT_RX_QUIESCED);
      continue;
    }

    if (!uart_gateway_mesh_mode_active()) {
      line_len = 0;
      s_connected_watchdog_enabled = false;
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    QueueHandle_t queue = s_uart_event_queue;
    if (queue == NULL ||
        xQueueReceive(queue, &event, pdMS_TO_TICKS(50)) != pdTRUE) {
      TickType_t now = xTaskGetTickCount();
      if (s_connected_watchdog_enabled) {
        if ((now - s_last_connected_tick) > pdMS_TO_TICKS(1000)) {
          ESP_LOGW(TAG_UART_GATEWAY, "No 'Connected' heartbeat from gateway for >1s -> switch to node");
          if (s_role_request_handler != NULL) {
            (void)s_role_request_handler(MESH_ROLE_NODE, s_role_request_context);
          }
          s_connected_watchdog_enabled = false;
          s_last_connected_tick = now;
        }
      }
      continue;
    }

    bits = xEventGroupGetBits(s_uart_event_group);
    if (bits & (UART_EVT_PAUSE_REQ | UART_EVT_STOP_REQ)) {
      continue;
    }

    switch (event.type) {
    case UART_DATA: {
      int read_len = uart_read_bytes(
          s_uart_num, rx_data,
          (event.size < UART_RX_BUF_SIZE) ? (int)event.size : UART_RX_BUF_SIZE,
          0);
      for (int i = 0; i < read_len; i++) {
        char c = (char)rx_data[i];
        if (c == '\r' || c == '\n') {
          if (line_len > 0) {
            line_buf[line_len] = '\0';
            handle_gateway_line(line_buf);
            line_len = 0;
          }
          continue;
        }
        if (line_len < (UART_CMD_LINE_MAX - 1)) {
          line_buf[line_len++] = c;
        } else {
          line_len = 0;
        }
      }
      break;
    }
    case UART_FIFO_OVF:
    case UART_BUFFER_FULL:
      ESP_LOGW(TAG_UART_GATEWAY, "UART RX overflow, flush input");
      uart_flush_input(s_uart_num);
      if (s_uart_event_queue != NULL) {
        xQueueReset(s_uart_event_queue);
      }
      break;
    default:
      break;
    }
  }

  xEventGroupSetBits(s_uart_event_group, UART_EVT_RX_EXITED);
  SemaphoreHandle_t rx_sem = s_rx_exit_sem;
  if (rx_sem != NULL) {
    xSemaphoreGive(rx_sem);
  }
  vTaskDelete(NULL);
}

static void uart_gateway_tx_task(void *pvParameter) {
  (void)pvParameter;

  for (;;) {
    EventBits_t bits = xEventGroupGetBits(s_uart_event_group);
    if (bits & UART_EVT_STOP_REQ) {
      break;
    }

    if (bits & UART_EVT_PAUSE_REQ) {
      xEventGroupSetBits(s_uart_event_group, UART_EVT_TX_QUIESCED);

      EventBits_t wait_bits = xEventGroupWaitBits(
          s_uart_event_group,
          UART_EVT_RESUME_REQ | UART_EVT_STOP_REQ,
          pdFALSE,
          pdFALSE,
          portMAX_DELAY);

      if (wait_bits & UART_EVT_STOP_REQ) {
        break;
      }
      xEventGroupClearBits(s_uart_event_group, UART_EVT_TX_QUIESCED);
      continue;
    }

    if (!uart_gateway_mesh_mode_active()) {
      vTaskDelay(pdMS_TO_TICKS(UART_GATEWAY_TX_RECV_WAIT_MS));
      continue;
    }

    if (s_data == NULL || MeshManager_GetRole() != MESH_ROLE_ROOT) {
      vTaskDelay(pdMS_TO_TICKS(UART_GATEWAY_TX_RECV_WAIT_MS));
      continue;
    }

    mesh_gateway_frame_t msg;
    if (!MeshManager_ReceiveGatewayFrame(&msg, pdMS_TO_TICKS(UART_GATEWAY_TX_RECV_WAIT_MS))) {
      TickType_t now = xTaskGetTickCount();
      if ((now - s_mesh_gateway_no_node_tick) >= pdMS_TO_TICKS(MESH_ROOT_UART_NO_NODE_MS)) {
        s_mesh_gateway_no_node_tick = now;
        (void)UartToGateWay_Send("No node\n", 8);
      }
      continue;
    }

    (void)uart_gateway_send_mesh_msg(&msg);
    s_mesh_gateway_no_node_tick = xTaskGetTickCount();

    for (int drained = 1; drained < UART_GATEWAY_TX_DRAIN_BATCH; drained++) {
      if (!uart_gateway_mesh_mode_active() || s_data == NULL ||
          MeshManager_GetRole() != MESH_ROLE_ROOT) {
        break;
      }
      if (!MeshManager_ReceiveGatewayFrame(&msg, 0)) {
        break;
      }
      (void)uart_gateway_send_mesh_msg(&msg);
      s_mesh_gateway_no_node_tick = xTaskGetTickCount();
    }
    uart_gateway_log_tx_stats();
  }

  xEventGroupSetBits(s_uart_event_group, UART_EVT_TX_EXITED);
  SemaphoreHandle_t tx_sem = s_tx_exit_sem;
  if (tx_sem != NULL) {
    xSemaphoreGive(tx_sem);
  }
  vTaskDelete(NULL);
}

static system_err_t gateway_quiesce_cb(void *context, TickType_t timeout_ticks) {
  (void)context;
  if (s_uart_event_group == NULL) {
    return MRS_ERR_CORE_NOT_INITIALIZED;
  }

  xEventGroupClearBits(s_uart_event_group, UART_EVT_RX_QUIESCED | UART_EVT_TX_QUIESCED | UART_EVT_RESUME_REQ);
  xEventGroupSetBits(s_uart_event_group, UART_EVT_PAUSE_REQ);

  // Wake up RX task if blocked in xQueueReceive
  if (s_uart_event_queue != NULL) {
    uart_event_t dummy = { .type = UART_EVENT_MAX };
    (void)xQueueSend(s_uart_event_queue, &dummy, 0);
  }

  EventBits_t ack_bits = xEventGroupWaitBits(
      s_uart_event_group,
      UART_EVT_RX_QUIESCED | UART_EVT_TX_QUIESCED,
      pdFALSE,
      pdTRUE,
      timeout_ticks);

  if ((ack_bits & (UART_EVT_RX_QUIESCED | UART_EVT_TX_QUIESCED)) !=
      (UART_EVT_RX_QUIESCED | UART_EVT_TX_QUIESCED)) {
    ESP_LOGE(TAG_UART_GATEWAY,
             "Gateway quiesce timeout! ack_bits=0x%lx (RX_QUIESCED=%d, TX_QUIESCED=%d)",
             (unsigned long)ack_bits,
             (ack_bits & UART_EVT_RX_QUIESCED) ? 1 : 0,
             (ack_bits & UART_EVT_TX_QUIESCED) ? 1 : 0);

    if (s_rx_task_handle != NULL) {
      ESP_LOGE(TAG_UART_GATEWAY, "RX task state=%d, stack_watermark=%u",
               (int)eTaskGetState(s_rx_task_handle),
               (unsigned)uxTaskGetStackHighWaterMark(s_rx_task_handle));
    }
    if (s_tx_task_handle != NULL) {
      ESP_LOGE(TAG_UART_GATEWAY, "TX task state=%d, stack_watermark=%u",
               (int)eTaskGetState(s_tx_task_handle),
               (unsigned)uxTaskGetStackHighWaterMark(s_tx_task_handle));
    }

    xEventGroupClearBits(s_uart_event_group, UART_EVT_PAUSE_REQ);
    xEventGroupSetBits(s_uart_event_group, UART_EVT_RESUME_REQ);
    return MRS_ERR_CORE_TIMEOUT;
  }

  s_uart_event_queue = NULL;
  s_uart_state = UART_STATE_PAUSED;
  return MRS_OK;
}

static system_err_t gateway_resume_cb(void *context, uart_port_t port, QueueHandle_t queue) {
  (void)context;
  (void)port;

  s_uart_event_queue = queue;

  xEventGroupClearBits(s_uart_event_group, UART_EVT_PAUSE_REQ | UART_EVT_RX_QUIESCED | UART_EVT_TX_QUIESCED);
  xEventGroupSetBits(s_uart_event_group, UART_EVT_RESUME_REQ);

  s_uart_state = UART_STATE_RUNNING;
  ESP_LOGI(TAG_UART_GATEWAY, "Gateway tasks resumed with new queue");
  return MRS_OK;
}

static system_err_t uart_gateway_stop_tasks(TickType_t timeout_ticks) {
  if (s_uart_event_group != NULL) {
    xEventGroupSetBits(s_uart_event_group, UART_EVT_STOP_REQ);
  }

  if (s_uart_event_queue != NULL) {
    uart_event_t dummy = { .type = UART_EVENT_MAX };
    (void)xQueueSend(s_uart_event_queue, &dummy, 0);
  }

  bool all_stopped = true;

  if (s_rx_task_handle != NULL) {
    if (s_rx_exit_sem != NULL && xSemaphoreTake(s_rx_exit_sem, timeout_ticks) == pdTRUE) {
      s_rx_task_handle = NULL;
    } else {
      ESP_LOGE(TAG_UART_GATEWAY, "RX task stop timeout; preserving task handle");
      all_stopped = false;
    }
  }

  if (s_tx_task_handle != NULL) {
    if (s_tx_exit_sem != NULL && xSemaphoreTake(s_tx_exit_sem, timeout_ticks) == pdTRUE) {
      s_tx_task_handle = NULL;
    } else {
      ESP_LOGE(TAG_UART_GATEWAY, "TX task stop timeout; preserving task handle");
      all_stopped = false;
    }
  }

  if (!all_stopped) {
    return MRS_ERR_CORE_TIMEOUT;
  }
  return MRS_OK;
}

system_err_t UartToGateWay_Deinit(void) {
  if (s_uart_state == UART_STATE_UNINITIALIZED) {
    return MRS_OK;
  }
  uart_state_t prev_state = s_uart_state;
  s_uart_state = UART_STATE_STOPPING;

  system_err_t stop_err = uart_gateway_stop_tasks(pdMS_TO_TICKS(1500));
  if (stop_err != MRS_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "UartToGateWay_Deinit: stop tasks failed: %s; preserving resources for retry",
             system_err_to_name(stop_err));
    s_uart_state = prev_state;
    return stop_err;
  }

  s_uart_event_queue = NULL;
  if (UartResource_IsLeaseValid(&s_gateway_lease)) {
    (void)UartResource_Release(&s_gateway_lease);
  }

  if (s_uart_event_group != NULL) {
    EventGroupHandle_t eg = s_uart_event_group;
    s_uart_event_group = NULL;
    vEventGroupDelete(eg);
  }
  if (s_rx_exit_sem != NULL) {
    SemaphoreHandle_t sem = s_rx_exit_sem;
    s_rx_exit_sem = NULL;
    vSemaphoreDelete(sem);
  }
  if (s_tx_exit_sem != NULL) {
    SemaphoreHandle_t sem = s_tx_exit_sem;
    s_tx_exit_sem = NULL;
    vSemaphoreDelete(sem);
  }

  s_data = NULL;
  s_role_request_handler = NULL;
  s_role_request_context = NULL;
  s_ota_handler = NULL;
  s_ota_context = NULL;
  s_uart_state = UART_STATE_UNINITIALIZED;
  s_uart_started = false;
  memset(&s_gateway_lease, 0, sizeof(s_gateway_lease));
  return MRS_OK;
}

system_err_t UartToGateWay_Init(DataManager_t *data) {
  if (s_uart_state == UART_STATE_RUNNING) {
    return MRS_OK;
  }
  if (data == NULL) {
    ESP_LOGE(TAG_UART_GATEWAY, "Init failed: DataManager is NULL");
    return MRS_ERR_CORE_INVALID_PARAM;
  }
  if (s_uart_state == UART_STATE_STOPPING) {
    return MRS_ERR_CORE_INVALID_STATE;
  }

  s_uart_state = UART_STATE_INITIALIZING;
  system_err_t failure_reason = MRS_OK;

#if CONFIG_MSMS_ENABLE_TESTS
  if (s_uart_init_fault_stage == 1) {
    s_uart_init_fault_stage = 0;
    failure_reason = MRS_ERR_CORE_OUT_OF_MEMORY;
    goto fail_cleanup_semaphores;
  }
#endif

  if (s_uart_event_group == NULL) {
    s_uart_event_group = xEventGroupCreate();
  }
  if (s_rx_exit_sem == NULL) {
    s_rx_exit_sem = xSemaphoreCreateBinary();
  }
  if (s_tx_exit_sem == NULL) {
    s_tx_exit_sem = xSemaphoreCreateBinary();
  }
  if (s_uart_event_group == NULL || s_rx_exit_sem == NULL || s_tx_exit_sem == NULL) {
    ESP_LOGE(TAG_UART_GATEWAY, "Failed to create FreeRTOS sync primitives");
    failure_reason = MRS_ERR_CORE_OUT_OF_MEMORY;
    goto fail_cleanup_semaphores;
  }

  s_data = data;
  xEventGroupClearBits(s_uart_event_group, 0x00FFFFFF);

  s_last_connected_tick = xTaskGetTickCount();
  s_mesh_gateway_no_node_tick = xTaskGetTickCount();
  s_uart_gateway_last_stats_tick = xTaskGetTickCount();
  s_uart_gateway_sent_frames = 0;
  s_uart_gateway_sent_bytes = 0;
  s_connected_watchdog_enabled = false;
  s_uart_num = PIN_UART_NUM;
  if (s_uart_num >= UART_NUM_MAX) {
    s_uart_num = UART_NUM_1;
    if (s_uart_num >= UART_NUM_MAX) {
      s_uart_num = UART_NUM_0;
    }
  }

  system_err_t res_init_err = UartResource_Init(s_uart_num, PIN_UART_TX, PIN_UART_RX);
  if (res_init_err != MRS_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "UartResource_Init failed: %s", system_err_to_name(res_init_err));
    failure_reason = res_init_err;
    goto fail_cleanup_semaphores;
  }

  system_err_t reg_err = UartResource_RegisterGatewayCallbacks(gateway_quiesce_cb, gateway_resume_cb, NULL);
  if (reg_err != MRS_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "UartResource_RegisterGatewayCallbacks failed: %s", system_err_to_name(reg_err));
    failure_reason = reg_err;
    goto fail_cleanup_semaphores;
  }

#if CONFIG_MSMS_ENABLE_TESTS
  if (s_uart_init_fault_stage == 2) {
    s_uart_init_fault_stage = 0;
    failure_reason = MRS_ERR_DRIVERS_INIT_FAILED;
    goto fail_cleanup_semaphores;
  }
#endif

  uart_resource_config_t gw_cfg = {
      .uart_config = {
          .baud_rate = CONFIG_UART_GATEWAY_BAUD_RATE,
          .data_bits = UART_DATA_8_BITS,
          .parity = UART_PARITY_DISABLE,
          .stop_bits = UART_STOP_BITS_1,
          .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
          .source_clk = UART_SCLK_DEFAULT,
      },
      .rx_buffer_size = UART_RX_BUF_SIZE,
      .tx_buffer_size = UART_TX_BUF_SIZE,
      .queue_size = UART_EVENT_QUEUE_LEN,
      .out_queue = &s_uart_event_queue,
      .intr_alloc_flags = 0,
      .enable_intr = true,
      .intr_config = {
          .intr_enable_mask = UART_RXFIFO_FULL_INT_ENA_M | UART_RXFIFO_TOUT_INT_ENA_M,
          .rxfifo_full_thresh = UART_RXFIFO_FULL_THRESH,
          .rx_timeout_thresh = UART_RX_TIMEOUT_THRESH,
          .txfifo_empty_intr_thresh = 0,
      },
  };

  system_err_t acq_err = UartResource_Acquire(UART_OWNER_GATEWAY, &gw_cfg, pdMS_TO_TICKS(1000), &s_gateway_lease);
  if (acq_err != MRS_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "UartResource_Acquire for GATEWAY failed: %s", system_err_to_name(acq_err));
    failure_reason = acq_err;
    goto fail_rollback_driver;
  }

  // Defensive guard (Requirement 6)
  if (!uart_is_driver_installed(s_uart_num)) {
    ESP_LOGE(TAG_UART_GATEWAY, "UART driver not installed after acquire!");
    (void)UartResource_Release(&s_gateway_lease);
    failure_reason = MRS_ERR_DRIVERS_INIT_FAILED;
    goto fail_rollback_driver;
  }

#if CONFIG_MSMS_ENABLE_TESTS
  if (s_uart_init_fault_stage == 3) {
    s_uart_init_fault_stage = 0;
    failure_reason = MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
    goto fail_rollback_driver;
  }
#endif

  if (xTaskCreate(uart_gateway_rx_task, "uart_gateway_rx", 4096, NULL, 6,
                  &s_rx_task_handle) != pdPASS) {
    ESP_LOGE(TAG_UART_GATEWAY, "Create uart_gateway_rx failed");
    failure_reason = MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
    goto fail_rollback_driver;
  }

#if CONFIG_MSMS_ENABLE_TESTS
  if (s_uart_init_fault_stage == 4) {
    s_uart_init_fault_stage = 0;
    failure_reason = MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
    goto fail_rollback_rx_task;
  }
#endif

  if (xTaskCreate(uart_gateway_tx_task, "uart_gateway_tx", 4096, NULL, 7,
                  &s_tx_task_handle) != pdPASS) {
    ESP_LOGE(TAG_UART_GATEWAY, "Create uart_gateway_tx failed");
    failure_reason = MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
    goto fail_rollback_rx_task;
  }

  s_uart_started = true;
  s_uart_state = UART_STATE_RUNNING;
  ESP_LOGI(
      TAG_UART_GATEWAY,
      "UART gateway RX+TX started on UART%d TX=%d RX=%d baud=%d",
      (int)s_uart_num, (int)PIN_UART_TX, (int)PIN_UART_RX,
      CONFIG_UART_GATEWAY_BAUD_RATE);
  return MRS_OK;

fail_rollback_rx_task: {
  system_err_t stop_res = uart_gateway_stop_tasks(pdMS_TO_TICKS(1000));
  if (stop_res != MRS_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "RX task stop timeout during init rollback! Preserving resources.");
    s_uart_state = UART_STATE_STOPPING;
    return MRS_ERR_CORE_TIMEOUT;
  }
}

fail_rollback_driver:
  s_uart_event_queue = NULL;
  if (UartResource_IsLeaseValid(&s_gateway_lease)) {
    (void)UartResource_Release(&s_gateway_lease);
  }

fail_cleanup_semaphores:
  if (s_uart_event_group != NULL) {
    vEventGroupDelete(s_uart_event_group);
    s_uart_event_group = NULL;
  }
  if (s_rx_exit_sem != NULL) {
    vSemaphoreDelete(s_rx_exit_sem);
    s_rx_exit_sem = NULL;
  }
  if (s_tx_exit_sem != NULL) {
    vSemaphoreDelete(s_tx_exit_sem);
    s_tx_exit_sem = NULL;
  }
  s_data = NULL;
  s_uart_started = false;
  s_uart_state = UART_STATE_UNINITIALIZED;
  memset(&s_gateway_lease, 0, sizeof(s_gateway_lease));
  return failure_reason;
}

void UartToGateWay_SetRoleRequestHandler(
    uart_gateway_role_request_fn handler, void *context) {
  s_role_request_handler = handler;
  s_role_request_context = context;
}

void UartToGateWay_SetOtaHandler(uart_gateway_ota_handler_fn handler,
                                void *context) {
  s_ota_handler = handler;
  s_ota_context = context;
}

int UartToGateWay_Send(const void *data, size_t len) {
  if (data == NULL || len == 0 || s_uart_state != UART_STATE_RUNNING ||
      !s_uart_started) {
    return 0;
  }
  return uart_write_bytes(s_uart_num, data, len);
}
