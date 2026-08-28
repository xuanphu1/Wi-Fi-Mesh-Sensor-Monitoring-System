#include "ScreenManager.h"

#include "MemoryManager.h"
#include "LinkListData.h"
#include "FOTAManager.h"
#include "PowerManager.h"
#include "UartToNode.h"
#include "WSHandle.h"
#include "WifiManager.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "lvgl_helpers.h"
#include "ui.h"
#include "xpt2046_soft.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  dm_metrics_t *metrics;
  dm_lvgl_t *lvgl;
  dm_telemetry_t *telemetry;
  dm_hw_t *hw;
} screen_ctx_t;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUF_SIZE];
// static lv_color_t buf2[DISP_BUF_SIZE];

static void set_label_text_if_changed(lv_obj_t *label, const char *text) {
  if (label == NULL || text == NULL) {
    return;
  }

  const char *current = lv_label_get_text(label);
  if (current == NULL || strcmp(current, text) != 0) {
    lv_label_set_text(label, text);
  }
}

static void set_label_fmt_if_changed(lv_obj_t *label, const char *fmt, ...) {
  if (label == NULL || fmt == NULL) {
    return;
  }

  char text[32];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);

  set_label_text_if_changed(label, text);
}

static void set_obj_hidden_if_changed(lv_obj_t *obj, bool hidden) {
  if (obj == NULL) {
    return;
  }

  bool is_hidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
  if (hidden && !is_hidden) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else if (!hidden && is_hidden) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

static void render_empty_node_info(void) {
  if (ui_SelectNode) {
    lv_dropdown_set_options(ui_SelectNode, "NONE");
  }
  set_label_text_if_changed(ui_LableValueSensor, "0");
  set_label_text_if_changed(ui_LabelPort1, "P1 :");
  set_label_text_if_changed(ui_LabelPort2, "P2 :");
  set_label_text_if_changed(ui_LabelPort3, "P3 :");
  set_label_text_if_changed(ui_ValuePort1, "NONE");
  set_label_text_if_changed(ui_ValuePort2, "NONE");
  set_label_text_if_changed(ui_VaulePort3, "NONE");
}

static void render_selected_node_info(void) {
  link_list_node_snapshot_t node = {0};
  if (!link_list_data_get_selected_node(&node)) {
    render_empty_node_info();
    return;
  }

  set_label_fmt_if_changed(ui_LableValueSensor, "%u", node.sensor_count);

  lv_obj_t *port_labels[LINK_LIST_NODE_PORT_MAX] = {
      ui_LabelPort1, ui_LabelPort2, ui_LabelPort3};
  lv_obj_t *value_labels[LINK_LIST_NODE_PORT_MAX] = {
      ui_ValuePort1, ui_ValuePort2, ui_VaulePort3};

  for (uint8_t i = 0; i < LINK_LIST_NODE_PORT_MAX; i++) {
    if (i < node.sensor_count && node.ports[i].name[0] != '\0') {
      set_label_fmt_if_changed(port_labels[i], "P%d :", node.ports[i].port);
      set_label_text_if_changed(value_labels[i], node.ports[i].name);
    } else {
      set_label_fmt_if_changed(port_labels[i], "P%u :", (unsigned)(i + 1U));
      set_label_text_if_changed(value_labels[i], "NONE");
    }
  }
}

static void update_des_dropdown_if_needed(void) {
  if (ui_SelectDesData == NULL) {
    return;
  }

  websocket_target_t target = websocket_get_selected_target();
  uint16_t expected_index = (target == WEBSOCKET_TARGET_SERVER) ? 1 : 0;

  static uint16_t last_synced_index = 0xFFFF;
  if (last_synced_index != expected_index) {
    last_synced_index = expected_index;
    if (lv_dropdown_get_selected(ui_SelectDesData) != expected_index) {
      lv_dropdown_set_selected(ui_SelectDesData, expected_index);
    }
  }
}

static void update_node_dropdown_if_needed(uint32_t *last_version) {
  if (last_version == NULL || ui_SelectNode == NULL) {
    return;
  }

  uint32_t version = link_list_data_get_version();
  if (*last_version == version) {
    return;
  }
  *last_version = version;

  char options[LINK_LIST_NODE_MAX * LINK_LIST_NODE_ID_MAX];
  if (link_list_data_build_dropdown_options(options, sizeof(options))) {
    lv_dropdown_set_options(ui_SelectNode, options);
  } else {
    lv_dropdown_set_options(ui_SelectNode, "NONE");
  }
}

static void lv_tick_task(void *arg) {
  (void)arg;
  lv_tick_inc(1);
}

static void lvgl_task(void *arg) {
  screen_ctx_t *ctx = (screen_ctx_t *)arg;
  ui_metrics_t m = {0};

  static const char *month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  TickType_t last_ui_update = 0;
  uint32_t last_node_dropdown_version = UINT32_MAX;

  while (1) {
    if (fota_is_running()) {
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    if (ctx && ctx->metrics && ctx->metrics->mutex &&
        xSemaphoreTake(ctx->metrics->mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      m = ctx->metrics->value;
      if (ctx->lvgl) {
        ctx->lvgl->loop_counter++;
      }
      xSemaphoreGive(ctx->metrics->mutex);
    }

    TickType_t now = xTaskGetTickCount();
    if ((now - last_ui_update) >= pdMS_TO_TICKS(250)) {
      last_ui_update = now;

      if (m.rtc_ok) {
        set_label_fmt_if_changed(ui_LabelHour, "%02d", m.rtc_time.tm_hour);
        set_label_fmt_if_changed(ui_LabelMinute, "%02d", m.rtc_time.tm_min);
        set_label_fmt_if_changed(ui_LabelSecond, "%02d", m.rtc_time.tm_sec);

        set_label_text_if_changed(ui_LabelWeekDay,
                                  getWeekDay((uint8_t)m.rtc_time.tm_wday));
        set_label_fmt_if_changed(ui_LabelDay, "%02d", m.rtc_time.tm_mday);
        if (m.rtc_time.tm_mon >= 0 && m.rtc_time.tm_mon <= 11) {
          set_label_text_if_changed(ui_LabelMonth,
                                    month_names[m.rtc_time.tm_mon]);
        }
        set_label_fmt_if_changed(ui_Labelyear, "%04d", m.rtc_time.tm_year + 1900);
      }

      // Uptime in Days (fractional)
      uint32_t up_s = m.uptime_s;
      float days_f = (float)up_s / 86400.0f;
      set_label_fmt_if_changed(ui_LabelUpTime, "%.2f", (double)days_f);

      // Battery
      int bat_pct = 0;
      if (ctx && ctx->hw) {
        bat_pct = power_manager_battery_get_percent(ctx->hw, NULL, NULL);
      }
      set_label_fmt_if_changed(ui_ValueBattery, "%d", bat_pct);

      // Battery Bars
      set_obj_hidden_if_changed(ui_LabeBattery1, bat_pct < 10);
      set_obj_hidden_if_changed(ui_LabeBattery2, bat_pct < 30);
      set_obj_hidden_if_changed(ui_LabeBattery3, bat_pct < 50);
      set_obj_hidden_if_changed(ui_LabeBattery4, bat_pct < 70);
      set_obj_hidden_if_changed(ui_LabeBattery5, bat_pct < 90);

      // Total Memory (SD Card)
      if (ui_ValueMemory) {
        uint32_t mb = m.sd_total_kb / 1024;
        uint32_t gb_whole = mb / 1000;
        uint32_t gb_frac = (mb % 1000) / 10;
        set_label_fmt_if_changed(ui_ValueMemory, "%lu.%02lu",
                                 (unsigned long)gb_whole,
                                 (unsigned long)gb_frac);
      }

      // Memory Used (System RAM %)
      uint8_t mem_pct = 0;
      memory_manager_get_usage(NULL, NULL, &mem_pct);
      set_label_fmt_if_changed(ui_ValueMemoryUsed, "%u", mem_pct);

      // Memory Bars
      set_obj_hidden_if_changed(ui_LabelUsedMem1, mem_pct < 10);
      set_obj_hidden_if_changed(ui_LabelUsedMem2, mem_pct < 25);
      set_obj_hidden_if_changed(ui_LabelUsedMem3, mem_pct < 45);
      set_obj_hidden_if_changed(ui_LabelUsedMem4, mem_pct < 65);
      set_obj_hidden_if_changed(ui_LabelUsedMem5, mem_pct < 85);
      set_obj_hidden_if_changed(ui_LabelUsedMem6, mem_pct < 95);

      set_label_fmt_if_changed(ui_LabelValueFPS, "%lu", (unsigned long)m.fps);

      // CPU/RAM raw
      set_label_fmt_if_changed(ui_LabelValueRestRam, "%lu.%lu",
                               (unsigned long)(m.ram_free_permille / 10U),
                               (unsigned long)(m.ram_free_permille % 10U));
      set_label_fmt_if_changed(ui_LabelValueRestCPU, "%lu.%lu",
                               (unsigned long)(m.cpu_load_permille / 10U),
                               (unsigned long)(m.cpu_load_permille % 10U));

      bool wifi_connected = is_wifi_connected();
      set_obj_hidden_if_changed(ui_ImageWifiConn, !wifi_connected);
      set_obj_hidden_if_changed(ui_ImageWifiNotCon, wifi_connected);

      set_label_text_if_changed(ui_LabelNumConn, "--");

      if (ui_LabelStatus) {
        set_label_text_if_changed(ui_LabelStatus,
                                  websocket_is_connected() ? "Connected"
                                                           : "Disconnected");
      }
      set_label_fmt_if_changed(ui_ValueReconnect, "%lu",
                               (unsigned long)websocket_get_reconnect_count());

      update_node_dropdown_if_needed(&last_node_dropdown_version);
      update_des_dropdown_if_needed();
      render_selected_node_info();
    }

    uint32_t wait_ms = lv_timer_handler();

    static TickType_t last_rate_update = 0;
    static uint32_t last_rx = 0;
    static uint32_t last_tx = 0;
    if (now - last_rate_update >= pdMS_TO_TICKS(1000)) {
      last_rate_update = now;
      if (ctx && ctx->telemetry) {
        uint32_t current_rx = ctx->telemetry->rx_packet_count;
        uint32_t current_tx = ctx->telemetry->tx_packet_count;
        uint32_t rx_rate = current_rx - last_rx;
        uint32_t tx_rate = current_tx - last_tx;
        last_rx = current_rx;
        last_tx = current_tx;

        set_label_fmt_if_changed(ui_Throuhout, "%lu",
                                 (unsigned long)current_rx);
        set_label_fmt_if_changed(ui_LabelRXRate, "%lu", (unsigned long)rx_rate);
        set_label_fmt_if_changed(ui_LabelTXRate, "%lu", (unsigned long)tx_rate);

        // IP & MAC Update using WifiManager APIs
        if (ui_ipDevice) {
          char ip_str[16];
          if (wifi_manager_get_ip_info(ip_str, sizeof(ip_str))) {
            set_label_text_if_changed(ui_ipDevice, ip_str);
          }
        }

        if (ui_MACDevice) {
          char mac_str[20];
          if (wifi_manager_get_mac_info(mac_str, sizeof(mac_str))) {
            set_label_text_if_changed(ui_MACDevice, mac_str);
          }
        }

        // Queue Update
        if (ui_LabelQueue) {
          uint32_t q_used = 0, q_total = 0;
          uart_to_node_get_queue_status(&q_used, &q_total);
          set_label_fmt_if_changed(ui_LabelQueue, "%lu/%lu",
                                   (unsigned long)q_used,
                                   (unsigned long)q_total);
        }
      }
    }

    if (wait_ms < 5) {
      wait_ms = 5;
    } else if (wait_ms > 20) {
      wait_ms = 20;
    }
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
  }
}

void screen_manager_start(dm_metrics_t *metrics, dm_lvgl_t *lvgl,
                          dm_telemetry_t *telemetry, dm_hw_t *hw,
                          UBaseType_t priority, BaseType_t core_id) {
  if (metrics == NULL || lvgl == NULL)
    return;

  lv_init();
  lvgl_driver_init();

  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, DISP_BUF_SIZE);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.flush_cb = disp_driver_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.hor_res = LV_HOR_RES_MAX;
  disp_drv.ver_res = LV_VER_RES_MAX;
  lv_disp_drv_register(&disp_drv);

  ESP_ERROR_CHECK(xpt2046_soft_register_lvgl_indev());
  link_list_data_init();

  const esp_timer_create_args_t periodic_timer_args = {
      .callback = &lv_tick_task, .name = "lv_tick"};
  static esp_timer_handle_t periodic_timer;
  ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));

  ui_init();

  static screen_ctx_t s_screen_ctx;
  s_screen_ctx.metrics = metrics;
  s_screen_ctx.lvgl = lvgl;
  s_screen_ctx.telemetry = telemetry;
  s_screen_ctx.hw = hw;
  xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 4096, &s_screen_ctx, priority,
                          NULL, core_id);
}

