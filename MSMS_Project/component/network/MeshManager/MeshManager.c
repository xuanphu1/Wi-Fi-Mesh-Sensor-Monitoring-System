#include "MeshManager.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>
#include "dhcpserver/dhcpserver.h"

static const char *TAG_MESH = "MeshManager";
static DataManager_t *s_data_manager = NULL;
static bool s_mesh_started = false;
static bool s_mesh_lite_initialized = false;
static bool s_mesh_lite_started_once = false;
static uint32_t s_mesh_udp_seq = 0;
static TimerHandle_t s_mesh_info_timer = NULL;
static TickType_t s_gateway_stats_tick = 0;
static uint32_t s_gateway_rx_frames = 0;
static uint32_t s_gateway_queued_frames = 0;
static uint32_t s_gateway_dropped_frames = 0;
static mesh_gateway_stats_t s_gateway_last_stats = {0};

typedef esp_err_t (*mesh_bridge_dhcps_change_cb_t)(esp_ip_addr_t *ip_info);
esp_err_t _esp_bridge_netif_list_add(esp_netif_t *netif,
                                     mesh_bridge_dhcps_change_cb_t dhcps_change_cb,
                                     const char *commit_id);
esp_err_t esp_bridge_netif_request_ip(esp_netif_ip_info_t *ip_info);

static void mesh_gateway_rx_queue_reset(DataManager_t *dm);
// static void mesh_log_error_array(DataManager_t *dm);

static void mesh_gateway_stats_reset(void)
{
    s_gateway_stats_tick = xTaskGetTickCount();
    s_gateway_rx_frames = 0;
    s_gateway_queued_frames = 0;
    s_gateway_dropped_frames = 0;
    memset(&s_gateway_last_stats, 0, sizeof(s_gateway_last_stats));
}

static void mesh_gateway_stats_note(DataManager_t *dm, bool queued)
{
    s_gateway_rx_frames++;
    if (queued) {
        s_gateway_queued_frames++;
    } else {
        s_gateway_dropped_frames++;
    }

    TickType_t now = xTaskGetTickCount();
    if ((now - s_gateway_stats_tick) < pdMS_TO_TICKS(1000)) {
        return;
    }

    s_gateway_last_stats.udp_rx_frames = s_gateway_rx_frames;
    s_gateway_last_stats.queued_frames = s_gateway_queued_frames;
    s_gateway_last_stats.dropped_frames = s_gateway_dropped_frames;

    UBaseType_t queue_used = 0;
    UBaseType_t queue_total = 0;
    if (dm != NULL && dm->meshIo.gateway_rx_queue != NULL) {
        queue_used = uxQueueMessagesWaiting(dm->meshIo.gateway_rx_queue);
        queue_total = queue_used + uxQueueSpacesAvailable(dm->meshIo.gateway_rx_queue);
    }

    ESP_LOGI(TAG_MESH,
             "Gateway RX: udp=%" PRIu32 ", queued=%" PRIu32 ", dropped=%" PRIu32 ", queue=%u/%u",
             s_gateway_last_stats.udp_rx_frames,
             s_gateway_last_stats.queued_frames,
             s_gateway_last_stats.dropped_frames,
             (unsigned)queue_used, (unsigned)queue_total);

    s_gateway_rx_frames = 0;
    s_gateway_queued_frames = 0;
    s_gateway_dropped_frames = 0;
    s_gateway_stats_tick = now;
}

static bool mesh_get_sta_ip_info(esp_netif_ip_info_t *out)
{
    if (out == NULL) {
        return false;
    }

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == NULL) {
        return false;
    }

    if (esp_netif_get_ip_info(sta_netif, out) != ESP_OK) {
        return false;
    }

    return out->ip.addr != 0;
}


/**
 * @brief Parse origin info in telemetry JSON: {"n":<level>, "i":"<ip>", ...}
 */
static void mesh_extract_origin_info(const uint8_t *payload, size_t payload_len,
                                     char *origin_ip, size_t origin_ip_cap,
                                     int *origin_lvl)
{
    if (origin_ip != NULL && origin_ip_cap > 0) {
        snprintf(origin_ip, origin_ip_cap, "unknown");
    }
    if (origin_lvl != NULL) {
        *origin_lvl = -1;
    }
    if (payload == NULL || payload_len == 0) {
        return;
    }

    char json[MESH_ROOT_UDP_FRAME_SIZE + 1];
    size_t copy_len = payload_len;
    if (copy_len > MESH_ROOT_UDP_FRAME_SIZE) {
        copy_len = MESH_ROOT_UDP_FRAME_SIZE;
    }
    memcpy(json, payload, copy_len);
    json[copy_len] = '\0';

    const char *ip_key = "\"i\":\"";
    char *ip_pos = strstr(json, ip_key);
    if (ip_pos != NULL && origin_ip != NULL && origin_ip_cap > 0) {
        ip_pos += strlen(ip_key);
        size_t i = 0;
        while (ip_pos[i] != '\0' && ip_pos[i] != '"' && i < (origin_ip_cap - 1)) {
            origin_ip[i] = ip_pos[i];
            i++;
        }
        origin_ip[i] = '\0';
    }

    const char *lvl_key = "\"n\":";
    char *lvl_pos = strstr(json, lvl_key);
    if (lvl_pos != NULL && origin_lvl != NULL) {
        lvl_pos += strlen(lvl_key);
        *origin_lvl = (int)strtol(lvl_pos, NULL, 10);
    }
}

// static void mesh_log_error_array(DataManager_t *dm)
// {
//     if (dm == NULL) {
//         return;
//     }
//     char err_buf[160];
//     int off = 0;
//     off += snprintf(err_buf + off, sizeof(err_buf) - (size_t)off, "[");
//     bool first = true;
//     for (size_t i = 0; i < DATA_MANAGER_ERROR_CAPACITY; i++) {
//         uint16_t code = dm->error_code[i];
//         if (code == 0U) {
//             continue;
//         }
//         int w = snprintf(err_buf + off, sizeof(err_buf) - (size_t)off,
//                          "%s0x%04X", first ? "" : ",", (unsigned)code);
//         if (w < 0 || (size_t)w >= sizeof(err_buf) - (size_t)off) {
//             break;
//         }
//         off += w;
//         first = false;
//     }
//     if (off < (int)sizeof(err_buf) - 2) {
//         snprintf(err_buf + off, sizeof(err_buf) - (size_t)off, "]");
//     } else {
//         err_buf[sizeof(err_buf) - 2] = ']';
//         err_buf[sizeof(err_buf) - 1] = '\0';
//     }
//     ESP_LOGI(TAG_MESH, "Telemetry err array: %s", err_buf);
// }

/** Khá»Ÿi táº¡o / xÃ³a tráº¡ng thÃ¡i mesh I/O (chá»‰ dÃ¹ng trong MeshManager). */
static void mesh_io_context_init(mesh_io_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->udp_tx_sock = -1;
    ctx->udp_rx_sock = -1;
}

/**
 * Trong mutex chá»‰ snap fd hiá»‡n táº¡i vÃ  gÃ¡n -1; caller gá»i close() bÃªn ngoÃ i.
 * out_tx/out_rx cÃ³ thá»ƒ NULL náº¿u khÃ´ng cáº§n.
 */
static void mesh_io_snap_invalidate_udp_sockets(DataManager_t *dm, int *out_tx,
                                                int *out_rx)
{
    if (out_tx != NULL) {
        *out_tx = -1;
    }
    if (out_rx != NULL) {
        *out_rx = -1;
    }
    if (dm == NULL || dm->meshIo.sock_mutex == NULL) {
        return;
    }
    if (xSemaphoreTake(dm->meshIo.sock_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return;
    }
    if (out_tx != NULL) {
        *out_tx = dm->meshIo.udp_tx_sock;
    }
    if (out_rx != NULL) {
        *out_rx = dm->meshIo.udp_rx_sock;
    }
    dm->meshIo.udp_tx_sock = -1;
    dm->meshIo.udp_rx_sock = -1;
    xSemaphoreGive(dm->meshIo.sock_mutex);
}

/**
 * Node: má»Ÿ UDP gá»­i tá»›i CONFIG_IP_ROOT. Root: bind UDP nháº­n.
 * Mutex chá»‰ dÃ¹ng Ä‘á»ƒ Ä‘á»c snap / xuáº¥t báº£n fd má»›i; socket/bind/setsockopt/close ngoÃ i mutex.
 */
static void mesh_io_ensure_udp_sockets(DataManager_t *dm)
{
    if (dm == NULL || dm->meshIo.sock_mutex == NULL) {
        return;
    }

    mesh_role_t role;
    int cur_tx;
    int cur_rx;
    if (xSemaphoreTake(dm->meshIo.sock_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return;
    }
    role = dm->meshIo.role;
    cur_tx = dm->meshIo.udp_tx_sock;
    cur_rx = dm->meshIo.udp_rx_sock;
    xSemaphoreGive(dm->meshIo.sock_mutex);

    if (role == MESH_ROLE_NODE) {
        if (cur_rx >= 0) {
            int rx_close = -1;
            if (xSemaphoreTake(dm->meshIo.sock_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                if (dm->meshIo.udp_rx_sock == cur_rx) {
                    rx_close = cur_rx;
                    dm->meshIo.udp_rx_sock = -1;
                }
                xSemaphoreGive(dm->meshIo.sock_mutex);
            }
            if (rx_close >= 0) {
                close(rx_close);
            }
        }
        if (cur_tx < 0) {
            int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
            if (s < 0) {
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                ESP_LOGE(TAG_MESH, "node UDP socket: errno %d", errno);
                return;
            }
            const uint32_t ip_be = inet_addr(CONFIG_IP_ROOT);
            const uint16_t port_host = (uint16_t)CONFIG_UDP_PORT;
            bool published = false;
            if (xSemaphoreTake(dm->meshIo.sock_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                if (dm->meshIo.role == MESH_ROLE_NODE && dm->meshIo.udp_tx_sock < 0) {
                    dm->meshIo.udp_tx_ip_be = ip_be;
                    dm->meshIo.udp_tx_port_host = port_host;
                    dm->meshIo.udp_tx_sock = s;
                    published = true;
                }
                xSemaphoreGive(dm->meshIo.sock_mutex);
            }
            if (!published) {
                close(s);
            }
        }
    } else {
        if (cur_tx >= 0) {
            int tx_close = -1;
            if (xSemaphoreTake(dm->meshIo.sock_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                if (dm->meshIo.udp_tx_sock == cur_tx) {
                    tx_close = cur_tx;
                    dm->meshIo.udp_tx_sock = -1;
                }
                xSemaphoreGive(dm->meshIo.sock_mutex);
            }
            if (tx_close >= 0) {
                close(tx_close);
            }
        }
        if (cur_rx < 0) {
            int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
            if (s < 0) {
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                ESP_LOGE(TAG_MESH, "root UDP socket: errno %d", errno);
                return;
            }
            struct sockaddr_in listen_addr;
            memset(&listen_addr, 0, sizeof(listen_addr));
            listen_addr.sin_family = AF_INET;
            listen_addr.sin_port = htons(CONFIG_UDP_PORT);
            listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(s, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                ESP_LOGE(TAG_MESH, "root bind: errno %d", errno);
                close(s);
                return;
            }
            struct timeval tv = {.tv_sec = 0, .tv_usec = 300000};
            if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                ESP_LOGE(TAG_MESH, "root SO_RCVTIMEO: errno %d", errno);
                close(s);
                return;
            }
            bool published = false;
            if (xSemaphoreTake(dm->meshIo.sock_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                if (dm->meshIo.role == MESH_ROLE_ROOT && dm->meshIo.udp_rx_sock < 0) {
                    dm->meshIo.udp_rx_sock = s;
                    published = true;
                }
                xSemaphoreGive(dm->meshIo.sock_mutex);
            }
            if (!published) {
                close(s);
            }
        }
    }
}

static void mesh_io_runtime_shutdown(DataManager_t *dm)
{
    if (dm == NULL) {
        return;
    }
    int tx = -1;
    int rx = -1;
    mesh_io_snap_invalidate_udp_sockets(dm, &tx, &rx);
    if (tx >= 0) {
        close(tx);
    }
    if (rx >= 0) {
        close(rx);
    }
    if (dm->meshIo.sock_mutex != NULL) {
        vSemaphoreDelete(dm->meshIo.sock_mutex);
        dm->meshIo.sock_mutex = NULL;
    }
    mesh_gateway_rx_queue_reset(dm);
    mesh_io_context_init(&dm->meshIo);
}

/**
 * @file MeshManager.c
 * @brief Mesh-lite child: bridge Wiâ€‘Fi, UDP JSON uplink, timer logs.
 */

/**
 * @brief True when esp_mesh_lite_get_level() reports an attached level.
 *
 * @return true if mesh level > 0, false otherwise.
 */
bool MeshManager_IsConnected(void) {
    return esp_mesh_lite_get_level() > 0;
}

bool MeshManager_IsStarted(void) {
    return s_mesh_started;
}

uint8_t MeshManager_GetConnectedNodeCount(void)
{
    wifi_sta_list_t sta_list = {0};
    esp_err_t ret = esp_wifi_ap_get_sta_list(&sta_list);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG_MESH, "esp_wifi_ap_get_sta_list failed: %s",
                 esp_err_to_name(ret));
        return 0;
    }
    return sta_list.num;
}

void MeshManager_GetGatewayStats(mesh_gateway_stats_t *out_stats)
{
    if (out_stats == NULL) {
        return;
    }
    *out_stats = s_gateway_last_stats;
}

/**
 * @brief Gộp dữ liệu cảm biến thành JSON gọn (schema MESH_UDP_JSON_SCHEMA).
 *
 * ---------------------------------------------------------------------------
 * ĐỊNH DẠNG (một object JSON, UTF-8, compact):
 *   {"v":<schema>,"seq":<counter>,"n":<mesh_level>,"i":"<sta_ipv4>","t":"<rtc_iso>","ver":"<x.x.x>","err":["0xNN",...],"p":[[port,type,v0,v1,...], ...]}
 * ---------------------------------------------------------------------------
 *
 * Thứ tự key giữ cố định: v -> seq -> n -> i -> t -> ver -> err -> p.
 *
 * v - version schema (int)
 *   - Phía nhận (root/server) đọc trước để chọn parser.
 *   - Giá trị = MESH_UDP_JSON_SCHEMA. Đổi cấu trúc thì tăng v.
 *
 * seq - packet sequence (uint32)
 *   - Tăng 1 cho mỗi gói UDP telemetry được build, bắt đầu từ 1 sau boot.
 *   - Root/server dùng seq theo từng node để tính packet loss.
 *
 * n - mesh level / node level (int)
 *   - Lấy từ esp_mesh_lite_get_level() tại thời điểm build gói.
 *   - Dùng phân biệt vị trí trong mesh (root/child khác nhau theo stack mesh-lite).
 *
 * i - IPv4 STA (string, dotted decimal)
 *   - IP mesh do parent cấp, đọc từ netif STA (WIFI_STA_DEF) tại thời điểm gửi.
 *   - Khớp định danh với header UDP / gateway (root có thể đối chiếu src_ip với i).
 *   - Chưa có IP (0.0.0.0) khi netif chưa sẵn sàng.
 *
 * t - Local RTC/System time (string, ISO 8601)
 *   - Định dạng: YYYY-MM-DDTHH:MM:SS.
 *   - Lấy từ time()/localtime_r(). Nếu chưa có thời gian hợp lệ thì giữ mặc định
 *     "1970-01-01T00:00:00".
 *
 * ver - Firmware version (string)
 *   - Lấy từ DataManager.version[3] theo format "major.minor.patch".
 *
 * err - Mảng lỗi runtime (array of string, hex 8-bit)
 *   - Định dạng mỗi phần tử: "0xNN".
 *   - Lấy từ DataManager.error_code[10].
 *   - MRS_OK không lưu vào mảng lỗi.
 *
 * p - ports: mảng các hàng đo (array of array)
 *   - Mỗi phần tử là một hàng: [port, type, v0, v1, ...].
 *   - Chỉ có port đã gán sensor (!= SENSOR_NONE) và read() trả MRS_OK.
 *   - Port không dùng hoặc lỗi read không xuất hiện (không có hàng rỗng).
 *
 * Một hàng [port, type, v0, v1, ...]:
 *
 *   port (int, cột 0) - ID cổng trên wire, 1-based:
 *     1 = Port 1, 2 = Port 2, 3 = Port 3
 *     (trong code: PortId_t PORT_1=0 .. PORT_3=2 -> wire = index + 1)
 *
 *   type (int, cột 1) - SensorType_t từ SensorTypes.h (ID loại cảm biến):
 *     0  = SENSOR_BME280
 *     1  = SENSOR_MHZ14A
 *     2  = SENSOR_PMS7003
 *     3  = SENSOR_DHT22
 *     4  = SENSOR_AHT10
 *     (SENSOR_NONE = -1 không gửi lên wire)
 *   Phía nhận dùng sensor_registry / sensor_type_to_name() hoặc bảng tra cố định
 *   trùng firmware để suy ra tên + số field + ý nghĩa từng v[i].
 *
 *   v0, v1, ... (float, từ cột 2) - giá trị đo
 *     - Thứ tự đúng với driver->read() -> SensorData_t.data_fl[i].
 *     - i chạy từ 0 đến min(driver->unit_count, 5) - 1 (tối đa 5 số / hàng).
 *     - In ra với độ chính xác %.3f.
 *     - Không gửi tên field trên wire; thứ tự khớp driver->description[i]
 *       của đúng type (xem SensorRegistry).
 *
 * Ví dụ:
 *   {"v":1,"seq":1,"n":2,"i":"192.168.5.2","t":"2026-04-26T10:20:00","ver":"1.2.3","err":["0x02","0x01"],"p":[[1,0,25.300,60.200,1013.250],[3,13,28.100,55.000]]}
 *   -> schema 1; seq 1; level 2; IP STA + thời gian; Port1 BME280; Port3 AHT10.
 *
 * @param buf Buffer đích (NUL-terminated).
 * @param cap Kích thước buf.
 * @param dm DataManager (NULL -> "p":[]).
 * @return Độ dài payload (strlen), hoặc -1 nếu buf/cap không hợp lệ hoặc ghi lỗi.
 */
static int mesh_build_udp_telemetry_json(char *buf, size_t cap, DataManager_t *dm)
{
    if (buf == NULL || cap < 64) {
        return -1;
    }

    char ip_str[16] = "0.0.0.0";
    char time_iso[20] = "1970-01-01T00:00:00";
    char ver_str[16] = "0.0.0";
    if (dm != NULL) {
        snprintf(ver_str, sizeof(ver_str), "%u.%u.%u",
                 (unsigned)dm->version[0],
                 (unsigned)dm->version[1],
                 (unsigned)dm->version[2]);
        ESP_LOGI(TAG_MESH, "Telemetry version: %s", ver_str);
        // mesh_log_error_array(dm);
    }
    esp_netif_ip_info_t ip_info;
    if (mesh_get_sta_ip_info(&ip_info)) {
        struct in_addr ia = {.s_addr = ip_info.ip.addr};
        snprintf(ip_str, sizeof(ip_str), "%s", inet_ntoa(ia));
    }
    time_t now = time(NULL);
    struct tm tm_local;
    if (localtime_r(&now, &tm_local) != NULL) {
        if (strftime(time_iso, sizeof(time_iso), "%Y-%m-%dT%H:%M:%S", &tm_local) == 0) {
            snprintf(time_iso, sizeof(time_iso), "%s", "1970-01-01T00:00:00");
        }
    }

    const uint32_t seq = ++s_mesh_udp_seq;
    int offset = 0;
    offset += snprintf(buf + offset, cap - (size_t)offset,
                       "{\"v\":%d,\"seq\":%" PRIu32 ",\"n\":%d,\"i\":\"%s\",\"t\":\"%s\",\"ver\":\"%s\",\"err\":[",
                       MESH_UDP_JSON_SCHEMA, seq, esp_mesh_lite_get_level(), ip_str,
                       time_iso, ver_str);

    if (offset < 0 || (size_t)offset >= cap) {
        buf[cap - 1] = '\0';
        return -1;
    }

    bool first_err = true;
    if (dm != NULL) {
        for (size_t i = 0; i < DATA_MANAGER_ERROR_CAPACITY; i++) {
            uint16_t code = dm->error_code[i];
            if (code == 0U) {
                continue;
            }
            size_t rem = cap - (size_t)offset;
            int w = snprintf(buf + offset, rem, "%s\"0x%02X\"",
                             first_err ? "" : ",", (unsigned)(code & 0xFFU));
            if (w < 0 || (size_t)w >= rem) {
                ESP_LOGW(TAG_MESH, "JSON payload truncated (err array)");
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                goto mesh_udp_json_close;
            }
            offset += w;
            first_err = false;
        }
    }
    {
        size_t rem = cap - (size_t)offset;
        int w = snprintf(buf + offset, rem, "],\"p\":[");
        if (w < 0 || (size_t)w >= rem) {
            ESP_LOGW(TAG_MESH, "JSON payload truncated (err->p)");
            if (dm != NULL) {
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
            }
            goto mesh_udp_json_close;
        }
        offset += w;
    }

    bool first_row = true;
    if (dm != NULL) {
        for (PortId_t port = 0; port < NUM_PORTS; port++) {
            SensorType_t type = dm->selectedSensor[port];
            if (type == SENSOR_NONE) {
                continue;
            }

            sensor_driver_t *driver = sensor_registry_get_driver(type);
            if (driver == NULL || driver->read == NULL) {
                ESP_LOGW(TAG_MESH, "Port %d: invalid driver for sensor %d", port, type);
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_ERR_CORE_INVALID_PARAM);
                continue;
            }

            SensorData_t data;
            system_err_t ret = driver->read(&data);
            if (ret != MRS_OK) {
                ESP_LOGW(TAG_MESH, "Port %d: read failed: %s",
                         port, system_err_to_name(ret));
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, ret);
                continue;
            }

            const int saved = offset;

            if (!first_row) {
                if (offset >= (int)cap - 2) {
                    ESP_LOGW(TAG_MESH, "JSON payload truncated");
                    ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                    goto mesh_udp_json_close;
                }
                buf[offset++] = ',';
                buf[offset] = '\0';
            }
            first_row = false;

            size_t rem = cap - (size_t)offset;
            int w = snprintf(buf + offset, rem, "[%d,%d", (int)(port + 1), (int)type);
            if (w < 0 || (size_t)w >= rem) {
                offset = saved;
                ESP_LOGW(TAG_MESH, "JSON payload truncated");
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                goto mesh_udp_json_close;
            }
            offset += w;

            uint8_t n = driver->unit_count;
            if (n > 5) {
                n = 5;
            }
            for (uint8_t i = 0; i < n; i++) {
                rem = cap - (size_t)offset;
                w = snprintf(buf + offset, rem, ",%.3f", data.data_fl[i]);
                if (w < 0 || (size_t)w >= rem) {
                    offset = saved;
                    ESP_LOGW(TAG_MESH, "JSON payload truncated");
                    ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                    goto mesh_udp_json_close;
                }
                offset += w;
            }

            if (offset >= (int)cap - 1) {
                offset = saved;
                ESP_LOGW(TAG_MESH, "JSON payload truncated");
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                goto mesh_udp_json_close;
            }
            buf[offset++] = ']';
            buf[offset] = '\0';
        }
    }

mesh_udp_json_close:
    if (offset < (int)cap - 2) {
        snprintf(buf + offset, cap - (size_t)offset, "]}");
    } else {
        buf[cap - 2] = '}';
        buf[cap - 1] = '\0';
    }

    return (int)strlen(buf);
}

static void mesh_gateway_rx_queue_reset(DataManager_t *dm)
{
    if (dm == NULL) {
        return;
    }
    if (dm->meshIo.gateway_rx_queue != NULL) {
        vQueueDelete(dm->meshIo.gateway_rx_queue);
        dm->meshIo.gateway_rx_queue = NULL;
    }
}

/**
 * @brief Mesh link + cáº¥u hÃ¬nh UDP: tráº¡ng thÃ¡i káº¿t ná»‘i, esp_mesh_lite_connect khi rá»›t,
 *        má»Ÿ/Ä‘Ã³ng socket node (tx) hoáº·c root (rx+bind) theo vai trÃ².
 *
 * @param pvParameters DataManager_t *.
 */
static void mesh_connection_task(void *pvParameters)
{
    DataManager_t *dm = (DataManager_t *)pvParameters;

    for (;;) {
        if (dm == NULL) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        bool up = MeshManager_IsConnected();
        dm->objectInfo.meshInfo.meshStatus = up ? CONNECTED : DISCONNECTED;
        dm->meshIo.link_up = up;

        if (!up) {
            int tx = -1;
            int rx = -1;
            mesh_io_snap_invalidate_udp_sockets(dm, &tx, &rx);
            if (tx >= 0) {
                close(tx);
            }
            if (rx >= 0) {
                close(rx);
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        mesh_io_ensure_udp_sockets(dm);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * @brief Chá»‰ truyá»n dá»¯ liá»‡u: node sendto telemetry; root recvfrom â†’ gateway_rx_queue.
 *        Socket Ä‘Ã£ Ä‘Æ°á»£c mesh_connection_task má»Ÿ vÃ  cáº¥u hÃ¬nh.
 *
 * @param pvParameters DataManager_t *.
 */
static void mesh_data_task(void *pvParameters)
{
    DataManager_t *dm = (DataManager_t *)pvParameters;

    for (;;) {
        if (dm == NULL) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (dm->meshIo.role == MESH_ROLE_NODE) {
            if (!dm->meshIo.link_up) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }

            if (esp_mesh_lite_get_level() <= 1) {
                ESP_LOGW(TAG_MESH, "Mesh is not joined as child yet (layer=%d), skip UDP telemetry",
                         esp_mesh_lite_get_level());
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }

#if CONFIG_MESH_LITE_NODE_INFO_REPORT
            uint32_t size = 0;
            const node_info_list_t *node = esp_mesh_lite_get_nodes_list(&size);
            const node_info_list_t *target = NULL;
            for (uint32_t loop = 0; (loop < size) && (node != NULL); loop++) {
                if (node->node->level == 1) {
                    target = node;
                    break;
                }
                node = node->next;
            }
            if (!target) {
                ESP_LOGW(TAG_MESH, "No root node found in mesh list, retry...");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
#endif
            int sock = -1;
            struct sockaddr_in dest_addr;
            memset(&dest_addr, 0, sizeof(dest_addr));
            if (dm->meshIo.sock_mutex != NULL &&
                xSemaphoreTake(dm->meshIo.sock_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                sock = dm->meshIo.udp_tx_sock;
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = htons(dm->meshIo.udp_tx_port_host);
                dest_addr.sin_addr.s_addr = dm->meshIo.udp_tx_ip_be;
                xSemaphoreGive(dm->meshIo.sock_mutex);
            }
            if (sock < 0) {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }

            char payload[512];
            int plen = mesh_build_udp_telemetry_json(payload, sizeof(payload), dm);
            if (plen < 0) {
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                ESP_LOGE(TAG_MESH, "Failed to build UDP telemetry JSON");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }

            ESP_LOGI(TAG_MESH, "UDP telemetry %d bytes -> %s:%d (schema v%d)",
                     plen, CONFIG_IP_ROOT, CONFIG_UDP_PORT, MESH_UDP_JSON_SCHEMA);
            ESP_LOGI(TAG_MESH, "payload: %s", payload);

            int err = sendto(sock, payload, (size_t)plen, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0) {
                ErrorCodes_PushError(dm->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_FAIL);
                ESP_LOGE(TAG_MESH, "Error occurred during sending: errno %d", errno);
            } else {
                ESP_LOGI(TAG_MESH, "Message sent successfully (%d bytes)", err);
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        int sock = -1;
        if (dm->meshIo.sock_mutex != NULL &&
            xSemaphoreTake(dm->meshIo.sock_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            sock = dm->meshIo.udp_rx_sock;
            xSemaphoreGive(dm->meshIo.sock_mutex);
        }
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        while (dm->meshIo.role == MESH_ROLE_ROOT) {
            uint8_t rx_buf[MESH_ROOT_UDP_FRAME_SIZE];
            struct sockaddr_in source_addr;
            socklen_t src_len = sizeof(source_addr);
            int recv_len = recvfrom(sock, rx_buf, sizeof(rx_buf), 0,
                                    (struct sockaddr *)&source_addr, &src_len);
            if (recv_len > 0 && dm->meshIo.gateway_rx_queue != NULL) {
                mesh_gateway_rx_msg_t msg = {0};
                msg.src_ip = source_addr.sin_addr.s_addr;
                if (recv_len > (int)sizeof(msg.data)) {
                    recv_len = (int)sizeof(msg.data);
                }
                msg.len = (uint16_t)recv_len;
                memcpy(msg.data, rx_buf, (size_t)recv_len);
                char origin_ip[16];
                int origin_lvl = -1;
                mesh_extract_origin_info(msg.data, msg.len, origin_ip,
                                         sizeof(origin_ip), &origin_lvl);
                ESP_LOGD(TAG_MESH,
                         "Mesh UDP RX: src_hop_ip=%s, origin_ip=%s, origin_lvl=%d, %d bytes",
                         inet_ntoa(source_addr.sin_addr), origin_ip, origin_lvl, recv_len);
                ESP_LOGD(TAG_MESH, "payload mesh RX: %.*s",
                         recv_len, (char *)msg.data);
                bool queued = xQueueSend(dm->meshIo.gateway_rx_queue, &msg, 0) == pdPASS;
                mesh_gateway_stats_note(dm, queued);
                if (!queued) {
                    ESP_LOGW(TAG_MESH,
                             "gateway_rx_queue full, drop UDP frame (%d bytes)",
                             recv_len);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * @brief Log Ä‘á»‹nh ká»³: kÃªnh WiFi, layer mesh, MAC STA, BSSID/RSSI parent, heap, STA con cá»§a AP, danh sÃ¡ch node (náº¿u báº­t report).
 *
 * @note Cháº¡y trong task FreeRTOS Timer Service; náº¿u panic/stack Tmr Svc, tÄƒng
 *       CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH trong menuconfig.
 */
static void mesh_print_system_info_timercb(TimerHandle_t timer)
{
    (void)timer;

    uint8_t primary = 0;
    uint8_t sta_mac[6] = {0};
    wifi_ap_record_t ap_info = {0};
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    wifi_sta_list_t wifi_sta_list = {0};

    if (esp_mesh_lite_get_level() > 1) {
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            memset(&ap_info, 0, sizeof(ap_info));
        }
    }

    if (esp_wifi_get_mac(WIFI_IF_STA, sta_mac) != ESP_OK) {
        memset(sta_mac, 0, sizeof(sta_mac));
    }

    if (esp_wifi_ap_get_sta_list(&wifi_sta_list) != ESP_OK) {
        memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
    }

    if (esp_wifi_get_channel(&primary, &second) != ESP_OK) {
        primary = 0;
    }

    ESP_LOGI(TAG_MESH,
             "System info: ch=%u, layer=%d, self mac: " MACSTR ", parent bssid: " MACSTR
             ", parent rssi: %d, free heap: %" PRIu32,
             (unsigned)primary, esp_mesh_lite_get_level(), MAC2STR(sta_mac),
             MAC2STR(ap_info.bssid),
             (ap_info.rssi != 0 ? ap_info.rssi : -120), esp_get_free_heap_size());

    for (int i = 0; i < wifi_sta_list.num; i++) {
        ESP_LOGI(TAG_MESH, "Child mac: " MACSTR, MAC2STR(wifi_sta_list.sta[i].mac));
    }

#if CONFIG_MESH_LITE_NODE_INFO_REPORT
    uint32_t size = 0;
    const node_info_list_t *node = esp_mesh_lite_get_nodes_list(&size);
    ESP_LOGI(TAG_MESH, "MeshLite nodes %" PRIu32 ":", size);
    for (uint32_t loop = 0; (loop < size) && (node != NULL); loop++) {
        struct in_addr ip_struct;
        ip_struct.s_addr = node->node->ip_addr;
        ESP_LOGI(TAG_MESH, "%" PRIu32 ": lvl=%d, " MACSTR ", %s", loop + 1U,
                 (int)node->node->level, MAC2STR(node->node->mac_addr),
                 inet_ntoa(ip_struct));
        node = node->next;
    }
#else
    ESP_LOGD(TAG_MESH,
             "Mesh node list skipped (set CONFIG_MESH_LITE_NODE_INFO_REPORT=y for list)");
#endif
}

/**
 * @brief Apply empty STA cfg and Kconfig SoftAP parameters without bridge deauth hooks.
 */
static void mesh_wifi_init(void)
{
    // Station
    wifi_config_t wifi_config;
    memset(&wifi_config, 0x0, sizeof(wifi_config_t));
    wifi_config.sta.pmf_cfg.capable = false;
    wifi_config.sta.pmf_cfg.required = false;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_MESH, "esp_wifi_set_config(STA) failed: %s", esp_err_to_name(err));
    }

    // SoftAP
    wifi_config_t wifi_softap_config = {
        .ap = {
            .ssid     = CONFIG_BRIDGE_SOFTAP_SSID,
            .password = CONFIG_BRIDGE_SOFTAP_PASSWORD,
            .channel  = 11,
        },
    };
#ifdef CONFIG_BRIDGE_SOFTAP_SSID_END_WITH_THE_MAC
    uint8_t softap_mac[6] = {0};
    char suffix[8] = {0};
    char temp_ssid[sizeof(wifi_softap_config.ap.ssid) + 1] = {0};
    esp_wifi_get_mac(WIFI_IF_AP, softap_mac);
    memcpy(temp_ssid, wifi_softap_config.ap.ssid, sizeof(wifi_softap_config.ap.ssid));
    snprintf(suffix, sizeof(suffix), "_%02x%02x%02x", softap_mac[3], softap_mac[4], softap_mac[5]);
    if ((strlen(temp_ssid) + strlen(suffix)) <= 32) {
        size_t base_len = strlen(temp_ssid);
        snprintf(temp_ssid + base_len, sizeof(temp_ssid) - base_len, "%s", suffix);
        memset(wifi_softap_config.ap.ssid, 0, sizeof(wifi_softap_config.ap.ssid));
        memcpy(wifi_softap_config.ap.ssid, temp_ssid, strlen(temp_ssid));
    } else {
        ESP_LOGW(TAG_MESH, "Mesh SoftAP SSID too long, skip MAC suffix");
    }
#endif
    wifi_softap_config.ap.max_connection = CONFIG_BRIDGE_SOFTAP_MAX_CONNECT_NUMBER;
    wifi_softap_config.ap.authmode =
        strlen((char *)wifi_softap_config.ap.password) < 8 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_softap_config.ap.pmf_cfg.capable = false;
    wifi_softap_config.ap.pmf_cfg.required = false;
    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_softap_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_MESH, "esp_wifi_set_config(AP) failed: %s", esp_err_to_name(err));
    }
}

static esp_err_t mesh_wifi_start(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_MESH, "esp_wifi_get_mode failed before start: %s", esp_err_to_name(err));
        return err;
    }

    if (mode == WIFI_MODE_NULL) {
        err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (err != ESP_OK) {
            ESP_LOGW(TAG_MESH, "esp_wifi_set_mode(APSTA) failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    err = esp_wifi_start();
    if (err == ESP_OK || err == ESP_ERR_WIFI_STATE) {
        ESP_LOGI(TAG_MESH, "Mesh WiFi started");
        return ESP_OK;
    }

    ESP_LOGW(TAG_MESH, "esp_wifi_start failed: %s", esp_err_to_name(err));
    return err;
}

static esp_err_t mesh_bridge_add_netif_no_callback(esp_netif_t *netif)
{
    esp_err_t err = _esp_bridge_netif_list_add(netif, NULL, "mrs-no-deauth");
    if (err == ESP_ERR_DUPLICATE_ADDITION) {
        return ESP_OK;
    }
    return err;
}

static esp_err_t mesh_set_softap_ip_no_napt(esp_netif_t *netif,
                                            const esp_netif_ip_info_t *ip_info)
{
    if (netif == NULL || ip_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_netif_dhcp_status_t state;
    esp_err_t err = esp_netif_dhcps_get_status(netif, &state);
    if (err != ESP_OK) {
        return err;
    }

    if (state != ESP_NETIF_DHCP_STOPPED) {
        err = esp_netif_dhcps_stop(netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            return err;
        }
    }

    err = esp_netif_set_ip_info(netif, ip_info);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG_MESH, "Mesh SoftAP IP set: " IPSTR, IP2STR(&ip_info->ip));

    esp_netif_dns_info_t dns_info = {0};
    dns_info.ip.type = IPADDR_TYPE_V4;
    dns_info.ip.u_addr.ip4.addr = ip_info->ip.addr;
    err = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_MESH, "esp_netif_set_dns_info failed: %s", esp_err_to_name(err));
    }

    err = esp_netif_dhcps_start(netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        return err;
    }

    return ESP_OK;
}

static esp_err_t mesh_bridge_create_softap_netif_no_deauth(void)
{
    esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (wifi_netif == NULL) {
        wifi_netif = esp_netif_create_default_wifi_ap();
    }
    if (wifi_netif == NULL) {
        return ESP_FAIL;
    }

    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    esp_err_t err = esp_netif_dhcps_option(wifi_netif, ESP_NETIF_OP_SET,
                                           ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &dhcps_dns_value, sizeof(dhcps_dns_value));
    if (err != ESP_OK) {
        ESP_LOGW(TAG_MESH, "esp_netif_dhcps_option failed: %s", esp_err_to_name(err));
    }

    wifi_mode_t mode = WIFI_MODE_NULL;
    err = esp_wifi_get_mode(&mode);
    if (err == ESP_OK && mode != WIFI_MODE_AP && mode != WIFI_MODE_APSTA) {
        err = esp_wifi_set_mode(mode | WIFI_MODE_AP);
        if (err != ESP_OK) {
            return err;
        }
    }

    err = mesh_bridge_add_netif_no_callback(wifi_netif);
    if (err != ESP_OK) {
        return err;
    }

#if defined(CONFIG_BRIDGE_WIFI_PMF_DISABLE)
    esp_wifi_disable_pmf_config(WIFI_IF_AP);
#endif

    esp_netif_ip_info_t allocate_ip_info;
    memset(&allocate_ip_info, 0, sizeof(allocate_ip_info));
    err = esp_bridge_netif_request_ip(&allocate_ip_info);
    if (err != ESP_OK) {
        return err;
    }

    return mesh_set_softap_ip_no_napt(wifi_netif, &allocate_ip_info);
}

static esp_err_t mesh_bridge_create_station_netif_no_deauth(void)
{
    esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (wifi_netif == NULL) {
        wifi_netif = esp_netif_create_default_wifi_sta();
    }
    if (wifi_netif == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = mesh_bridge_add_netif_no_callback(wifi_netif);
    if (err != ESP_OK) {
        return err;
    }

    wifi_mode_t mode = WIFI_MODE_NULL;
    err = esp_wifi_get_mode(&mode);
    if (err == ESP_OK && mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA) {
        err = esp_wifi_set_mode(mode | WIFI_MODE_STA);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static void mesh_bridge_create_netifs_no_deauth(void)
{
    ESP_LOGI(TAG_MESH, "Creating bridge netifs without SoftAP deauth callback");
    esp_err_t err = ESP_OK;

#if defined(CONFIG_BRIDGE_DATA_FORWARDING_NETIF_SOFTAP)
    err = mesh_bridge_create_softap_netif_no_deauth();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_MESH, "Create mesh SoftAP bridge netif failed: %s", esp_err_to_name(err));
    }
#endif

#if defined(CONFIG_BRIDGE_EXTERNAL_NETIF_STATION)
    err = mesh_bridge_create_station_netif_no_deauth();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_MESH, "Create mesh STA bridge netif failed: %s", esp_err_to_name(err));
    }
#if defined(CONFIG_BRIDGE_WIFI_PMF_DISABLE)
    esp_wifi_disable_pmf_config(WIFI_IF_STA);
#endif
#endif
}

/**
 * @brief Load SoftAP SSID/password from NVS or MAC-suffixed defaults, push to mesh-lite.
 */
static void mesh_app_wifi_set_softap_info(void)
{
    char softap_ssid[33];
    char softap_psw[64];
    uint8_t softap_mac[6];
    size_t ssid_size = sizeof(softap_ssid);
    size_t psw_size = sizeof(softap_psw);
    esp_wifi_get_mac(WIFI_IF_AP, softap_mac);
    memset(softap_ssid, 0x0, sizeof(softap_ssid));
    memset(softap_psw, 0x0, sizeof(softap_psw));

    if (esp_mesh_lite_get_softap_ssid_from_nvs(softap_ssid, &ssid_size) == ESP_OK) {
        ESP_LOGI(TAG_MESH, "Get ssid from nvs: %s", softap_ssid);
    } else {
#ifdef CONFIG_BRIDGE_SOFTAP_SSID_END_WITH_THE_MAC
        snprintf(softap_ssid, sizeof(softap_ssid), "%.25s_%02x%02x%02x",
                 CONFIG_BRIDGE_SOFTAP_SSID, softap_mac[3], softap_mac[4], softap_mac[5]);
#else
        snprintf(softap_ssid, sizeof(softap_ssid), "%.32s", CONFIG_BRIDGE_SOFTAP_SSID);
#endif
        ESP_LOGI(TAG_MESH, "Get ssid from nvs failed, set ssid: %s", softap_ssid);
    }

    if (esp_mesh_lite_get_softap_psw_from_nvs(softap_psw, &psw_size) == ESP_OK) {
        ESP_LOGI(TAG_MESH, "Get psw from nvs: [HIDDEN]");
    } else {
        strlcpy(softap_psw, CONFIG_BRIDGE_SOFTAP_PASSWORD, sizeof(softap_psw));
        ESP_LOGI(TAG_MESH, "Get psw from nvs failed, set psw: [HIDDEN]");
    }

    esp_mesh_lite_set_softap_info(softap_ssid, softap_psw);
}

/**
 * @brief One-shot bootstrap: netif, mesh_lite init/start, UDP task, debug timer.
 *
 * @param data Application state; stores mesh client task handle in TaskHandle_Array.
 */
void MeshManager_ResetState(void)
{
    if (s_mesh_started) {
        ESP_LOGI(TAG_MESH, "Skip esp_mesh_lite_disconnect to keep reconnect available");
    }
    if (s_mesh_info_timer != NULL) {
        xTimerStop(s_mesh_info_timer, 0);
        xTimerDelete(s_mesh_info_timer, 0);
        s_mesh_info_timer = NULL;
    }
    if (s_data_manager != NULL) {
        mesh_io_runtime_shutdown(s_data_manager);
    }
    mesh_gateway_stats_reset();
    s_mesh_started = false;
    s_data_manager = NULL;
}

bool MeshManager_SwitchRole(mesh_role_t role)
{
    if (!s_mesh_started) {
        ESP_LOGW(TAG_MESH, "SwitchRole ignored: mesh not started");
        return false;
    }
    if (s_data_manager->meshIo.role == role) {
        ESP_LOGI(TAG_MESH, "SwitchRole: already %s", role == MESH_ROLE_ROOT ? "root" : "node");
        return true;
    }

    if (role == MESH_ROLE_ROOT) {
        /* mesh_data_task chuyá»ƒn sang nhÃ¡nh root (Ä‘Ã³ng socket node / má»Ÿ bind). */
        s_data_manager->meshIo.role = MESH_ROLE_ROOT;
        ESP_LOGI(TAG_MESH, "SwitchRole in-place: NODE -> ROOT");
        mesh_wifi_start();
        esp_mesh_lite_set_allowed_level(1);
        /* Best effort: detach from previous parent if currently a child. */
        esp_err_t err = esp_wifi_disconnect();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT &&
            err != ESP_ERR_WIFI_NOT_STARTED) {
            ESP_LOGW(TAG_MESH, "esp_wifi_disconnect failed: %s", esp_err_to_name(err));
        }
        return true;
    }

    s_data_manager->meshIo.role = MESH_ROLE_NODE;
    ESP_LOGI(TAG_MESH, "SwitchRole in-place: ROOT -> NODE");
    mesh_wifi_start();
    esp_mesh_lite_set_disallowed_level(1);
    return true;
}

void MeshManager_StartMesh(DataManager_t *data, mesh_role_t mesh_role)
{
    if (s_mesh_started) {
        ESP_LOGI(TAG_MESH, "Mesh client already started");
        return;
    }

    s_data_manager = data;
    mesh_gateway_stats_reset();
    mesh_io_context_init(&data->meshIo);
    data->meshIo.role = mesh_role;
    data->meshIo.gateway_rx_queue =
        xQueueCreate(MESH_GATEWAY_RX_QUEUE_DEPTH, sizeof(mesh_gateway_rx_msg_t));
    if (data->meshIo.gateway_rx_queue == NULL) {
        ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_ERR_CORE_OUT_OF_MEMORY);
        ESP_LOGE(TAG_MESH, "xQueueCreate(gateway_rx_queue) failed");
        return;
    }
    data->meshIo.sock_mutex = xSemaphoreCreateMutex();
    if (data->meshIo.sock_mutex == NULL) {
        ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, MRS_ERR_CORE_OUT_OF_MEMORY);
        ESP_LOGE(TAG_MESH, "xSemaphoreCreateMutex(sock_mutex) failed");
        mesh_gateway_rx_queue_reset(data);
        mesh_io_context_init(&data->meshIo);
        return;
    }
    s_data_manager->objectInfo.meshInfo.ipRoot = CONFIG_IP_ROOT;
    ESP_LOGI(TAG_MESH, "Starting mesh-lite (%s)...", mesh_role == MESH_ROLE_ROOT ? "root" : "child");

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG_MESH, "Event loop already exists, skipping...");
    } else if (err != ESP_OK) {
        ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, err);
        ESP_LOGE(TAG_MESH, "Failed to create event loop: %s", esp_err_to_name(err));
        // Optional: return here if this is a fatal error other than duplicate loop
    }

    /* After InternetManager_Clean() Wi-Fi driver is deinitialized.
     * iot_bridge creates netifs via esp_wifi_set_mode(), so ensure Wiâ€‘Fi is initialized first.
     */
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG_MESH, "WiFi driver already initialized, skipping...");
    } else if (err != ESP_OK) {
        ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, err);
        ESP_LOGE(TAG_MESH, "Failed to initialize WiFi driver: %s", esp_err_to_name(err));
        return;
    }
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_MESH, "Failed to set WiFi storage RAM: %s", esp_err_to_name(err));
    }

    mesh_bridge_create_netifs_no_deauth();
    mesh_wifi_init();
    err = mesh_wifi_start();
    if (err != ESP_OK) {
        ErrorCodes_PushError(data->error_code, DATA_MANAGER_ERROR_CAPACITY, err);
        return;
    }

    if (!s_mesh_lite_initialized) {
        esp_mesh_lite_config_t mesh_lite_config = ESP_MESH_LITE_DEFAULT_INIT();
        mesh_lite_config.join_mesh_ignore_router_status = true;
    /* Root: cáº§n Wiâ€‘Fi Ä‘Ã£ cáº¥u hÃ¬nh (STA) theo vÃ­ dá»¥ mesh-lite; child: join khÃ´ng cáº§n STA. */
        mesh_lite_config.join_mesh_without_configured_wifi = true;
        esp_mesh_lite_erase_rtc_store();
        esp_mesh_lite_init(&mesh_lite_config);
        s_mesh_lite_initialized = true;
    } else {
        ESP_LOGI(TAG_MESH, "Mesh-Lite already initialized, skip init/rebind");
    }
    mesh_lite_sta_config_t router_config;
    memset(&router_config, 0x0, sizeof(router_config));
    err = esp_mesh_lite_set_router_config(&router_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_MESH, "Failed to clear router config for mesh mode: %s",
                 esp_err_to_name(err));
    }

    mesh_app_wifi_set_softap_info();

    if (mesh_role == MESH_ROLE_ROOT) {
        ESP_LOGI(TAG_MESH, "Root node (allowed_level 1)");
        esp_mesh_lite_set_allowed_level(1);
    } else {
        ESP_LOGI(TAG_MESH, "Child node (disallowed_level 1)");
        esp_mesh_lite_set_disallowed_level(1);
    }

    if (!s_mesh_lite_started_once) {
        esp_mesh_lite_start();
        s_mesh_lite_started_once = true;
    } else {
        ESP_LOGI(TAG_MESH, "Mesh-Lite already started once, reconnecting");
        esp_mesh_lite_connect();
    }
    if (xTaskCreate(mesh_connection_task, "mesh_link", 3072, data, 5,
                    &data->TaskHandle_Array[TASK_MESH_LINK]) != pdPASS) {
        ESP_LOGE(TAG_MESH, "xTaskCreate(mesh_link) failed");
    }
    if (xTaskCreate(mesh_data_task, "mesh_data", 4096, data, 5,
                    &data->TaskHandle_Array[TASK_MESH_DATA]) != pdPASS) {
        ESP_LOGE(TAG_MESH, "xTaskCreate(mesh_data) failed");
    }

    s_mesh_info_timer = xTimerCreate("mesh_print_system_info",
                                       pdMS_TO_TICKS(10000), pdTRUE, NULL,
                                       mesh_print_system_info_timercb);
    if (s_mesh_info_timer != NULL) {
        xTimerStart(s_mesh_info_timer, 0);
    } else {
        ESP_LOGW(TAG_MESH, "xTimerCreate(mesh_print_system_info) failed");
    }

    s_mesh_started = true;
}
