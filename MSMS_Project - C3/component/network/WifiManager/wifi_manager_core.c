#include "wifi_manager_internal.h"

typedef struct {
  const char *ssid;
  const char *password;
} wifi_boot_credential_t;

/*
 * Danh sách Wi-Fi thử khi khởi động.
 * Bạn có thể thêm nhiều phần tử tại đây.
 */
static const wifi_boot_credential_t s_boot_wifi_list[] = {
    {.ssid = CONFIG_WIFI_SSID, .password = CONFIG_WIFI_PASS},
    {.ssid = "MrKoi", .password = "12345789"},
    {.ssid = "Vinh Than", .password = "0987110001"},
    {.ssid = "Smile", .password = "hoiphongbencanh"},

};

static bool wifi_try_sta_once(wifi_manager_ctx_t *ctx, const char *ssid,
                              const char *password) {
  if (ssid == NULL || ssid[0] == '\0') {
    return false;
  }

  wifi_config_t wifi_config = {
      .sta =
          {
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
              .pmf_cfg = {.capable = true, .required = false},
          },
  };
  strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
  if (password != NULL) {
    strncpy((char *)wifi_config.sta.password, password,
            sizeof(wifi_config.sta.password) - 1);
  }

  if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    ESP_LOGW(WIFI_TAG, "WiFi mutex busy, skip attempt for SSID=%s", ssid);
    return false;
  }

  xEventGroupClearBits(ctx->event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT |
                                             WIFI_STA_LINKED_BIT);
  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  esp_wifi_start();
  xSemaphoreGive(ctx->mutex);

  ESP_LOGI(WIFI_TAG, "Attempting startup WiFi: SSID=%s", ssid);
  EventBits_t bits =
      xEventGroupWaitBits(ctx->event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                          pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
  if (bits & WIFI_CONNECTED_BIT) {
    strncpy(ctx->configured_ssid, ssid, sizeof(ctx->configured_ssid) - 1);
    ctx->configured_ssid[sizeof(ctx->configured_ssid) - 1] = '\0';
    strncpy(ctx->configured_password, password ? password : "",
            sizeof(ctx->configured_password) - 1);
    ctx->configured_password[sizeof(ctx->configured_password) - 1] = '\0';
    ctx->sta_configured = true;
    ctx->user_sta_configured = false;
    return true;
  }
  return false;
}

static wifi_manager_ctx_t s_ctx = {
    .ap_mode_active = false,
    .sta_configured = false,
    .user_sta_configured = false,
    .sta_connecting = false,
    .stop_requested = false,
    .connect_pending = false,
    .last_reconnect_attempt_tick = 0,
    .initialized = false,
    .event_handlers_registered = false,
    .sta_netif = NULL,
    .ap_netif = NULL,
    .mutex = NULL,
    .retry_num = 0,
    .server = NULL,
    .connect_task_handle = NULL,
    .manager_task_handle = NULL,
    .dns_server_task_handle = NULL,
    .dns_sock = -1,
    .dns_stop = false,
    .wifi_state = NULL,
    .ap_ip = "192.168.4.1",
    .ap_netmask = "255.255.255.0",
    .ap_gateway = "192.168.4.1",
};

wifi_manager_ctx_t *wifi_manager_ctx(void) { return &s_ctx; }

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  (void)arg;
  wifi_manager_ctx_t *ctx = wifi_manager_ctx();
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ctx->sta_connecting = true;
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    xEventGroupSetBits(ctx->event_group, WIFI_STA_LINKED_BIT);
    xEventGroupClearBits(ctx->event_group, WIFI_FAIL_BIT);
    ctx->sta_connecting = false;
    ESP_LOGI(WIFI_TAG, "WiFi STA link established");
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(ctx->event_group,
                         WIFI_CONNECTED_BIT | WIFI_STA_LINKED_BIT);
    xEventGroupSetBits(ctx->event_group, WIFI_FAIL_BIT);
    ctx->sta_connecting = false;
    ctx->last_reconnect_attempt_tick = xTaskGetTickCount();
    if (ctx->wifi_state) {
      ctx->wifi_state->sta_connected = false;
      ctx->wifi_state->status = STATUS_WARNING;
    }
    ESP_LOGW(WIFI_TAG, "WiFi disconnected, waiting for reconnect scheduler");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(WIFI_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    ctx->retry_num = 0;
    ctx->sta_connecting = false;
    xEventGroupClearBits(ctx->event_group, WIFI_FAIL_BIT);
    xEventGroupSetBits(ctx->event_group, WIFI_CONNECTED_BIT);
    if (ctx->wifi_state) {
      ctx->wifi_state->sta_connected = true;
      ctx->wifi_state->status = STATUS_OK;
    }
  }
}

void wifi_manager_attach_state(dm_wifi_t *wifi_state) {
  wifi_manager_ctx()->wifi_state = wifi_state;
}

esp_err_t wifi_init_common(void) {
  wifi_manager_ctx_t *ctx = wifi_manager_ctx();
  if (ctx->initialized) {
    ESP_LOGI(WIFI_TAG, "WiFi already initialized");
    return ESP_OK;
  }

  ESP_ERROR_CHECK(esp_netif_init());
  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_ERROR_CHECK(err);
  }

  if (ctx->sta_netif == NULL) {
    ctx->sta_netif = esp_netif_create_default_wifi_sta();
    if (ctx->sta_netif == NULL) {
      ESP_LOGE(WIFI_TAG, "Failed to create STA netif");
      return ESP_FAIL;
    }
  }

  if (ctx->ap_netif == NULL) {
    ctx->ap_netif = esp_netif_create_default_wifi_ap();
    if (ctx->ap_netif == NULL) {
      ESP_LOGE(WIFI_TAG, "Failed to create AP netif");
      return ESP_FAIL;
    }
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  if (!ctx->event_handlers_registered) {
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));
    ctx->event_handlers_registered = true;
  }

  ctx->initialized = true;
  return ESP_OK;
}

void wifi_init_sta(void) {
  wifi_manager_ctx_t *ctx = wifi_manager_ctx();
  ctx->stop_requested = false;
  if (ctx->event_group == NULL) {
    ctx->event_group = xEventGroupCreate();
  }
  if (ctx->mutex == NULL) {
    ctx->mutex = xSemaphoreCreateMutex();
  }

  wifi_init_common();
  wifi_http_load_config_from_nvs();

  bool connected = false;
  for (size_t i = 0; i < sizeof(s_boot_wifi_list) / sizeof(s_boot_wifi_list[0]);
       i++) {
    for (int attempt = 1; attempt <= 5; attempt++) {
      ESP_LOGI(WIFI_TAG, "Startup WiFi try %d/5 for SSID=%s", attempt,
               s_boot_wifi_list[i].ssid);
      if (wifi_try_sta_once(ctx, s_boot_wifi_list[i].ssid,
                            s_boot_wifi_list[i].password)) {
        ESP_LOGI(WIFI_TAG, "Connected startup WiFi: SSID=%s",
                 s_boot_wifi_list[i].ssid);
        connected = true;
        break;
      }
    }
    if (connected) {
      break;
    }
  }

  if (!connected) {
    ESP_LOGI(WIFI_TAG, "All startup WiFi credentials failed, starting AP mode");
    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      esp_wifi_stop();
      vTaskDelay(pdMS_TO_TICKS(500));
      wifi_init_ap();
      xSemaphoreGive(ctx->mutex);
    }
  }

  if (ctx->connect_task_handle == NULL) {
    xTaskCreate(wifi_connect_task, "wifi_connect", 4096, NULL, 5,
                &ctx->connect_task_handle);
  }
  if (ctx->manager_task_handle == NULL) {
    xTaskCreate(wifi_manager_task, "wifi_mgr", 4096, NULL, 5,
                &ctx->manager_task_handle);
  }
}

void wifi_manager_stop_tasks(void) {
  wifi_manager_ctx_t *ctx = wifi_manager_ctx();
  ctx->stop_requested = true;
  ctx->connect_pending = false;
  ctx->sta_connecting = false;
  ctx->sta_configured = false;
  ctx->user_sta_configured = false;
  ctx->last_reconnect_attempt_tick = xTaskGetTickCount();

  (void)stop_webserver();
  ctx->dns_stop = true;
  if (ctx->dns_sock >= 0) {
    shutdown(ctx->dns_sock, SHUT_RDWR);
    close(ctx->dns_sock);
    ctx->dns_sock = -1;
  }

  if (ctx->event_group != NULL) {
    xEventGroupClearBits(ctx->event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT |
                                               WIFI_STA_LINKED_BIT);
    xEventGroupSetBits(ctx->event_group, WIFI_AP_MODE_BIT);
  }

  for (int i = 0; i < 30; i++) {
    if (ctx->connect_task_handle == NULL && ctx->manager_task_handle == NULL) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ctx->initialized = false;
  ctx->ap_mode_active = false;
  ctx->sta_netif = NULL;
  ctx->ap_netif = NULL;
}

bool is_wifi_connected(void) {
  wifi_manager_ctx_t *ctx = wifi_manager_ctx();
  if (ctx->event_group == NULL) {
    return false;
  }
  EventBits_t bits = xEventGroupGetBits(ctx->event_group);
  return (bits & WIFI_CONNECTED_BIT) == WIFI_CONNECTED_BIT;
}

bool is_wifi_connecting(void) {
  wifi_manager_ctx_t *ctx = wifi_manager_ctx();
  if (ctx->event_group == NULL) {
    return false;
  }
  EventBits_t bits = xEventGroupGetBits(ctx->event_group);
  return !(bits & WIFI_STA_LINKED_BIT) && !(bits & WIFI_AP_MODE_BIT) &&
         !(bits & WIFI_FAIL_BIT) && ctx->connect_pending;
}

void wifi_manager_get_mac(uint8_t mac[6]) {
  if (mac == NULL)
    return;
  esp_wifi_get_mac(WIFI_IF_STA, mac);
}

int8_t wifi_manager_get_rssi(void) {
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    return ap_info.rssi;
  }
  return 0;
}
