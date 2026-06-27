#include "mdns.h"
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
    {.ssid = "Chung Cu Mini MoMo", .password = "21082021"},
    {.ssid = "Smile", .password = "hoiphongbencanh"}
    // {.ssid = "Unknown", .password = "12345789"},
    // {.ssid = "Vinh Than", .password = "0987110001"},

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
                          pdFALSE, pdFALSE, pdMS_TO_TICKS(20000));
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
    .connect_pending = false,
    .last_reconnect_attempt_tick = 0,
    .initialized = false,
    .sta_netif = NULL,
    .ap_netif = NULL,
    .mutex = NULL,
    .retry_num = 0,
    .server = NULL,
    .dns_server_task_handle = NULL,
    .dns_sock = -1,
    .dns_stop = false,
    .wifi_state = NULL,
    .ap_ip = "192.168.4.1",
    .ap_netmask = "255.255.255.0",
    .ap_gateway = "192.168.4.1",
};

static void wifi_manager_start_mdns(void) {
  static bool s_mdns_started;

  if (s_mdns_started) {
    return;
  }

  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGW(WIFI_TAG, "mDNS init failed: %s", esp_err_to_name(err));
    return;
  }

  err = mdns_hostname_set("meshgateway");
  if (err != ESP_OK) {
    ESP_LOGW(WIFI_TAG, "mDNS hostname set failed: %s", esp_err_to_name(err));
    return;
  }
  (void)mdns_instance_name_set("MeshGateway Configuration");

  mdns_txt_item_t service_txt[] = {
      {.key = "path", .value = "/"},
  };
  err = mdns_service_add("MeshGateway HTTP", "_http", "_tcp", 80, service_txt,
                         sizeof(service_txt) / sizeof(service_txt[0]));
  if (err != ESP_OK) {
    ESP_LOGW(WIFI_TAG, "mDNS HTTP service add failed: %s",
             esp_err_to_name(err));
    return;
  }

  s_mdns_started = true;
  ESP_LOGI(WIFI_TAG, "mDNS started: http://meshgateway.local");
}

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
    wifi_manager_start_mdns();
  }
}

void wifi_manager_attach_state(dm_wifi_t *wifi_state, dm_ws_t *ws_state) {
  wifi_manager_ctx()->wifi_state = wifi_state;
  wifi_manager_ctx()->ws_state = ws_state;
}

esp_err_t wifi_init_common(void) {
  wifi_manager_ctx_t *ctx = wifi_manager_ctx();
  if (ctx->initialized) {
    ESP_LOGI(WIFI_TAG, "WiFi already initialized");
    return ESP_OK;
  }

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

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

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler, NULL));

  ctx->initialized = true;
  return ESP_OK;
}

void wifi_init_sta(void) {
  wifi_manager_ctx_t *ctx = wifi_manager_ctx();
  ctx->event_group = xEventGroupCreate();
  ctx->mutex = xSemaphoreCreateMutex();

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

  if (connected) {
    ESP_LOGI(WIFI_TAG,
             "Startup WiFi connected, keeping configuration AP active");
  } else {
    ESP_LOGI(WIFI_TAG,
             "All startup WiFi credentials failed, starting configuration AP");
  }
  if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (!connected) {
      esp_wifi_stop();
      vTaskDelay(pdMS_TO_TICKS(500));
    }
    wifi_init_ap();
    xSemaphoreGive(ctx->mutex);
  }

  static bool wifi_bg_tasks_started;
  if (!wifi_bg_tasks_started) {
    wifi_bg_tasks_started = true;
    xTaskCreate(wifi_connect_task, "wifi_connect", 4096, NULL, 5, NULL);
    xTaskCreate(wifi_manager_task, "wifi_mgr", 4096, NULL, 5, NULL);
  }
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

bool wifi_manager_get_ip_info(char *ip_buf, size_t max_len) {
  if (!ip_buf || max_len == 0)
    return false;
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (!netif) {
    snprintf(ip_buf, max_len, "0.0.0.0");
    return false;
  }
  esp_netif_ip_info_t ip_info;
  if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
    snprintf(ip_buf, max_len, IPSTR, IP2STR(&ip_info.ip));
    return true;
  }
  snprintf(ip_buf, max_len, "0.0.0.0");
  return false;
}

bool wifi_manager_get_mac_info(char *mac_buf, size_t max_len) {
  if (!mac_buf || max_len == 0)
    return false;
  uint8_t mac[6] = {0};
  if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
    snprintf(mac_buf, max_len, "...%02X:%02X:%02X", mac[3], mac[4], mac[5]);
    return true;
  }
  snprintf(mac_buf, max_len, "00:00:00:00:00:00");
  return false;
}
