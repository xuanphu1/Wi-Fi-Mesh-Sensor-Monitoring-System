#ifndef __WIFI_MANAGER_H__
#define __WIFI_MANAGER_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_http_server.h"

#include "cJSON.h"
#include "Datamanager.h"

/** Gắn state Wi-Fi để cập nhật STA status khi nhận event. */
void wifi_manager_attach_state(dm_wifi_t *wifi_state, dm_ws_t *ws_state);

// WiFi event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_AP_MODE_BIT BIT2
#define WIFI_STA_LINKED_BIT BIT3

// WiFi configuration
#define AP_SSID CONFIG_AP_SSID
#define AP_PASSWORD CONFIG_AP_PASSWORD
#define AP_CHANNEL CONFIG_AP_CHANNEL
#define AP_MAX_CONN 4
#define DNS_PORT 53
#define DNS_MAX_LEN 512
#define DHCPS_OFFER_DNS 0x02
#define WIFI_RECONNECT_INTERVAL_MS 5000



void wifi_init_sta(void);
void wifi_connect_task(void *pvParameters);
void wifi_manager_task(void *pvParameters);
bool is_wifi_connected(void);
bool is_wifi_connecting(void);

/** Get current STA IP string. Return true if success. */
bool wifi_manager_get_ip_info(char *ip_buf, size_t max_len);

/** Get current STA MAC string. Return true if success. */
bool wifi_manager_get_mac_info(char *mac_buf, size_t max_len);


#endif // __WIFI_MANAGER_H__
