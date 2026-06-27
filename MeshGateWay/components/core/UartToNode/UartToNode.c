/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "UartToNode.h"

#include "Datamanager.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"


#include "esp_check.h"
#include "esp_log.h"


#include "driver/gpio.h"
#include "driver/uart.h"


static const char *TAG = "uart_to_node";

static const char k_handshake[] = UART_TO_NODE_HS_WIRE;

static QueueHandle_t s_uart_queue;
static QueueHandle_t s_uplink_queue = NULL;
static dm_ws_t *s_ws_state = NULL;
static dm_telemetry_t *s_telemetry = NULL;

static uart_port_t cfg_uart_num(void) {
#if defined(CONFIG_UART_TO_NODE_UART_NUM)
  return (uart_port_t)CONFIG_UART_TO_NODE_UART_NUM;
#else
  return UART_NUM_1;
#endif
}

static int cfg_baudrate(void) {
#if defined(CONFIG_UART_TO_NODE_BAUDRATE)
  return CONFIG_UART_TO_NODE_BAUDRATE;
#else
  return 115200;
#endif
}

static int cfg_tx_pin(void) {
#if defined(CONFIG_UART_TO_NODE_TX_PIN)
  return CONFIG_UART_TO_NODE_TX_PIN;
#else
  return GPIO_NUM_17;
#endif
}

static int cfg_rx_pin(void) {
#if defined(CONFIG_UART_TO_NODE_RX_PIN)
  return CONFIG_UART_TO_NODE_RX_PIN;
#else
  return GPIO_NUM_16;
#endif
}

static int cfg_rts_pin(void) {
#if defined(CONFIG_UART_TO_NODE_RTS_PIN)
  return CONFIG_UART_TO_NODE_RTS_PIN;
#else
  return UART_PIN_NO_CHANGE;
#endif
}

static int cfg_cts_pin(void) {
#if defined(CONFIG_UART_TO_NODE_CTS_PIN)
  return CONFIG_UART_TO_NODE_CTS_PIN;
#else
  return UART_PIN_NO_CHANGE;
#endif
}

static TickType_t cfg_send_period_ticks(void) {
  uint32_t ms = 500;
#if defined(CONFIG_UART_TO_NODE_SEND_PERIOD_MS)
  ms = (uint32_t)CONFIG_UART_TO_NODE_SEND_PERIOD_MS;
#endif
  return pdMS_TO_TICKS(ms);
}

void uart_to_node_attach_uplink_queue(QueueHandle_t uplink_queue) {
  s_uplink_queue = uplink_queue;
}

void uart_to_node_attach_ws_state(dm_ws_t *ws_state) { s_ws_state = ws_state; }

void uart_to_node_attach_telemetry(dm_telemetry_t *telemetry) {
  s_telemetry = telemetry;
}

size_t uart_to_node_get_buffered_len(void) {
    size_t buffered = 0;
    uart_get_buffered_data_len(cfg_uart_num(), &buffered);
    return buffered;
}

void uart_to_node_get_queue_status(uint32_t *used, uint32_t *total) {
  if (used)
    *used = 0;
  if (total)
    *total = 0;
  if (s_uplink_queue) {
    if (used)
      *used = (uint32_t)uxQueueMessagesWaiting(s_uplink_queue);
    if (total)
      *total = (uint32_t)(uxQueueMessagesWaiting(s_uplink_queue) +
                          uxQueueSpacesAvailable(s_uplink_queue));
  }
}

/** Độ dài phần chữ trong `UART_NODE_MSG_TYPE_NO_NODE` (bỏ \\n ở cuối macro nếu
 * có). */
static size_t uart_no_node_type_body_len(void) {
  size_t n = sizeof(UART_NODE_MSG_TYPE_NO_NODE) - 1U;
  if (n > 0 && UART_NODE_MSG_TYPE_NO_NODE[n - 1U] == '\n') {
    n--;
  }
  return n;
}

/** So khớp giá trị “No Node” (JSON `type` hoặc dòng UART sau trim, không chứa
 * \\n). */
static bool uart_str_match_no_node_type(const char *s) {
  if (!s) {
    return false;
  }
  size_t bl = uart_no_node_type_body_len();
  return strlen(s) == bl && strncasecmp(s, UART_NODE_MSG_TYPE_NO_NODE, bl) == 0;
}

/** Dòng ASCII thuần “No node” / “No Node” từ mesh (không JSON) — chỉ life,
 * không uplink. */
static bool uart_rx_plaintext_no_node(const uint8_t *data, size_t len) {
  char line[UART_NODE_RX_DATA_MAX + 1];

  if (len == 0 || len >= sizeof(line)) {
    return false;
  }
  memcpy(line, data, len);
  line[len] = '\0';
  return uart_str_match_no_node_type(line);
}

/** Bản tin chỉ để “check life” — không đẩy uplink (vẫn xử lý RX/handshake bình
 * thường). */
static bool uart_rx_is_no_node_only_life(const uint8_t *data, size_t len) {
  if (!data || len == 0 || len > UART_NODE_RX_DATA_MAX) {
    return false;
  }
  /* Trim khoảng trắng / xuống dòng ở đầu cuối (UART JSON line). */
  while (len > 0 && isspace((unsigned char)data[0])) {
    data++;
    len--;
  }
  while (len > 0 && isspace((unsigned char)data[len - 1])) {
    len--;
  }
  if (len == 0) {
    return false;
  }
  /* Dòng chữ “No node” (log hex: 4e 6f 20 6e 6f 64 65 0a) — không phải JSON. */
  if (data[0] != '{') {
    return uart_rx_plaintext_no_node(data, len);
  }

  /*
   * JSON telemetry kiểu {"v":1,...} không có khóa "type" — tránh
   * malloc+cJSON_Parse mỗi chunk (dễ cạn heap và làm HTTP captive/SPIFFS fail).
   */
  char tmp[UART_NODE_RX_DATA_MAX + 1];
  memcpy(tmp, data, len);
  tmp[len] = '\0';
  if (strstr(tmp, "\"type\"") == NULL) {
    return false;
  }

  char *buf = (char *)malloc(len + 1);
  if (!buf) {
    return false;
  }
  memcpy(buf, data, len);
  buf[len] = '\0';

  cJSON *root = cJSON_Parse(buf);
  free(buf);
  if (!root) {
    return false;
  }

  const cJSON *t = cJSON_GetObjectItem(root, "type");
  bool skip = false;
  if (cJSON_IsString(t) && t->valuestring) {
    if (uart_str_match_no_node_type(t->valuestring)) {
      skip = true;
    }
  }
  cJSON_Delete(root);
  return skip;
}

/** Đẩy chunk RX vào queue chung — WebSocket gửi lên server. */
static void uart_node_enqueue_rx(const uint8_t *data, size_t len) {
  if (!s_uplink_queue || !data || len == 0) {
    return;
  }
  if (s_ws_state == NULL || !s_ws_state->connected) {
    return;
  }

  while (len > 0) {
    uart_node_rx_item_t item;
    item.len = len > UART_NODE_RX_DATA_MAX ? UART_NODE_RX_DATA_MAX : len;
    memcpy(item.data, data, item.len);

    if (xQueueSend(s_uplink_queue, &item, 0) != pdTRUE) {
      ESP_LOGW(TAG, "uart uplink queue full, drop %u bytes",
               (unsigned)item.len);
    }

    data += item.len;
    len -= item.len;
  }
}

static TickType_t cfg_idle_reset_ticks(void) {
  uint32_t ms = 2000;
#if defined(CONFIG_UART_TO_NODE_RX_IDLE_RESET_MS)
  ms = (uint32_t)CONFIG_UART_TO_NODE_RX_IDLE_RESET_MS;
#endif
  return pdMS_TO_TICKS(ms);
}

static void uart_to_node_send_line(const char *s) {
  if (!s) {
    return;
  }
  uart_write_bytes(cfg_uart_num(), s, (size_t)strlen(s));
  uart_write_bytes(cfg_uart_num(), "\r\n", 2);
}

static void uart_to_node_reset_handshake(uart_to_node_ctx_t *ctx) {
  ctx->state = UART_TO_NODE_STATE_START_ROOT;
  ctx->hs_fill = 0;
}

static void uart_to_node_handshake_ok(uart_to_node_ctx_t *ctx) {
  ctx->state = UART_TO_NODE_STATE_CONNECTED;
  ctx->hs_fill = 0;
  ESP_LOGI(TAG,
           "Handshake OK (exact wire): Switching to root mode... -> connected");
}

/** Đẩy từng byte RX; chỉ khi đang chờ handshake mới quét khớp `k_handshake`. */
static void uart_to_node_hs_push_byte(uart_to_node_ctx_t *ctx, uint8_t b) {
  if (ctx->state != UART_TO_NODE_STATE_START_ROOT) {
    return;
  }

  if (ctx->hs_fill < UART_TO_NODE_HS_LEN) {
    ctx->hs_roll[ctx->hs_fill++] = b;
  } else {
    memmove(ctx->hs_roll, ctx->hs_roll + 1, UART_TO_NODE_HS_LEN - 1);
    ctx->hs_roll[UART_TO_NODE_HS_LEN - 1] = b;
  }

  if (ctx->hs_fill < UART_TO_NODE_HS_LEN) {
    return;
  }
  if (memcmp(ctx->hs_roll, k_handshake, UART_TO_NODE_HS_LEN) == 0) {
    uart_to_node_handshake_ok(ctx);
  }
}

/** Log UART RX theo dòng (kết thúc '\n') để tránh bị cắt theo chunk UART. */
static void uart_to_node_log_rx_chunk(const uint8_t *data, size_t len) {
  if (len == 0 || !data) {
    return;
  }
  enum { RX_LINE_LOG_BUF_SZ = 512 };
  static char s_rx_line[RX_LINE_LOG_BUF_SZ];
  static size_t s_rx_line_len = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = data[i];

    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (s_rx_line_len > 0) {
        s_rx_line[s_rx_line_len] = '\0';
        // Sử dụng LOGD thay vì LOGI để không in ra màn hình console ở chế độ mặc định
        // Việc in ra console (ESP_LOGI) quá nhiều ở tốc độ baud cao sẽ làm nghẽn task và gây tràn FIFO
        ESP_LOGD(TAG, "RX line: %s", s_rx_line);
        s_rx_line_len = 0;
      }
      continue;
    }

    if (!isprint(c) && c != '\t') {
      c = '.';
    }

    if (s_rx_line_len >= (RX_LINE_LOG_BUF_SZ - 1U)) {
      s_rx_line[s_rx_line_len] = '\0';
      ESP_LOGW(TAG, "RX line (truncated): %s", s_rx_line);
      s_rx_line_len = 0;
    }

    s_rx_line[s_rx_line_len++] = (char)c;
  }
}

static void uart_to_node_feed_rx(uart_to_node_ctx_t *ctx, const uint8_t *data,
                                 size_t len) {
  uart_to_node_log_rx_chunk(data, len);

  if (!uart_rx_is_no_node_only_life(data, len)) {
    if (s_telemetry) {
      s_telemetry->rx_packet_count++;
      s_telemetry->rx_byte_count += len;
    }
    uart_node_enqueue_rx(data, len);
  }

  for (size_t i = 0; i < len; i++) {
    uart_to_node_hs_push_byte(ctx, data[i]);
  }
}

static void uart_to_node_drain_rx_hw_fifo(uart_to_node_ctx_t *ctx,
                                          uart_port_t u) {
  uint8_t tmp[256];
  size_t buffered = 0;

  for (;;) {
    if (uart_get_buffered_data_len(u, &buffered) != ESP_OK || buffered == 0) {
      break;
    }
    size_t chunk = buffered > sizeof(tmp) ? sizeof(tmp) : buffered;
    int rd = uart_read_bytes(u, tmp, chunk, 0);
    if (rd <= 0) {
      break;
    }
    uart_to_node_feed_rx(ctx, tmp, (size_t)rd);
  }
}

static esp_err_t uart_to_node_uart_init(void) {
  uart_config_t uart_config = {
      .baud_rate = cfg_baudrate(),
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };

  uart_port_t u = cfg_uart_num();

  /* RX lớn + queue sự kiện: driver tạo queue, ISR đẩy uart_event_t (UART_DATA,
   * overflow, ...) */
  ESP_RETURN_ON_ERROR(uart_driver_install(u, 8192, 2048, 64, &s_uart_queue, ESP_INTR_FLAG_IRAM),
                      TAG, "uart_driver_install");
  ESP_RETURN_ON_ERROR(uart_param_config(u, &uart_config), TAG,
                      "uart_param_config");
  ESP_RETURN_ON_ERROR(
      uart_set_pin(u, cfg_tx_pin(), cfg_rx_pin(), cfg_rts_pin(), cfg_cts_pin()),
      TAG, "uart_set_pin");

  uart_set_rx_full_threshold(u, 50);
  uart_set_rx_timeout(u, 10);

  uart_flush_input(u);
  return ESP_OK;
}

static void uart_to_node_task(void *arg) {
  (void)arg;

  uart_to_node_ctx_t ctx = {0};
  uart_to_node_reset_handshake(&ctx);
  ctx.last_rx_tick = xTaskGetTickCount();
  ctx.next_tx_tick = xTaskGetTickCount();

  const TickType_t send_period = cfg_send_period_ticks();
  const TickType_t idle_reset = cfg_idle_reset_ticks();

  uart_port_t u = cfg_uart_num();

  while (1) {
    uart_event_t event;
    const TickType_t queue_wait = pdMS_TO_TICKS(40);

    if (xQueueReceive(s_uart_queue, &event, queue_wait) == pdTRUE) {
      switch (event.type) {
      case UART_DATA: {
        ctx.last_rx_tick = xTaskGetTickCount();
        size_t remaining = (size_t)event.size;
        uint8_t tmp[256];
        if (remaining == 0) {
          break;
        }
        while (remaining > 0) {
          size_t chunk = remaining > sizeof(tmp) ? sizeof(tmp) : remaining;
          int rd = uart_read_bytes(u, tmp, chunk, pdMS_TO_TICKS(50));
          if (rd <= 0) {
            break;
          }
          uart_to_node_feed_rx(&ctx, tmp, (size_t)rd);
          remaining -= (size_t)rd;
        }
        break;
      }
      case UART_FIFO_OVF:
        ESP_LOGW(TAG, "UART RX FIFO overflow (mất dữ liệu) -> flush RX");
        uart_flush_input(u);
        break;
      case UART_BUFFER_FULL:
        ESP_LOGW(TAG, "UART driver ringbuffer full -> flush RX");
        uart_flush_input(u);
        break;
      case UART_BREAK:
      case UART_PARITY_ERR:
      case UART_FRAME_ERR:
        ESP_LOGW(TAG, "UART error event=%d", (int)event.type);
        break;
      default:
        break;
      }
    }

    /* Idle timeout: chỉ sau khi đã connected */
    if (ctx.state == UART_TO_NODE_STATE_CONNECTED) {
      TickType_t now = xTaskGetTickCount();
      if ((now - ctx.last_rx_tick) > idle_reset) {
        ESP_LOGW(TAG, "RX idle > %lu ms, reset handshake",
                 (unsigned long)(idle_reset * portTICK_PERIOD_MS));
        uart_to_node_reset_handshake(&ctx);
        ctx.last_rx_tick = now;
        ctx.next_tx_tick = now;
      }
    }

    /* TX định kỳ */
    TickType_t now = xTaskGetTickCount();
    if ((int32_t)(now - ctx.next_tx_tick) >= 0) {
      size_t buf_len = uart_to_node_get_buffered_len();
      if (ctx.state == UART_TO_NODE_STATE_START_ROOT) {
        uart_to_node_send_line("Start Root");
        ESP_LOGI(TAG, "Send: Start Root | UART Buffer: %zu bytes", buf_len);
      } else {
        uart_to_node_send_line("Connected");
        ESP_LOGI(TAG, "Send: Connected | UART Buffer: %zu bytes", buf_len);
      }
      ctx.next_tx_tick = now + send_period;
    }
  }
}

esp_err_t uart_to_node_start(void) {
#if defined(CONFIG_UART_TO_NODE_ENABLE) && !CONFIG_UART_TO_NODE_ENABLE
  return ESP_ERR_NOT_SUPPORTED;
#else
  static bool started = false;
  if (started) {
    return ESP_OK;
  }
  started = true;

  ESP_RETURN_ON_ERROR(uart_to_node_uart_init(), TAG, "uart init failed");

  uint32_t stack_words = 4096;
  UBaseType_t prio = 5;
#if defined(CONFIG_UART_TO_NODE_TASK_STACK)
  stack_words = (uint32_t)CONFIG_UART_TO_NODE_TASK_STACK;
#endif
#if defined(CONFIG_UART_TO_NODE_TASK_PRIO)
  prio = (UBaseType_t)CONFIG_UART_TO_NODE_TASK_PRIO;
#endif

  BaseType_t ok = xTaskCreate(uart_to_node_task, "uart_to_node",
                              (uint32_t)stack_words, NULL, prio, NULL);
  return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
#endif
}
