#include "wifi_manager_internal.h"
#include "WSHandle.h"

#if defined(CONFIG_HTTP_PORT)
static uint16_t s_http_port_store = CONFIG_HTTP_PORT;
#else
static uint16_t s_http_port_store = 80;
#endif
static char s_http_ip_store[16] = "0.0.0.0";

esp_err_t wifi_http_load_config_from_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("http_cfg", NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;
    }
    size_t len = sizeof(s_http_ip_store);
    err = nvs_get_str(h, "http_ip", s_http_ip_store, &len);
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }
    uint16_t p = 0;
    if (nvs_get_u16(h, "http_port", &p) == ESP_OK && p != 0) {
        s_http_port_store = p;
    }
    nvs_close(h);
    return ESP_OK;
}

esp_err_t save_http_config(const char *ip, uint16_t port)
{
    if (ip == NULL || strlen(ip) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open("http_cfg", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, "http_ip", ip);
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }
    err = nvs_set_u16(h, "http_port", port);
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }
    err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        return err;
    }
    strncpy(s_http_ip_store, ip, sizeof(s_http_ip_store) - 1);
    s_http_ip_store[sizeof(s_http_ip_store) - 1] = '\0';
    s_http_port_store = port;
    return ESP_OK;
}

const char *get_http_ip(void)
{
    return s_http_ip_store;
}

uint16_t get_http_port(void)
{
    return s_http_port_store;
}

esp_err_t root_handler(httpd_req_t *req)
{
    FILE *file = fopen("/spiffs/index.html", "r");
    if (file == NULL) {
        ESP_LOGE(WIFI_TAG, "Failed to open index.html");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        ESP_LOGE(WIFI_TAG, "Failed to allocate memory for HTML file");
        fclose(file);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, buffer, bytes_read);

    free(buffer);
    return ESP_OK;
}

esp_err_t wifi_config_handler(httpd_req_t *req)
{
    char content[512];
    int recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    if (recv_len <= 0) {
        return ESP_FAIL;
    }
    content[recv_len] = '\0';

    cJSON *root = cJSON_Parse(content);
    if (root == NULL) {
        return ESP_FAIL;
    }

    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(root, "password");
    cJSON *ws_url_json = cJSON_GetObjectItem(root, "ws_url");
    cJSON *http_ip_json = cJSON_GetObjectItem(root, "http_ip");
    cJSON *http_port_json = cJSON_GetObjectItem(root, "http_port");

    if (cJSON_IsString(ws_url_json)) {
        char ws_url[128];
        strncpy(ws_url, ws_url_json->valuestring, sizeof(ws_url) - 1);
        ws_url[sizeof(ws_url) - 1] = '\0';

        ESP_LOGI(WIFI_TAG, "Configuring WebSocket URL: %s", ws_url);
        if (save_ws_url(ws_url) == ESP_OK) {
            ESP_LOGI(WIFI_TAG, "WebSocket URL saved");
        } else {
            ESP_LOGE(WIFI_TAG, "Failed to set WebSocket URL");
        }
    }

    if (cJSON_IsString(http_ip_json)) {
        char http_ip[16];
        strncpy(http_ip, http_ip_json->valuestring, sizeof(http_ip) - 1);
        http_ip[sizeof(http_ip) - 1] = '\0';

#if defined(CONFIG_HTTP_PORT)
        uint16_t http_port = CONFIG_HTTP_PORT;
#else
        uint16_t http_port = 80;
#endif
        if (cJSON_IsNumber(http_port_json)) {
            http_port = (uint16_t)cJSON_GetNumberValue(http_port_json);
        }

        ESP_LOGI(WIFI_TAG, "Configuring HTTP: IP=%s, Port=%d", http_ip, http_port);
        if (save_http_config(http_ip, http_port) == ESP_OK) {
            ESP_LOGI(WIFI_TAG, "HTTP configuration saved successfully");
        } else {
            ESP_LOGE(WIFI_TAG, "Failed to save HTTP configuration");
        }
    } else if (cJSON_IsNumber(http_port_json)) {
        uint16_t http_port = (uint16_t)cJSON_GetNumberValue(http_port_json);
        const char *current_ip = get_http_ip();

        ESP_LOGI(WIFI_TAG, "Configuring HTTP Port: %d (IP: %s)", http_port, current_ip);
        if (save_http_config(current_ip, http_port) == ESP_OK) {
            ESP_LOGI(WIFI_TAG, "HTTP Port configuration saved successfully");
        } else {
            ESP_LOGE(WIFI_TAG, "Failed to save HTTP Port configuration");
        }
    }

    if (cJSON_IsString(ssid_json) && cJSON_IsString(password_json)) {
        char ssid[33];
        char password[65];
        strncpy(ssid, ssid_json->valuestring, sizeof(ssid) - 1);
        strncpy(password, password_json->valuestring, sizeof(password) - 1);
        ssid[sizeof(ssid) - 1] = '\0';
        password[sizeof(password) - 1] = '\0';

        ESP_LOGI(WIFI_TAG, "Configuring WiFi: SSID=%s", ssid);

        wifi_manager_ctx_t *ctx = wifi_manager_ctx();
        strncpy(ctx->pending_ssid, ssid, sizeof(ctx->pending_ssid) - 1);
        strncpy(ctx->pending_password, password, sizeof(ctx->pending_password) - 1);
        ctx->pending_ssid[sizeof(ctx->pending_ssid) - 1] = '\0';
        ctx->pending_password[sizeof(ctx->pending_password) - 1] = '\0';
        strncpy(ctx->configured_ssid, ssid, sizeof(ctx->configured_ssid) - 1);
        strncpy(ctx->configured_password, password, sizeof(ctx->configured_password) - 1);
        ctx->configured_ssid[sizeof(ctx->configured_ssid) - 1] = '\0';
        ctx->configured_password[sizeof(ctx->configured_password) - 1] = '\0';
        ctx->sta_configured = true;
        ctx->user_sta_configured = true;
        ctx->last_reconnect_attempt_tick = 0;
        ctx->connect_pending = true;

        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Attempting to connect...");

        char *response_str = cJSON_PrintUnformatted(response);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response_str, strlen(response_str));

        free(response_str);
        cJSON_Delete(response);
    } else if (ws_url_json != NULL || http_ip_json != NULL || http_port_json != NULL) {
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", true);

        char message[256] = {0};
        int msg_len = 0;
        bool has_config = false;

        if (ws_url_json != NULL) {
            strncpy(message + msg_len, "WebSocket URL saved. ", sizeof(message) - msg_len - 1);
            msg_len = strlen(message);
            has_config = true;
        }
        if (http_ip_json != NULL || http_port_json != NULL) {
            strncpy(message + msg_len, "HTTP configuration saved. ", sizeof(message) - msg_len - 1);
            msg_len = strlen(message);
            has_config = true;
        }

        if (has_config) {
            strncpy(message + msg_len, "Configuration will be applied on next connection.", sizeof(message) - msg_len - 1);
            cJSON_AddStringToObject(response, "message", message);
        } else {
            cJSON_AddStringToObject(response, "message", "Configuration saved.");
        }

        char *response_str = cJSON_PrintUnformatted(response);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response_str, strlen(response_str));

        free(response_str);
        cJSON_Delete(response);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t wifi_status_handler(httpd_req_t *req)
{
    wifi_manager_ctx_t *ctx = wifi_manager_ctx();
    const char *current_ws_url = get_ws_url();
    const char *current_http_ip = get_http_ip();
    uint16_t current_http_port = get_http_port();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ap_mode", ctx->ap_mode_active);
    cJSON_AddStringToObject(root, "ap_ssid", AP_SSID);
    cJSON_AddStringToObject(root, "ws_url", current_ws_url);
    cJSON_AddStringToObject(root, "http_ip", current_http_ip);
    cJSON_AddNumberToObject(root, "http_port", current_http_port);

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    wifi_manager_ctx_t *ctx = wifi_manager_ctx();
    if (ctx->mutex == NULL) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"wifi not ready\"}");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"wifi busy\"}");
        return ESP_FAIL;
    }

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        xSemaphoreGive(ctx->mutex);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"scan start failed\"}");
        return err;
    }

    uint16_t ap_num = 0;
    err = esp_wifi_scan_get_ap_num(&ap_num);
    if (err != ESP_OK) {
        xSemaphoreGive(ctx->mutex);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"scan count failed\"}");
        return err;
    }

    if (ap_num == 0) {
        xSemaphoreGive(ctx->mutex);
        cJSON *empty = cJSON_CreateObject();
        cJSON_AddBoolToObject(empty, "success", true);
        cJSON_AddArrayToObject(empty, "networks");
        char *json_str = cJSON_PrintUnformatted(empty);
        cJSON_Delete(empty);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_str, strlen(json_str));
        free(json_str);
        return ESP_OK;
    }

    wifi_ap_record_t *records = (wifi_ap_record_t *)calloc(ap_num, sizeof(wifi_ap_record_t));
    if (!records) {
        xSemaphoreGive(ctx->mutex);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"no memory\"}");
        return ESP_ERR_NO_MEM;
    }

    uint16_t ap_num_copy = ap_num;
    err = esp_wifi_scan_get_ap_records(&ap_num_copy, records);
    xSemaphoreGive(ctx->mutex);
    if (err != ESP_OK) {
        free(records);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"success\":false,\"message\":\"scan read failed\"}");
        return err;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON *arr = cJSON_AddArrayToObject(root, "networks");
    for (uint16_t i = 0; i < ap_num_copy; i++) {
        if (records[i].ssid[0] == '\0') {
            continue;
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (const char *)records[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", records[i].rssi);
        cJSON_AddNumberToObject(item, "auth", records[i].authmode);
        cJSON_AddItemToArray(arr, item);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(records);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ESP_OK;
}

esp_err_t redirect_handler(httpd_req_t *req)
{
    FILE *file = fopen("/spiffs/redirect.html", "r");
    if (file == NULL) {
        ESP_LOGE(WIFI_TAG, "Failed to open redirect.html");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        ESP_LOGE(WIFI_TAG, "Failed to allocate memory for redirect HTML file");
        fclose(file);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, buffer, bytes_read);

    free(buffer);
    return ESP_OK;
}

esp_err_t common_get_handler(httpd_req_t *req, httpd_err_code_t error)
{
    (void)error;
    ESP_LOGI(WIFI_TAG, "Handling URI: %s", req->uri);
    return captive_portal_handler(req);
}

esp_err_t captive_portal_handler(httpd_req_t *req)
{
    return root_handler(req);
}

esp_err_t start_webserver(void)
{
    wifi_manager_ctx_t *ctx = wifi_manager_ctx();
    if (ctx->server != NULL) {
        ESP_LOGW(WIFI_TAG, "Web server already running");
        return ESP_OK;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    /* Captive OS luôn gọi HTTP :80; không ràng buộc theo NVS http_port (dùng cho status UI). */
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 24;

    if (httpd_start(&ctx->server, &config) != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error starting web server");
        return ESP_FAIL;
    }

    httpd_uri_t root_uri = {.uri = "/",
                            .method = HTTP_GET,
                            .handler = root_handler,
                            .user_ctx = NULL};
    httpd_register_uri_handler(ctx->server, &root_uri);

    httpd_uri_t config_uri = {.uri = "/configure",
                              .method = HTTP_POST,
                              .handler = wifi_config_handler,
                              .user_ctx = NULL};
    httpd_register_uri_handler(ctx->server, &config_uri);

    httpd_uri_t status_uri = {.uri = "/status",
                              .method = HTTP_GET,
                              .handler = wifi_status_handler,
                              .user_ctx = NULL};
    httpd_register_uri_handler(ctx->server, &status_uri);

    httpd_uri_t scan_uri = {.uri = "/scan",
                            .method = HTTP_GET,
                            .handler = wifi_scan_handler,
                            .user_ctx = NULL};
    httpd_register_uri_handler(ctx->server, &scan_uri);

    static const char *const k_captive_probe_get[] = {
        "/generate_204",
        "/gen_204",
        "/connecttest.txt",
        "/ncsi.txt",
        "/success.txt",
        "/canonical.html",
    };
    for (size_t i = 0; i < sizeof(k_captive_probe_get) / sizeof(k_captive_probe_get[0]); i++) {
        httpd_uri_t probe = {.uri = k_captive_probe_get[i],
                             .method = HTTP_GET,
                             .handler = captive_portal_handler,
                             .user_ctx = NULL};
        httpd_register_uri_handler(ctx->server, &probe);
    }

    httpd_uri_t hotspot_detect = {.uri = "/hotspot-detect.html",
                                  .method = HTTP_GET,
                                  .handler = captive_portal_handler,
                                  .user_ctx = NULL};
    httpd_register_uri_handler(ctx->server, &hotspot_detect);

    httpd_register_err_handler(ctx->server, HTTPD_404_NOT_FOUND, common_get_handler);

    ESP_LOGI(WIFI_TAG, "Web server started on port %d", config.server_port);
    return ESP_OK;
}

esp_err_t stop_webserver(void)
{
    wifi_manager_ctx_t *ctx = wifi_manager_ctx();
    if (ctx->server) {
        httpd_stop(ctx->server);
        ctx->server = NULL;
        ESP_LOGI(WIFI_TAG, "Web server stopped");
    }
    if (ctx->dns_server_task_handle) {
        /* Stop DNS task gracefully so UDP :53 can be rebound next time. */
        ctx->dns_stop = true;
        if (ctx->dns_sock >= 0) {
            shutdown(ctx->dns_sock, SHUT_RDWR);
            close(ctx->dns_sock);
            ctx->dns_sock = -1;
        }
        for (int i = 0; i < 20 && ctx->dns_server_task_handle != NULL; i++) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        ESP_LOGI(WIFI_TAG, "DNS server task stopped");
    }
    return ESP_OK;
}
