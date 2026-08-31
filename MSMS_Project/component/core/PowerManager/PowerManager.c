/**
 * @file PowerManager.c
 * @brief Power management implementation for 3V3 (GPIO 12) and 5V (GPIO 14) rails.
 */
#include "PowerManager.h"
#include "PinManager.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG_POWER_MANAGER = "POWER_MANAGER";

typedef struct {
    gpio_num_t pin;
    bool is_on;
    TimerHandle_t timer;
    bool target_state_on_timeout;
    TickType_t timer_expire_tick;
} power_channel_ctx_t;

static power_channel_ctx_t s_channels[2]; // index 0: 3V3, index 1: 5V
static SemaphoreHandle_t s_power_mutex = NULL;
static bool s_initialized = false;

#define CH_IDX_3V3 0
#define CH_IDX_5V  1

static void timer_callback_3v3(TimerHandle_t xTimer) {
    (void)xTimer;
    if (s_power_mutex && xSemaphoreTake(s_power_mutex, portMAX_DELAY) == pdTRUE) {
        bool target = s_channels[CH_IDX_3V3].target_state_on_timeout;
        s_channels[CH_IDX_3V3].is_on = target;
        gpio_set_level(s_channels[CH_IDX_3V3].pin, target ? 1 : 0);
        s_channels[CH_IDX_3V3].timer_expire_tick = 0;
        ESP_LOGI(TAG_POWER_MANAGER, "Timer fired: 3V3 rail set to %s", target ? "ON" : "OFF");
        xSemaphoreGive(s_power_mutex);
    }
}

static void timer_callback_5v(TimerHandle_t xTimer) {
    (void)xTimer;
    if (s_power_mutex && xSemaphoreTake(s_power_mutex, portMAX_DELAY) == pdTRUE) {
        bool target = s_channels[CH_IDX_5V].target_state_on_timeout;
        s_channels[CH_IDX_5V].is_on = target;
        gpio_set_level(s_channels[CH_IDX_5V].pin, target ? 1 : 0);
        s_channels[CH_IDX_5V].timer_expire_tick = 0;
        ESP_LOGI(TAG_POWER_MANAGER, "Timer fired: 5V rail set to %s", target ? "ON" : "OFF");
        xSemaphoreGive(s_power_mutex);
    }
}

esp_err_t PowerManager_Init(void) {
    if (s_initialized) {
        return ESP_OK;
    }

    if (s_power_mutex == NULL) {
        s_power_mutex = xSemaphoreCreateMutex();
        if (s_power_mutex == NULL) {
            ESP_LOGE(TAG_POWER_MANAGER, "Failed to create power mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    memset(s_channels, 0, sizeof(s_channels));
    s_channels[CH_IDX_3V3].pin = PIN_POWER_3V3;
    s_channels[CH_IDX_5V].pin = PIN_POWER_5V;

    uint64_t pin_mask = (1ULL << PIN_POWER_3V3) | (1ULL << PIN_POWER_5V);
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_POWER_MANAGER, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    // Default: Turn OFF both rails (Low active is 0 when High Active)
    gpio_set_level(PIN_POWER_3V3, 0);
    gpio_set_level(PIN_POWER_5V, 0);
    s_channels[CH_IDX_3V3].is_on = false;
    s_channels[CH_IDX_5V].is_on = false;

    // Create FreeRTOS software timers (one-shot)
    s_channels[CH_IDX_3V3].timer = xTimerCreate("pwr_3v3_tmr", pdMS_TO_TICKS(1000), pdFALSE, NULL, timer_callback_3v3);
    s_channels[CH_IDX_5V].timer = xTimerCreate("pwr_5v_tmr", pdMS_TO_TICKS(1000), pdFALSE, NULL, timer_callback_5v);

    if (s_channels[CH_IDX_3V3].timer == NULL || s_channels[CH_IDX_5V].timer == NULL) {
        ESP_LOGE(TAG_POWER_MANAGER, "Failed to create power timers");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG_POWER_MANAGER, "PowerManager initialized (3V3: GPIO %d, 5V: GPIO %d)",
             PIN_POWER_3V3, PIN_POWER_5V);
    return ESP_OK;
}

static esp_err_t set_channel_internal(int idx, bool enable) {
    if (idx < 0 || idx > 1) return ESP_ERR_INVALID_ARG;

    // Cancel any running timer when manual set is called
    if (s_channels[idx].timer != NULL && xTimerIsTimerActive(s_channels[idx].timer) != pdFALSE) {
        xTimerStop(s_channels[idx].timer, 0);
        s_channels[idx].timer_expire_tick = 0;
    }

    s_channels[idx].is_on = enable;
    esp_err_t err = gpio_set_level(s_channels[idx].pin, enable ? 1 : 0);
    ESP_LOGI(TAG_POWER_MANAGER, "%s rail set to %s (GPIO %d)",
             (idx == CH_IDX_3V3) ? "3V3" : "5V", enable ? "ON" : "OFF", s_channels[idx].pin);
    return err;
}

esp_err_t PowerManager_SetState(power_channel_t channel, bool enable) {
    if (!s_initialized) {
        esp_err_t init_err = PowerManager_Init();
        if (init_err != ESP_OK) return init_err;
    }

    if (s_power_mutex && xSemaphoreTake(s_power_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    if (channel & POWER_CHANNEL_3V3) {
        ret = set_channel_internal(CH_IDX_3V3, enable);
    }
    if ((channel & POWER_CHANNEL_5V) && ret == ESP_OK) {
        ret = set_channel_internal(CH_IDX_5V, enable);
    }

    if (s_power_mutex) xSemaphoreGive(s_power_mutex);
    return ret;
}

bool PowerManager_GetState(power_channel_t channel) {
    if (!s_initialized) return false;

    bool state = false;
    if (s_power_mutex && xSemaphoreTake(s_power_mutex, portMAX_DELAY) == pdTRUE) {
        if (channel == POWER_CHANNEL_3V3) {
            state = s_channels[CH_IDX_3V3].is_on;
        } else if (channel == POWER_CHANNEL_5V) {
            state = s_channels[CH_IDX_5V].is_on;
        }
        xSemaphoreGive(s_power_mutex);
    }
    return state;
}

esp_err_t PowerManager_Toggle(power_channel_t channel) {
    if (!s_initialized) {
        esp_err_t init_err = PowerManager_Init();
        if (init_err != ESP_OK) return init_err;
    }

    if (s_power_mutex && xSemaphoreTake(s_power_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (channel & POWER_CHANNEL_3V3) {
        set_channel_internal(CH_IDX_3V3, !s_channels[CH_IDX_3V3].is_on);
    }
    if (channel & POWER_CHANNEL_5V) {
        set_channel_internal(CH_IDX_5V, !s_channels[CH_IDX_5V].is_on);
    }

    if (s_power_mutex) xSemaphoreGive(s_power_mutex);
    return ESP_OK;
}

static esp_err_t schedule_internal(int idx, bool target_state, uint32_t delay_ms) {
    if (idx < 0 || idx > 1 || delay_ms == 0) return ESP_ERR_INVALID_ARG;

    s_channels[idx].target_state_on_timeout = target_state;
    TickType_t ticks = pdMS_TO_TICKS(delay_ms);
    if (ticks == 0) ticks = 1;

    s_channels[idx].timer_expire_tick = xTaskGetTickCount() + ticks;
    if (xTimerChangePeriod(s_channels[idx].timer, ticks, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG_POWER_MANAGER, "Failed to start timer for %s", (idx == CH_IDX_3V3) ? "3V3" : "5V");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_POWER_MANAGER, "Scheduled %s to turn %s in %lu ms",
             (idx == CH_IDX_3V3) ? "3V3" : "5V", target_state ? "ON" : "OFF", (unsigned long)delay_ms);
    return ESP_OK;
}

esp_err_t PowerManager_EnableForDuration(power_channel_t channel, uint32_t duration_ms) {
    if (!s_initialized) {
        esp_err_t init_err = PowerManager_Init();
        if (init_err != ESP_OK) return init_err;
    }

    if (duration_ms == 0) {
        return PowerManager_SetState(channel, false);
    }

    if (s_power_mutex && xSemaphoreTake(s_power_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (channel & POWER_CHANNEL_3V3) {
        s_channels[CH_IDX_3V3].is_on = true;
        gpio_set_level(s_channels[CH_IDX_3V3].pin, 1);
        schedule_internal(CH_IDX_3V3, false, duration_ms);
    }
    if (channel & POWER_CHANNEL_5V) {
        s_channels[CH_IDX_5V].is_on = true;
        gpio_set_level(s_channels[CH_IDX_5V].pin, 1);
        schedule_internal(CH_IDX_5V, false, duration_ms);
    }

    if (s_power_mutex) xSemaphoreGive(s_power_mutex);
    return ESP_OK;
}

esp_err_t PowerManager_ScheduleAction(power_channel_t channel, bool target_state, uint32_t delay_ms) {
    if (!s_initialized) {
        esp_err_t init_err = PowerManager_Init();
        if (init_err != ESP_OK) return init_err;
    }

    if (delay_ms == 0) {
        return PowerManager_SetState(channel, target_state);
    }

    if (s_power_mutex && xSemaphoreTake(s_power_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (channel & POWER_CHANNEL_3V3) {
        schedule_internal(CH_IDX_3V3, target_state, delay_ms);
    }
    if (channel & POWER_CHANNEL_5V) {
        schedule_internal(CH_IDX_5V, target_state, delay_ms);
    }

    if (s_power_mutex) xSemaphoreGive(s_power_mutex);
    return ESP_OK;
}

esp_err_t PowerManager_CancelTimer(power_channel_t channel) {
    if (!s_initialized) return ESP_OK;

    if (s_power_mutex && xSemaphoreTake(s_power_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if ((channel & POWER_CHANNEL_3V3) && s_channels[CH_IDX_3V3].timer) {
        xTimerStop(s_channels[CH_IDX_3V3].timer, 0);
        s_channels[CH_IDX_3V3].timer_expire_tick = 0;
        ESP_LOGI(TAG_POWER_MANAGER, "Cancelled timer for 3V3 rail");
    }
    if ((channel & POWER_CHANNEL_5V) && s_channels[CH_IDX_5V].timer) {
        xTimerStop(s_channels[CH_IDX_5V].timer, 0);
        s_channels[CH_IDX_5V].timer_expire_tick = 0;
        ESP_LOGI(TAG_POWER_MANAGER, "Cancelled timer for 5V rail");
    }

    if (s_power_mutex) xSemaphoreGive(s_power_mutex);
    return ESP_OK;
}

uint32_t PowerManager_GetRemainingTimeMs(power_channel_t channel) {
    if (!s_initialized) return 0;

    uint32_t remaining = 0;
    if (s_power_mutex && xSemaphoreTake(s_power_mutex, portMAX_DELAY) == pdTRUE) {
        int idx = (channel == POWER_CHANNEL_5V) ? CH_IDX_5V : CH_IDX_3V3;
        if (s_channels[idx].timer != NULL && xTimerIsTimerActive(s_channels[idx].timer) != pdFALSE) {
            TickType_t now = xTaskGetTickCount();
            if (s_channels[idx].timer_expire_tick > now) {
                remaining = (uint32_t)((s_channels[idx].timer_expire_tick - now) * portTICK_PERIOD_MS);
            }
        }
        xSemaphoreGive(s_power_mutex);
    }
    return remaining;
}
