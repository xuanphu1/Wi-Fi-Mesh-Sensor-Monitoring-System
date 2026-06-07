#include "ScreenManager.h"

#include "ui.h"
#include "WifiManager.h"
#include "lvgl.h"
#include "lvgl_helpers.h"
#include "esp_timer.h"
#include "esp_err.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DISP_BUF_SIZE];
static lv_color_t buf2[DISP_BUF_SIZE];

static void set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    if (label == NULL || text == NULL) {
        return;
    }

    const char *current = lv_label_get_text(label);
    if (current == NULL || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void set_label_fmt_if_changed(lv_obj_t *label, const char *fmt, ...)
{
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

static void set_obj_hidden_if_changed(lv_obj_t *obj, bool hidden)
{
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

static void ws_status_async_cb(void *user_data)
{
    bool connected = ((intptr_t)user_data != 0);
    if (ui_LabelStatus) {
        lv_label_set_text(ui_LabelStatus, connected ? "Connected" : "Disconnected");
    }
}

static void lv_tick_task(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

static void lvgl_task(void *arg)
{
    typedef struct {
        dm_metrics_t *metrics;
        dm_lvgl_t *lvgl;
    } screen_ctx_t;
    screen_ctx_t *ctx = (screen_ctx_t *)arg;
    ui_metrics_t m = {0};

    static const char *month_names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    TickType_t last_ui_update = 0;

    while (1)
    {
        if (ctx && ctx->metrics && ctx->metrics->mutex &&
            xSemaphoreTake(ctx->metrics->mutex, pdMS_TO_TICKS(5)) == pdTRUE)
        {
            m = ctx->metrics->value;
            if (ctx->lvgl) {
                ctx->lvgl->loop_counter++;
            }
            xSemaphoreGive(ctx->metrics->mutex);
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_ui_update) >= pdMS_TO_TICKS(250)) {
            last_ui_update = now;

            if (m.rtc_ok)
            {
                set_label_fmt_if_changed(ui_LabelHour, "%02d", m.rtc_time.tm_hour);
                set_label_fmt_if_changed(ui_LabelMinute, "%02d", m.rtc_time.tm_min);
                set_label_fmt_if_changed(ui_LabelSecond, "%02d", m.rtc_time.tm_sec);

                set_label_text_if_changed(ui_LabelWeekDay, getWeekDay((uint8_t)m.rtc_time.tm_wday));
                set_label_fmt_if_changed(ui_LabelDay, "%02d", m.rtc_time.tm_mday);
                if (m.rtc_time.tm_mon >= 0 && m.rtc_time.tm_mon <= 11) {
                    set_label_text_if_changed(ui_LabelMonth, month_names[m.rtc_time.tm_mon]);
                }
                set_label_fmt_if_changed(ui_Labelyear, "%04d", m.rtc_time.tm_year);
            }

            set_label_fmt_if_changed(ui_ValueBattery, "%d", m.battery_pct);
            set_label_fmt_if_changed(ui_LabelValueFPS, "%lu", (unsigned long)m.fps);

            if (ui_ValueMemory)
            {
                // Display in decimal GB to match SD card info logs (1 GB = 1,000,000 KB)
                const uint32_t kb_per_gb = 1000U * 1000U;
                uint32_t gb_whole = m.sd_total_kb / kb_per_gb;
                uint32_t gb_frac = (uint32_t)((m.sd_total_kb % kb_per_gb) * 100ULL / kb_per_gb);
                set_label_fmt_if_changed(ui_ValueMemory, "%lu.%02lu", (unsigned long)gb_whole, (unsigned long)gb_frac);
            }
            set_label_fmt_if_changed(ui_ValueMemoryUsed, "%lu", (unsigned long)m.sd_used_percent);
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
        }

        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms < 5) {
            wait_ms = 5;
        } else if (wait_ms > 20) {
            wait_ms = 20;
        }
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

void screen_manager_start(dm_metrics_t *metrics, dm_lvgl_t *lvgl, UBaseType_t priority, BaseType_t core_id)
{
    if (metrics == NULL || lvgl == NULL)
        return;

    lv_init();
    lvgl_driver_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    lv_disp_drv_register(&disp_drv);

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "lv_tick"};
    static esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));

    ui_init();

    static struct {
        dm_metrics_t *metrics;
        dm_lvgl_t *lvgl;
    } s_screen_ctx;
    s_screen_ctx.metrics = metrics;
    s_screen_ctx.lvgl = lvgl;
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 8192, &s_screen_ctx, priority, NULL, core_id);
}

void screen_manager_set_ws_status(bool connected)
{
    (void)lv_async_call(ws_status_async_cb, (void *)(intptr_t)(connected ? 1 : 0));
}
