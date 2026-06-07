#include "ESP_Bus.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct
{
    bool used;
    uint32_t event_id;
    esp_bus_handler_t handler;
    void *user_ctx;
} esp_bus_slot_t;

static SemaphoreHandle_t s_mutex;
static esp_bus_slot_t s_slots[ESP_BUS_MAX_SUBSCRIBERS];
static bool s_inited;

static bool event_matches(uint32_t subscribed, uint32_t published)
{
    if (subscribed == ESP_BUS_EVENT_ANY)
    {
        return true;
    }
    return subscribed == published;
}

esp_err_t esp_bus_init(void)
{
    if (s_inited)
    {
        return ESP_ERR_INVALID_STATE;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    memset(s_slots, 0, sizeof(s_slots));
    s_inited = true;
    return ESP_OK;
}

esp_err_t esp_bus_deinit(void)
{
    if (!s_inited)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_FAIL;
    }

    memset(s_slots, 0, sizeof(s_slots));
    s_inited = false;
    xSemaphoreGive(s_mutex);

    vSemaphoreDelete(s_mutex);
    s_mutex = NULL;
    return ESP_OK;
}

esp_err_t esp_bus_subscribe(uint32_t event_id,
                            esp_bus_handler_t handler,
                            void *user_ctx,
                            esp_bus_sub_handle_t *out_handle)
{
    if (!s_inited || handler == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_FAIL;
    }

    esp_err_t err = ESP_ERR_NO_MEM;
    for (int i = 0; i < ESP_BUS_MAX_SUBSCRIBERS; i++)
    {
        if (!s_slots[i].used)
        {
            s_slots[i].used = true;
            s_slots[i].event_id = event_id;
            s_slots[i].handler = handler;
            s_slots[i].user_ctx = user_ctx;
            if (out_handle != NULL)
            {
                *out_handle = (esp_bus_sub_handle_t)i;
            }
            err = ESP_OK;
            break;
        }
    }

    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t esp_bus_unsubscribe(esp_bus_sub_handle_t handle)
{
    if (!s_inited || handle < 0 || handle >= ESP_BUS_MAX_SUBSCRIBERS)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_FAIL;
    }

    memset(&s_slots[handle], 0, sizeof(esp_bus_slot_t));
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t esp_bus_publish(uint32_t event_id, const void *payload, size_t len)
{
    if (!s_inited)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (xPortInIsrContext())
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE)
    {
        return ESP_FAIL;
    }

    esp_bus_handler_t handlers[ESP_BUS_MAX_DISPATCH_SNAPSHOT];
    void *user_ctxs[ESP_BUS_MAX_DISPATCH_SNAPSHOT];
    int n = 0;

    for (int i = 0; i < ESP_BUS_MAX_SUBSCRIBERS && n < ESP_BUS_MAX_DISPATCH_SNAPSHOT; i++)
    {
        if (!s_slots[i].used)
        {
            continue;
        }
        if (!event_matches(s_slots[i].event_id, event_id))
        {
            continue;
        }
        handlers[n] = s_slots[i].handler;
        user_ctxs[n] = s_slots[i].user_ctx;
        n++;
    }

    xSemaphoreGive(s_mutex);

    for (int j = 0; j < n; j++)
    {
        if (handlers[j] != NULL)
        {
            handlers[j](event_id, payload, len, user_ctxs[j]);
        }
    }

    return ESP_OK;
}
