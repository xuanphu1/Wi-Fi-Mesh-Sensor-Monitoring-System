#include "wifi_manager_internal.h"

static void wifi_prepare_sta_config(wifi_config_t *wifi_config,
                                    const char *ssid,
                                    const char *password)
{
    memset(wifi_config, 0, sizeof(*wifi_config));
    wifi_config->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config->sta.pmf_cfg.capable = true;
    wifi_config->sta.pmf_cfg.required = false;
    strncpy((char *)wifi_config->sta.ssid, ssid, sizeof(wifi_config->sta.ssid) - 1);
    strncpy((char *)wifi_config->sta.password, password, sizeof(wifi_config->sta.password) - 1);
}

static void wifi_try_connect_configured_sta(wifi_manager_ctx_t *ctx,
                                            const char *ssid,
                                            const char *password)
{
    if (ctx->sta_connecting) {
        return;
    }

    wifi_config_t wifi_config;
    wifi_prepare_sta_config(&wifi_config, ssid, password);

    xEventGroupClearBits(ctx->event_group, WIFI_FAIL_BIT);
    esp_wifi_set_mode(ctx->ap_mode_active ? WIFI_MODE_APSTA : WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_OK || err == ESP_ERR_WIFI_STATE) {
        ctx->sta_connecting = true;
    } else {
        ESP_LOGW(WIFI_TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
    }
    ctx->last_reconnect_attempt_tick = xTaskGetTickCount();
}

void wifi_connect_task(void *pvParameters)
{
    (void)pvParameters;
    wifi_manager_ctx_t *ctx = wifi_manager_ctx();
    while (1) {
        if (ctx->connect_pending) {
            ESP_LOGI(WIFI_TAG, "Attempting to connect to WiFi: SSID=%s", ctx->pending_ssid);

            if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                xEventGroupClearBits(ctx->event_group,
                                     WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_STA_LINKED_BIT);
                ctx->retry_num = 0;

                if (!ctx->initialized) {
                    wifi_init_common();
                }

                wifi_try_connect_configured_sta(ctx, ctx->pending_ssid, ctx->pending_password);

                EventBits_t bits = xEventGroupWaitBits(
                    ctx->event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE,
                    pdFALSE, pdMS_TO_TICKS(10000));

                if (bits & WIFI_CONNECTED_BIT) {
                    ESP_LOGI(WIFI_TAG, "WiFi connected successfully");
                } else {
                    ESP_LOGE(WIFI_TAG, "WiFi connection failed");
                    if (!ctx->ap_mode_active) {
                        wifi_init_ap();
                    }
                }

                ctx->connect_pending = false;
                xSemaphoreGive(ctx->mutex);
            } else {
                ESP_LOGE(WIFI_TAG, "Failed to acquire WiFi mutex");
                ctx->connect_pending = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void wifi_manager_task(void *pvParameters)
{
    (void)pvParameters;
    wifi_manager_ctx_t *ctx = wifi_manager_ctx();
    while (1) {
        if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            EventBits_t bits = xEventGroupGetBits(ctx->event_group);

            if ((bits & WIFI_CONNECTED_BIT) && !(bits & WIFI_AP_MODE_BIT)) {
                ESP_LOGI(WIFI_TAG, "WiFi STA connected, enabling configuration AP in AP+STA mode");
                wifi_init_ap();
                bits = xEventGroupGetBits(ctx->event_group);
            }

            if (!(bits & WIFI_STA_LINKED_BIT) && !ctx->connect_pending) {
                if (ctx->user_sta_configured) {
                    TickType_t now = xTaskGetTickCount();
                    if (!(bits & WIFI_AP_MODE_BIT)) {
                        ESP_LOGI(WIFI_TAG, "WiFi disconnected, enabling AP while retrying STA");
                        wifi_init_ap();
                        bits = xEventGroupGetBits(ctx->event_group);
                    }
                    if ((ctx->last_reconnect_attempt_tick == 0) ||
                        ((now - ctx->last_reconnect_attempt_tick) >=
                         pdMS_TO_TICKS(WIFI_RECONNECT_INTERVAL_MS))) {
                        wifi_try_connect_configured_sta(ctx,
                                                        ctx->configured_ssid,
                                                        ctx->configured_password);
                        ESP_LOGI(WIFI_TAG, "Retrying configured WiFi in %d ms cycle: SSID=%s",
                                 WIFI_RECONNECT_INTERVAL_MS, ctx->configured_ssid);
                    }
                } else {
                    if (!(bits & WIFI_AP_MODE_BIT)) {
                        ESP_LOGI(WIFI_TAG, "WiFi disconnected, starting AP mode (no user config reconnect)");
                        esp_wifi_stop();
                        wifi_init_ap();
                    }
                }
            }

            xSemaphoreGive(ctx->mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
