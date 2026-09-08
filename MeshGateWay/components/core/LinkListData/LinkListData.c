#include "LinkListData.h"

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>

typedef struct link_node {
    link_list_node_snapshot_t data;
    struct link_node *next;
} link_node_t;

static link_node_t s_nodes[LINK_LIST_NODE_MAX];
static link_node_t *s_head = NULL;
static size_t s_count = 0;
static char s_selected_id[LINK_LIST_NODE_ID_MAX] = {0};
static uint32_t s_version = 0;
static SemaphoreHandle_t s_mutex = NULL;

static void ensure_init(void) {
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }
}

void link_list_data_init(void) {
    ensure_init();
}

static const char *sensor_type_name(int type) {
    switch (type) {
    case 0:
        return "BME280";
    case 1:
        return "SHT30";
    case 2:
        return "SCD41";
    case 3:
        return "PMS7003";
    case 13:
        return "SHT40";
    default:
        return NULL;
    }
}

static void fill_sensor_name(int type, char *buf, size_t buf_size) {
    const char *name = sensor_type_name(type);
    if (name != NULL) {
        snprintf(buf, buf_size, "%s", name);
    } else {
        snprintf(buf, buf_size, "TYPE%d", type);
    }
}

static const char *json_get_id(cJSON *root) {
    static const char *keys[] = {"mac", "MAC", "m", "id", "i"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        cJSON *item = cJSON_GetObjectItem(root, keys[i]);
        if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
            return item->valuestring;
        }
    }
    return NULL;
}

static link_node_t *find_node_locked(const char *id) {
    for (link_node_t *node = s_head; node != NULL; node = node->next) {
        if (strncmp(node->data.id, id, sizeof(node->data.id)) == 0) {
            return node;
        }
    }
    return NULL;
}

static link_node_t *alloc_node_locked(void) {
    if (s_count >= LINK_LIST_NODE_MAX) {
        return NULL;
    }

    link_node_t *node = &s_nodes[s_count++];
    memset(node, 0, sizeof(*node));
    node->next = s_head;
    s_head = node;
    return node;
}

static void parse_ports(cJSON *root, link_list_node_snapshot_t *out) {
    cJSON *ports = cJSON_GetObjectItem(root, "p");
    if (!cJSON_IsArray(ports)) {
        return;
    }

    uint8_t count = 0;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, ports) {
        if (count >= LINK_LIST_NODE_PORT_MAX || !cJSON_IsArray(entry)) {
            continue;
        }

        cJSON *j_port = cJSON_GetArrayItem(entry, 0);
        cJSON *j_type = cJSON_GetArrayItem(entry, 1);
        if (!cJSON_IsNumber(j_port) || !cJSON_IsNumber(j_type)) {
            continue;
        }

        out->ports[count].port = j_port->valueint;
        out->ports[count].type = j_type->valueint;
        fill_sensor_name(out->ports[count].type, out->ports[count].name,
                         sizeof(out->ports[count].name));
        count++;
    }

    out->sensor_count = count;
}

static void ingest_line(const char *line) {
    if (line == NULL || line[0] != '{') {
        return;
    }

    cJSON *root = cJSON_Parse(line);
    if (root == NULL) {
        return;
    }

    const char *id = json_get_id(root);
    if (id == NULL) {
        cJSON_Delete(root);
        return;
    }

    link_list_node_snapshot_t next = {0};
    snprintf(next.id, sizeof(next.id), "%s", id);

    cJSON *j_n = cJSON_GetObjectItem(root, "n");
    if (cJSON_IsNumber(j_n) && j_n->valueint > 0) {
        next.level = (uint8_t)j_n->valueint;
    } else {
        next.level = 1;
    }

    cJSON *j_bat = cJSON_GetObjectItem(root, "bat");
    if (!j_bat) j_bat = cJSON_GetObjectItem(root, "battery");
    if (!j_bat) j_bat = cJSON_GetObjectItem(root, "b");
    if (cJSON_IsNumber(j_bat)) {
        next.battery_pct = (uint8_t)j_bat->valueint;
        if (next.battery_pct > 0 && next.battery_pct <= 20) {
            next.is_low_bat = true;
        }
    }

    cJSON *j_err = cJSON_GetObjectItem(root, "err");
    if (cJSON_IsArray(j_err)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, j_err) {
            if (cJSON_IsString(item) && item->valuestring &&
                (strstr(item->valuestring, "BAT") || strstr(item->valuestring, "bat"))) {
                next.is_low_bat = true;
            }
        }
    }

    parse_ports(root, &next);
    cJSON_Delete(root);

    ensure_init();
    if (s_mutex == NULL ||
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }

    link_node_t *node = find_node_locked(next.id);
    bool is_new_node = false;
    if (node == NULL) {
        node = alloc_node_locked();
        is_new_node = node != NULL;
    }
    if (node != NULL) {
        node->data = next;
        if (s_selected_id[0] == '\0') {
            snprintf(s_selected_id, sizeof(s_selected_id), "%s", next.id);
        }
        if (is_new_node) {
            s_version++;
        }
    }

    xSemaphoreGive(s_mutex);
}

void link_list_data_ingest_uart_chunk(const uint8_t *data, size_t len) {
    enum { LINE_BUF_SZ = 1024 };
    static char s_line[LINE_BUF_SZ];
    static size_t s_line_len = 0;

    if (data == NULL || len == 0) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (s_line_len > 0) {
                s_line[s_line_len] = '\0';
                ingest_line(s_line);
                s_line_len = 0;
            }
            continue;
        }
        if (s_line_len < LINE_BUF_SZ - 1U) {
            s_line[s_line_len++] = c;
        } else {
            s_line_len = 0;
        }
    }
}

uint32_t link_list_data_get_version(void) {
    ensure_init();
    uint32_t version = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        version = s_version;
        xSemaphoreGive(s_mutex);
    }
    return version;
}

size_t link_list_data_get_count(void) {
    ensure_init();
    size_t count = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        count = s_count;
        xSemaphoreGive(s_mutex);
    }
    return count;
}

uint8_t link_list_data_get_max_level(void) {
    ensure_init();
    uint8_t max_level = 1; // Gateway root is layer 1 in Mesh-Lite
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        for (link_node_t *node = s_head; node != NULL; node = node->next) {
            if (node->data.level > max_level) {
                max_level = node->data.level;
            }
        }
        xSemaphoreGive(s_mutex);
    }
    return max_level;
}

size_t link_list_data_get_low_battery_count(void) {
    ensure_init();
    size_t count = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        for (link_node_t *node = s_head; node != NULL; node = node->next) {
            if (node->data.is_low_bat) {
                count++;
            }
        }
        xSemaphoreGive(s_mutex);
    }
    return count;
}

bool link_list_data_build_dropdown_options(char *buf, size_t buf_size) {
    if (buf == NULL || buf_size == 0) {
        return false;
    }

    ensure_init();
    if (s_mutex == NULL ||
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        snprintf(buf, buf_size, "NONE");
        return false;
    }

    if (s_count == 0) {
        snprintf(buf, buf_size, "NONE");
        xSemaphoreGive(s_mutex);
        return false;
    }

    size_t used = 0;
    for (link_node_t *node = s_head; node != NULL; node = node->next) {
        int written = snprintf(buf + used, buf_size - used, "%s%s",
                               used == 0 ? "" : "\n", node->data.id);
        if (written < 0 || (size_t)written >= buf_size - used) {
            break;
        }
        used += (size_t)written;
    }

    xSemaphoreGive(s_mutex);
    return true;
}

bool link_list_data_select_node_by_index(uint16_t index) {
    ensure_init();
    if (s_mutex == NULL ||
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return false;
    }

    uint16_t current = 0;
    bool ok = false;
    for (link_node_t *node = s_head; node != NULL; node = node->next) {
        if (current == index) {
            snprintf(s_selected_id, sizeof(s_selected_id), "%s", node->data.id);
            ok = true;
            break;
        }
        current++;
    }

    xSemaphoreGive(s_mutex);
    return ok;
}

bool link_list_data_get_selected_node(link_list_node_snapshot_t *out) {
    if (out == NULL) {
        return false;
    }

    ensure_init();
    if (s_mutex == NULL ||
        xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return false;
    }

    link_node_t *node = NULL;
    if (s_selected_id[0] != '\0') {
        node = find_node_locked(s_selected_id);
    }
    if (node == NULL) {
        node = s_head;
    }

    if (node != NULL) {
        *out = node->data;
        xSemaphoreGive(s_mutex);
        return true;
    }

    xSemaphoreGive(s_mutex);
    return false;
}
