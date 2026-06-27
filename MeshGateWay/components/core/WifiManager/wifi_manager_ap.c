#include "wifi_manager_internal.h"

#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>

/** Trả về chỉ số byte ngay sau QNAME (tức là byte đầu của QTYPE). */
static int dns_skip_qname(const uint8_t *pkt, int pkt_len, int pos)
{
    while (pos < pkt_len) {
        uint8_t lab = pkt[pos];
        if (lab == 0U) {
            return pos + 1;
        }
        if ((lab & 0xC0U) == 0xC0U) {
            return pos + 2;
        }
        if (lab > 63U) {
            return -1;
        }
        pos += 1 + lab;
    }
    return -1;
}

esp_err_t save_ap_ip_config(const char *ip, const char *netmask, const char *gateway)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_ap", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to open NVS wifi_ap namespace: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, "ap_ip", ip);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to save AP IP: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, "ap_netmask", netmask);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to save AP netmask: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, "ap_gateway", gateway);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to save AP gateway: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to commit AP IP config: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(WIFI_TAG, "AP IP config saved: IP=%s, Netmask=%s, Gateway=%s", ip, netmask, gateway);
    return ESP_OK;
}

esp_err_t load_ap_ip_config(char *ip, char *netmask, char *gateway, size_t len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_ap", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(WIFI_TAG, "No saved AP IP config found, using defaults");
        return err;
    }

    size_t required_size = len;
    err = nvs_get_str(nvs_handle, "ap_ip", ip, &required_size);
    if (err != ESP_OK) {
        ESP_LOGI(WIFI_TAG, "Failed to read AP IP, using default");
        nvs_close(nvs_handle);
        return err;
    }

    required_size = len;
    err = nvs_get_str(nvs_handle, "ap_netmask", netmask, &required_size);
    if (err != ESP_OK) {
        ESP_LOGI(WIFI_TAG, "Failed to read AP netmask, using default");
        nvs_close(nvs_handle);
        return err;
    }

    required_size = len;
    err = nvs_get_str(nvs_handle, "ap_gateway", gateway, &required_size);
    if (err != ESP_OK) {
        ESP_LOGI(WIFI_TAG, "Failed to read AP gateway, using default");
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(WIFI_TAG, "AP IP config loaded: IP=%s, Netmask=%s, Gateway=%s", ip, netmask, gateway);
    return ESP_OK;
}

void dns_server_task(void *pvParameters)
{
    (void)pvParameters;
    wifi_manager_ctx_t *ctx = wifi_manager_ctx();
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    uint8_t rx_buffer[DNS_MAX_LEN];
    uint8_t tx_buffer[DNS_MAX_LEN];
    char *ip_addr = ctx->ap_ip;

    if (sock < 0) {
        ESP_LOGE(WIFI_TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    ctx->dns_sock = sock;
    ctx->dns_stop = false;

    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DNS_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(WIFI_TAG, "Failed to bind socket");
        close(sock);
        ctx->dns_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    /* Poll stop flag periodically (recv timeout). */
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (!ctx->dns_stop) {
        socklen_t socklen = sizeof(client_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0,
                           (struct sockaddr *)&client_addr, &socklen);

        if (ctx->dns_stop) {
            break;
        }

        if (len < 0) {
            /* Timeout or transient error; loop to re-check stop flag. */
            continue;
        }

        if (len <= 12) {
            continue;
        }
        /* Chỉ trả lời query chuẩn (QR=0). */
        if ((rx_buffer[2] & 0x80U) != 0U) {
            continue;
        }

        int qtype_off = dns_skip_qname(rx_buffer, len, 12);
        if (qtype_off < 0 || qtype_off + 4 > len) {
            continue;
        }

        uint16_t qtype =
            (uint16_t)((uint16_t)rx_buffer[qtype_off] << 8) | (uint16_t)rx_buffer[qtype_off + 1];

        int qend = qtype_off + 4;
        memcpy(tx_buffer, rx_buffer, (size_t)qend);

        tx_buffer[2] |= 0x80U;
        tx_buffer[6] = 0;
        tx_buffer[7] = 1;

        int off = qend;
        tx_buffer[off++] = 0xC0;
        tx_buffer[off++] = 0x0C;

        if (qtype == 28U) {
            /* AAAA — nhiều điện thoại gửi trước A; trả IPv4 map trong IPv6 (::ffff:x.x.x.x). */
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 28;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 1;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 60;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 16;
            memset(tx_buffer + off, 0, 10);
            tx_buffer[off + 10] = 0xff;
            tx_buffer[off + 11] = 0xff;
            if (inet_pton(AF_INET, ip_addr, tx_buffer + off + 12) != 1) {
                continue;
            }
            off += 16;
        } else {
            /* A và các kiểu khác (mặc định): trả A gateway cho captive. */
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 1;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 1;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 60;
            tx_buffer[off++] = 0;
            tx_buffer[off++] = 4;
            if (inet_pton(AF_INET, ip_addr, tx_buffer + off) != 1) {
                continue;
            }
            off += 4;
        }

        if (off > DNS_MAX_LEN) {
            continue;
        }

        sendto(sock, tx_buffer, (size_t)off, 0, (struct sockaddr *)&client_addr,
               sizeof(client_addr));
    }

    if (ctx->dns_sock >= 0) {
        close(ctx->dns_sock);
        ctx->dns_sock = -1;
    }
    ctx->dns_server_task_handle = NULL;
    vTaskDelete(NULL);
}

void wifi_init_ap(void)
{
    wifi_manager_ctx_t *ctx = wifi_manager_ctx();
    if (ctx->ap_mode_active) {
        ESP_LOGI(WIFI_TAG, "WiFi AP already active at http://%s/", ctx->ap_ip);
        (void)start_webserver();
        return;
    }

    ESP_LOGI(WIFI_TAG, "Initializing WiFi AP mode");
    if (!ctx->initialized) {
        wifi_init_common();
    }

    if (load_ap_ip_config(ctx->ap_ip, ctx->ap_netmask, ctx->ap_gateway, sizeof(ctx->ap_ip)) != ESP_OK) {
        strncpy(ctx->ap_ip, "192.168.4.1", sizeof(ctx->ap_ip) - 1);
        strncpy(ctx->ap_netmask, "255.255.255.0", sizeof(ctx->ap_netmask) - 1);
        strncpy(ctx->ap_gateway, "192.168.4.1", sizeof(ctx->ap_gateway) - 1);
        ctx->ap_ip[sizeof(ctx->ap_ip) - 1] = '\0';
        ctx->ap_netmask[sizeof(ctx->ap_netmask) - 1] = '\0';
        ctx->ap_gateway[sizeof(ctx->ap_gateway) - 1] = '\0';
    }

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ctx->ap_netif));

    esp_netif_ip_info_t ip_info = {.ip.addr = ipaddr_addr(ctx->ap_ip),
                                   .netmask.addr = ipaddr_addr(ctx->ap_netmask),
                                   .gw.addr = ipaddr_addr(ctx->ap_gateway)};
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ctx->ap_netif, &ip_info));

    esp_netif_dns_info_t dns_info = {0};
    dns_info.ip.u_addr.ip4.addr = ipaddr_addr(ctx->ap_ip);
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ctx->ap_netif, ESP_NETIF_DNS_MAIN, &dns_info));
    uint8_t dhcps_offer_dns = DHCPS_OFFER_DNS;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ctx->ap_netif,
                                           ESP_NETIF_OP_SET,
                                           ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &dhcps_offer_dns,
                                           sizeof(dhcps_offer_dns)));

    ESP_ERROR_CHECK(esp_netif_dhcps_start(ctx->ap_netif));

    wifi_config_t wifi_config = {
        .ap =
            {
                .ssid = AP_SSID,
                .ssid_len = strlen(AP_SSID),
                .password = "",
                .max_connection = AP_MAX_CONN,
                .authmode = WIFI_AUTH_OPEN,
                .channel = AP_CHANNEL,
            },
    };

    // APSTA giúp captive portal vẫn phát AP và đồng thời quét được Wi-Fi xung quanh.
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(WIFI_TAG, "WiFi AP started with SSID: %s (No password required)", AP_SSID);
    ESP_LOGI(WIFI_TAG, "AP IP address: %s", ctx->ap_ip);

    if (ctx->dns_server_task_handle == NULL) {
        xTaskCreate(dns_server_task, "dns_server", 2560, NULL, 5, &ctx->dns_server_task_handle);
    }

    ctx->ap_mode_active = true;
    xEventGroupSetBits(ctx->event_group, WIFI_AP_MODE_BIT);

    (void)start_webserver();
}
