#include "mesh_network.h"

#include "esp_bridge.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_mesh_lite.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "sdkconfig.h"
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifndef CONFIG_MESH_CHANNEL
#define CONFIG_MESH_CHANNEL 11
#endif

static const char *TAG = "MeshNetwork";
static bool s_initialized;
static bool s_started_once;
static TimerHandle_t s_info_timer;

static void configure_wifi(void) {
  wifi_config_t station = {0};
  station.sta.pmf_cfg.capable = false;
  station.sta.pmf_cfg.required = false;
  esp_err_t result = esp_bridge_wifi_set_config(WIFI_IF_STA, &station);
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "Configure mesh STA failed: %s", esp_err_to_name(result));
  }

  wifi_config_t access_point = {
      .ap = {.ssid = CONFIG_BRIDGE_SOFTAP_SSID,
             .password = CONFIG_BRIDGE_SOFTAP_PASSWORD,
             .channel = CONFIG_MESH_CHANNEL},
  };
  access_point.ap.pmf_cfg.capable = false;
  access_point.ap.pmf_cfg.required = false;
  result = esp_bridge_wifi_set_config(WIFI_IF_AP, &access_point);
  if (result != ESP_OK) {
    ESP_LOGW(TAG, "Configure mesh AP failed: %s", esp_err_to_name(result));
  }
}

static void configure_softap_identity(void) {
  char ssid[33] = {0};
  char password[64] = {0};
  size_t ssid_size = sizeof(ssid);
  size_t password_size = sizeof(password);

  if (esp_mesh_lite_get_softap_ssid_from_nvs(ssid, &ssid_size) != ESP_OK) {
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_AP, mac);
#ifdef CONFIG_BRIDGE_SOFTAP_SSID_END_WITH_THE_MAC
    snprintf(ssid, sizeof(ssid), "%.25s_%02x%02x%02x",
             CONFIG_BRIDGE_SOFTAP_SSID, mac[3], mac[4], mac[5]);
#else
    snprintf(ssid, sizeof(ssid), "%.32s", CONFIG_BRIDGE_SOFTAP_SSID);
#endif
  }

  if (esp_mesh_lite_get_softap_psw_from_nvs(password, &password_size) !=
      ESP_OK) {
    strlcpy(password, CONFIG_BRIDGE_SOFTAP_PASSWORD, sizeof(password));
  }
  esp_mesh_lite_set_softap_info(ssid, password);
}

static esp_err_t start_wifi(void) {
  wifi_mode_t mode = WIFI_MODE_NULL;
  esp_err_t result = esp_wifi_get_mode(&mode);
  if (result != ESP_OK) {
    return result;
  }
  if (mode == WIFI_MODE_NULL) {
    result = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (result != ESP_OK) {
      return result;
    }
  }
  result = esp_wifi_start();
  return result == ESP_ERR_WIFI_STATE ? ESP_OK : result;
}

static void disable_pmf(void) {
  wifi_config_t config;
  if (esp_wifi_get_config(WIFI_IF_AP, &config) == ESP_OK) {
    config.ap.pmf_cfg.capable = false;
    config.ap.pmf_cfg.required = false;
    esp_wifi_set_config(WIFI_IF_AP, &config);
  }
  if (esp_wifi_get_config(WIFI_IF_STA, &config) == ESP_OK) {
    config.sta.pmf_cfg.capable = false;
    config.sta.pmf_cfg.required = false;
    esp_wifi_set_config(WIFI_IF_STA, &config);
  }
}

static void log_system_info(TimerHandle_t timer) {
  (void)timer;
  uint8_t channel = 0;
  wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
  uint8_t station_mac[6] = {0};
  wifi_ap_record_t parent = {0};
  wifi_sta_list_t children = {0};

  esp_wifi_get_channel(&channel, &secondary);
  esp_wifi_get_mac(WIFI_IF_STA, station_mac);
  esp_wifi_sta_get_ap_info(&parent);
  esp_wifi_ap_get_sta_list(&children);

  ESP_LOGI(TAG,
           "ch=%u level=%d self=" MACSTR " parent=" MACSTR
           " rssi=%d children=%u heap=%" PRIu32,
           (unsigned)channel, esp_mesh_lite_get_level(), MAC2STR(station_mac),
           MAC2STR(parent.bssid), parent.rssi, (unsigned)children.num,
           esp_get_free_heap_size());
}

esp_err_t mesh_network_set_role(mesh_role_t role) {
  esp_err_t result = start_wifi();
  if (result != ESP_OK) {
    return result;
  }

  if (role == MESH_ROLE_ROOT) {
    result = esp_mesh_lite_set_allowed_level(1);
    esp_mesh_lite_set_wifi_reconnect_interval(4000000, 0, 4000000);
    esp_err_t disconnect_result = esp_wifi_disconnect();
    if (disconnect_result != ESP_OK &&
        disconnect_result != ESP_ERR_WIFI_NOT_CONNECT &&
        disconnect_result != ESP_ERR_WIFI_NOT_STARTED) {
      ESP_LOGW(TAG, "Detach old parent failed: %s",
               esp_err_to_name(disconnect_result));
    }
  } else {
    esp_mesh_lite_set_allowed_level(0);
    result = esp_mesh_lite_set_disallowed_level(1);
    esp_mesh_lite_set_wifi_reconnect_interval(3, 2, 5);
    esp_wifi_disconnect();
    if (result == ESP_OK && s_started_once) {
      esp_mesh_lite_connect();
    }
  }
  return result;
}

esp_err_t mesh_network_start(mesh_role_t role) {
  esp_err_t result = esp_netif_init();
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
    return result;
  }
  result = esp_event_loop_create_default();
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
    return result;
  }

  esp_bridge_create_all_netif();
  configure_wifi();

  if (!s_initialized) {
    esp_mesh_lite_config_t config = ESP_MESH_LITE_DEFAULT_INIT();
    config.join_mesh_ignore_router_status = true;
    config.join_mesh_without_configured_wifi = true;
    esp_mesh_lite_init(&config);
    s_initialized = true;
  }

  configure_softap_identity();
  result = mesh_network_set_role(role);
  if (result != ESP_OK) {
    return result;
  }

  if (!s_started_once) {
    esp_mesh_lite_start();
    s_started_once = true;
  }
  disable_pmf();

  if (s_info_timer == NULL) {
    s_info_timer = xTimerCreate("mesh_info", pdMS_TO_TICKS(10000), pdTRUE,
                                NULL, log_system_info);
    if (s_info_timer != NULL) {
      xTimerStart(s_info_timer, 0);
    }
  }
  return ESP_OK;
}

void mesh_network_stop_runtime(void) {
  if (s_info_timer != NULL) {
    xTimerStop(s_info_timer, 0);
    xTimerDelete(s_info_timer, 0);
    s_info_timer = NULL;
  }
}
