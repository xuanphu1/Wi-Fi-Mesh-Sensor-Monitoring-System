#include "UartToGateWay.h"

#include "DataManager.h"
#include "FunctionManager.h"
#include "InternetManager.h"
#include "PinManager.h"
#include "ProcessingDataMesh.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
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
#define UART_CMD_LINE_MAX 64
#define UART_RXFIFO_FULL_THRESH 64
#define UART_RX_TIMEOUT_THRESH 10
#define UART_GATEWAY_TX_RECV_WAIT_MS 100
#define UART_GATEWAY_TX_DRAIN_BATCH 64
#define UART_GATEWAY_TX_STATS_MS 1000

static QueueHandle_t s_uart_event_queue = NULL;
static DataManager_t *s_data = NULL;
static bool s_uart_started = false;
static uart_port_t s_uart_num = UART_NUM_1;

/* Gateway heartbeat: when in ROOT mode, gateway must periodically send
 * "Connected". */
static TickType_t s_last_connected_tick = 0;
static bool s_connected_watchdog_enabled = false;
/** Root: lần cuối gửi heartbeat "No node" khi queue mesh rỗng. */
static TickType_t s_mesh_gateway_no_node_tick = 0;
static TickType_t s_uart_gateway_last_stats_tick = 0;
static uint32_t s_uart_gateway_sent_frames = 0;
static uint32_t s_uart_gateway_sent_bytes = 0;
static bool s_pending_start_root = false;

static bool uart_gateway_mesh_mode_active(void) {
  return InternetManager_GetMode() == INTERNET_MODE_MESH;
}

static bool uart_gateway_mesh_stack_ready(void) {
  InternetManagerStatus_t status = {0};
  return InternetManager_GetStatus(s_data, &status) == MRS_OK &&
         status.mesh_started;
}

static bool uart_gateway_start_root_if_ready(void) {
  if (!uart_gateway_mesh_stack_ready()) {
    return false;
  }

  int nw = UartToGateWay_Send("Switching to root mode...\r\n",
                              sizeof("Switching to root mode...\r\n"));
  ESP_LOGI(TAG_UART_GATEWAY,
           "UART cmd: Start Root -> switch mesh root (TX %d/%d bytes)", nw,
           (int)sizeof("Switching to root mode...\r\n"));
  wifi_mesh_join_as_root_callback(s_data);
  s_connected_watchdog_enabled = true;
  s_last_connected_tick = xTaskGetTickCount();
  s_pending_start_root = false;
  return true;
}

/** Parse nhanh origin info trong JSON telemetry để log theo node gốc. */
static void gateway_extract_origin_info(const uint8_t *payload,
                                        size_t payload_len, char *origin_ip,
                                        size_t origin_ip_cap, int *origin_lvl) {
  if (origin_ip != NULL && origin_ip_cap > 0) {
    snprintf(origin_ip, origin_ip_cap, "unknown");
  }
  if (origin_lvl != NULL) {
    *origin_lvl = -1;
  }
  if (payload == NULL || payload_len == 0) {
    return;
  }

  char json[MESH_ROOT_UDP_FRAME_SIZE + 1];
  size_t copy_len = payload_len;
  if (copy_len > MESH_ROOT_UDP_FRAME_SIZE) {
    copy_len = MESH_ROOT_UDP_FRAME_SIZE;
  }
  memcpy(json, payload, copy_len);
  json[copy_len] = '\0';

  const char *ip_key = "\"i\":\"";
  char *ip_pos = strstr(json, ip_key);
  if (ip_pos != NULL && origin_ip != NULL && origin_ip_cap > 0) {
    ip_pos += strlen(ip_key);
    size_t i = 0;
    while (ip_pos[i] != '\0' && ip_pos[i] != '"' && i < (origin_ip_cap - 1)) {
      origin_ip[i] = ip_pos[i];
      i++;
    }
    origin_ip[i] = '\0';
  }

  const char *lvl_key = "\"n\":";
  char *lvl_pos = strstr(json, lvl_key);
  if (lvl_pos != NULL && origin_lvl != NULL) {
    lvl_pos += strlen(lvl_key);
    *origin_lvl = (int)strtol(lvl_pos, NULL, 10);
  }
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
  if (line == NULL || s_data == NULL) {
    return;
  }

  while (*line != '\0' && isspace((unsigned char)*line)) {
    line++;
  }

  size_t len = strlen(line);
  while (len > 0 && isspace((unsigned char)line[len - 1])) {
    line[len - 1] = '\0';
    len--;
  }

  if (equals_ignore_case(line, "Connected")) {
    s_last_connected_tick = xTaskGetTickCount();
    return;
  }

  if (equals_ignore_case(line, "Start Root")) {
    if (!uart_gateway_start_root_if_ready()) {
      s_pending_start_root = true;
      int nw = UartToGateWay_Send("Mesh not ready\r\n", 16);
      ESP_LOGW(TAG_UART_GATEWAY,
               "Start Root pending: mesh-lite init not finished yet (TX %d/16)",
               nw);
    }
    return;
  }

  ESP_LOGW(TAG_UART_GATEWAY, "Unknown UART cmd: '%s'", line);
}

/** Nhận: queue sự kiện UART, đọc byte, ghép dòng lệnh gateway → ESP. */
static void uart_gateway_rx_task(void *pvParameter) {
  (void)pvParameter;

  uart_event_t event;
  uint8_t rx_data[UART_RX_BUF_SIZE];
  char line_buf[UART_CMD_LINE_MAX];
  size_t line_len = 0;

  while (1) {
    if (!uart_gateway_mesh_mode_active()) {
      line_len = 0;
      s_connected_watchdog_enabled = false;
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    if (s_pending_start_root && uart_gateway_start_root_if_ready()) {
      continue;
    }

    if (xQueueReceive(s_uart_event_queue, &event, pdMS_TO_TICKS(500)) !=
        pdTRUE) {
      TickType_t now = xTaskGetTickCount();
      if (s_pending_start_root && uart_gateway_start_root_if_ready()) {
        continue;
      }
      if (s_connected_watchdog_enabled) {
        if ((now - s_last_connected_tick) > pdMS_TO_TICKS(1000)) {
          ESP_LOGW(TAG_UART_GATEWAY, "No 'Connected' heartbeat from gateway "
                                     "for >1s -> switch to node");
          wifi_mesh_join_as_node_callback(s_data);
          s_connected_watchdog_enabled = false;
          s_last_connected_tick = now;
        }
      }
      continue;
    }

    switch (event.type) {
    case UART_DATA: {
      int read_len = uart_read_bytes(
          s_uart_num, rx_data,
          (event.size < UART_RX_BUF_SIZE) ? (int)event.size : UART_RX_BUF_SIZE,
          0);
      if (read_len > 0) {
        ESP_LOGI(TAG_UART_GATEWAY, "RAW RX (%d bytes): %.*s", read_len,
                 read_len, (char *)rx_data);
      }
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
      xQueueReset(s_uart_event_queue);
      break;
    default:
      break;
    }
  }
}

/** Gửi: mesh root lấy `gateway_rx_queue` → UART; heartbeat `No node` khi queue
 * rỗng. */
static bool uart_gateway_send_mesh_msg(const mesh_gateway_rx_msg_t *msg) {
  if (msg == NULL || msg->len == 0 || msg->len > sizeof(msg->data)) {
    return false;
  }

  char tx_buffer[1450];
  int sent = 0;
  ESP_LOGI(TAG_UART_GATEWAY, "Gateway UART TX payload (%u bytes): %.*s",
           (unsigned)msg->len, (int)msg->len, msg->data);

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
    ESP_LOGI(TAG_UART_GATEWAY, "Gateway UART TX sent %d bytes", sent);
  } else {
    ESP_LOGW(TAG_UART_GATEWAY, "Gateway UART TX failed: %d", sent);
  }
  return true;
}

static void uart_gateway_log_tx_stats(QueueHandle_t queue) {
  TickType_t now = xTaskGetTickCount();
  if ((now - s_uart_gateway_last_stats_tick) <
      pdMS_TO_TICKS(UART_GATEWAY_TX_STATS_MS)) {
    return;
  }

  UBaseType_t queue_used = 0;
  UBaseType_t queue_total = 0;
  if (queue != NULL) {
    queue_used = uxQueueMessagesWaiting(queue);
    queue_total = queue_used + uxQueueSpacesAvailable(queue);
  }

  ESP_LOGI(TAG_UART_GATEWAY,
           "UART gateway TX: frames=%" PRIu32 ", bytes=%" PRIu32
           ", queue=%u/%u",
           s_uart_gateway_sent_frames, s_uart_gateway_sent_bytes,
           (unsigned)queue_used, (unsigned)queue_total);
  s_uart_gateway_sent_frames = 0;
  s_uart_gateway_sent_bytes = 0;
  s_uart_gateway_last_stats_tick = now;
}

static void uart_gateway_tx_task(void *pvParameter) {
  (void)pvParameter;

  for (;;) {
    if (!uart_gateway_mesh_mode_active()) {
      vTaskDelay(pdMS_TO_TICKS(UART_GATEWAY_TX_RECV_WAIT_MS));
      continue;
    }

    if (s_data == NULL || s_data->meshIo.role != MESH_ROLE_ROOT ||
        s_data->meshIo.gateway_rx_queue == NULL) {
      vTaskDelay(pdMS_TO_TICKS(UART_GATEWAY_TX_RECV_WAIT_MS));
      continue;
    }
    mesh_gateway_rx_msg_t msg;
    QueueHandle_t queue = s_data->meshIo.gateway_rx_queue;
    if (xQueueReceive(queue, &msg,
                      pdMS_TO_TICKS(UART_GATEWAY_TX_RECV_WAIT_MS)) != pdTRUE) {
      TickType_t now = xTaskGetTickCount();
      if ((now - s_mesh_gateway_no_node_tick) >=
          pdMS_TO_TICKS(MESH_ROOT_UART_NO_NODE_MS)) {
        (void)UartToGateWay_Send("No node\n", 8);
        s_mesh_gateway_no_node_tick = now;
      }
      uart_gateway_log_tx_stats(queue);
      continue;
    }

    (void)uart_gateway_send_mesh_msg(&msg);
    s_mesh_gateway_no_node_tick = xTaskGetTickCount();

    for (int drained = 1; drained < UART_GATEWAY_TX_DRAIN_BATCH; drained++) {
      if (!uart_gateway_mesh_mode_active() || s_data == NULL ||
          s_data->meshIo.role != MESH_ROLE_ROOT ||
          s_data->meshIo.gateway_rx_queue == NULL) {
        break;
      }
      queue = s_data->meshIo.gateway_rx_queue;
      if (xQueueReceive(queue, &msg, 0) != pdTRUE) {
        break;
      }
      (void)uart_gateway_send_mesh_msg(&msg);
      s_mesh_gateway_no_node_tick = xTaskGetTickCount();
    }
    uart_gateway_log_tx_stats(queue);
  }
}

system_err_t UartToGateWay_Init(DataManager_t *data) {
  if (s_uart_started) {
    return MRS_OK;
  }
  if (data == NULL) {
    ESP_LOGE(TAG_UART_GATEWAY, "Init failed: DataManager is NULL");
    return MRS_ERR_CORE_INVALID_PARAM;
  }

  s_data = data;
  s_last_connected_tick = xTaskGetTickCount();
  s_mesh_gateway_no_node_tick = xTaskGetTickCount();
  s_uart_gateway_last_stats_tick = xTaskGetTickCount();
  s_uart_gateway_sent_frames = 0;
  s_uart_gateway_sent_bytes = 0;
  s_connected_watchdog_enabled = false;
  s_uart_num = PIN_UART_NUM;
  if (s_uart_num >= UART_NUM_MAX) {
    ESP_LOGW(TAG_UART_GATEWAY,
             "Configured UART%d is invalid on this target; fallback UART1",
             (int)s_uart_num);
    s_uart_num = UART_NUM_1;
    if (s_uart_num >= UART_NUM_MAX) {
      s_uart_num = UART_NUM_0;
    }
  }

  const uart_config_t uart_config = {
      .baud_rate = CONFIG_UART_GATEWAY_BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };

  if (uart_driver_install(s_uart_num, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE,
                          UART_EVENT_QUEUE_LEN, &s_uart_event_queue,
                          0) != ESP_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "uart_driver_install failed");
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }
  if (uart_param_config(s_uart_num, &uart_config) != ESP_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "uart_param_config failed");
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }
  if (uart_set_pin(s_uart_num, PIN_UART_TX, PIN_UART_RX, UART_PIN_NO_CHANGE,
                   UART_PIN_NO_CHANGE) != ESP_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "uart_set_pin failed");
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }

  const uart_intr_config_t uart_intr = {
      .intr_enable_mask =
          UART_RXFIFO_FULL_INT_ENA_M | UART_RXFIFO_TOUT_INT_ENA_M,
      .rxfifo_full_thresh = UART_RXFIFO_FULL_THRESH,
      .rx_timeout_thresh = UART_RX_TIMEOUT_THRESH,
      .txfifo_empty_intr_thresh = 0,
  };
  if (uart_intr_config(s_uart_num, &uart_intr) != ESP_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "uart_intr_config failed");
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }
  if (uart_enable_rx_intr(s_uart_num) != ESP_OK) {
    ESP_LOGE(TAG_UART_GATEWAY, "uart_enable_rx_intr failed");
    return MRS_ERR_DRIVERS_INIT_FAILED;
  }

  TaskHandle_t rx_handle = NULL;
  if (xTaskCreate(uart_gateway_rx_task, "uart_gateway_rx", 4096, NULL, 6,
                  &rx_handle) != pdPASS) {
    ESP_LOGE(TAG_UART_GATEWAY, "Create uart_gateway_rx failed");
    uart_driver_delete(s_uart_num);
    s_uart_event_queue = NULL;
    return MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
  }
  if (xTaskCreate(uart_gateway_tx_task, "uart_gateway_tx", 4096, NULL, 7,
                  NULL) != pdPASS) {
    ESP_LOGE(TAG_UART_GATEWAY, "Create uart_gateway_tx failed");
    if (rx_handle != NULL) {
      vTaskDelete(rx_handle);
    }
    uart_driver_delete(s_uart_num);
    s_uart_event_queue = NULL;
    return MRS_ERR_FUNCTIONMANAGER_TASK_FAILED;
  }

  s_uart_started = true;
  ESP_LOGI(
      TAG_UART_GATEWAY,
      "UART gateway RX+TX on UART%d TX=%d RX=%d baud=%d (rxfifo=%d timeout=%d)",
      (int)s_uart_num, (int)PIN_UART_TX, (int)PIN_UART_RX,
      CONFIG_UART_GATEWAY_BAUD_RATE, UART_RXFIFO_FULL_THRESH,
      UART_RX_TIMEOUT_THRESH);
  return MRS_OK;
}

int UartToGateWay_Send(const void *data, size_t len) {
  if (data == NULL || len == 0) {
    return 0;
  }
  return uart_write_bytes(s_uart_num, data, len);
}
