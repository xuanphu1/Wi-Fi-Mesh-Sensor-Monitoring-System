#pragma once

#include "WifiManager.h"
#include "nvs_flash.h"

#define WIFI_TAG "WiFiManager"

typedef struct
{
    EventGroupHandle_t event_group;
    bool ap_mode_active;
    bool sta_configured;
    bool user_sta_configured;
    bool sta_connecting;
    volatile bool stop_requested;
    char pending_ssid[33];
    char pending_password[65];
    char configured_ssid[33];
    char configured_password[65];
    bool connect_pending;
    TickType_t last_reconnect_attempt_tick;
    bool initialized;
    bool event_handlers_registered;
    esp_netif_t *sta_netif;
    esp_netif_t *ap_netif;
    SemaphoreHandle_t mutex;
    int retry_num;
    httpd_handle_t server;
    TaskHandle_t connect_task_handle;
    TaskHandle_t manager_task_handle;
    TaskHandle_t dns_server_task_handle;
    int dns_sock;
    volatile bool dns_stop;
    dm_wifi_t *wifi_state;
    char ap_ip[16];
    char ap_netmask[16];
    char ap_gateway[16];
} wifi_manager_ctx_t;

wifi_manager_ctx_t *wifi_manager_ctx(void);

esp_err_t wifi_init_common(void);
void wifi_init_ap(void);
void dns_server_task(void *pvParameters);

esp_err_t save_ap_ip_config(const char *ip, const char *netmask, const char *gateway);
esp_err_t load_ap_ip_config(char *ip, char *netmask, char *gateway, size_t len);

esp_err_t wifi_http_load_config_from_nvs(void);
esp_err_t save_http_config(const char *ip, uint16_t port);
const char *get_http_ip(void);
uint16_t get_http_port(void);

esp_err_t start_webserver(void);
esp_err_t stop_webserver(void);
esp_err_t root_handler(httpd_req_t *req);
esp_err_t wifi_config_handler(httpd_req_t *req);
esp_err_t wifi_status_handler(httpd_req_t *req);
esp_err_t wifi_scan_handler(httpd_req_t *req);
esp_err_t redirect_handler(httpd_req_t *req);
esp_err_t common_get_handler(httpd_req_t *req, httpd_err_code_t error);
esp_err_t captive_portal_handler(httpd_req_t *req);
