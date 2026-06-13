#include "ProcessingDataMesh.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_timer.h"
#include "esp_log.h"

#define PROCESSING_DATA_MESH_MAX_NODES 16
#define PROCESSING_DATA_MESH_NODE_ID_LEN 16
#define PROCESSING_DATA_MESH_PACKETLOSS_WINDOW_MS 10000ULL

static const char *TAG = "ProcessingDataMesh";

typedef struct {
    bool used;
    char node_id[PROCESSING_DATA_MESH_NODE_ID_LEN];
    uint32_t last_seq;
    uint64_t window_start_ms;
    uint32_t window_received;
    uint32_t window_lost;
} mesh_packetloss_state_t;

static mesh_packetloss_state_t s_packetloss_state[PROCESSING_DATA_MESH_MAX_NODES];

static const cJSON *json_get_required_item(const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    return item;
}

static const char *json_get_string_or_default(const cJSON *root, const char *key,
                                              const char *fallback)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        return item->valuestring;
    }
    return fallback;
}

static int json_get_int_or_default(const cJSON *root, const char *key, int fallback)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsNumber(item)) {
        return item->valueint;
    }
    return fallback;
}

static uint32_t json_get_u32_or_zero(const cJSON *root, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0) {
        return 0;
    }
    return (uint32_t)item->valuedouble;
}

static mesh_packetloss_state_t *packetloss_state_for_node(const char *node_id)
{
    mesh_packetloss_state_t *free_slot = NULL;

    for (size_t i = 0; i < PROCESSING_DATA_MESH_MAX_NODES; i++) {
        mesh_packetloss_state_t *state = &s_packetloss_state[i];
        if (state->used && strncmp(state->node_id, node_id, sizeof(state->node_id)) == 0) {
            return state;
        }
        if (!state->used && free_slot == NULL) {
            free_slot = state;
        }
    }

    if (free_slot == NULL) {
        free_slot = &s_packetloss_state[0];
        ESP_LOGW(TAG, "Packetloss state table full, rotating slot 0");
    }

    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->used = true;
    strlcpy(free_slot->node_id, node_id, sizeof(free_slot->node_id));
    return free_slot;
}

static uint64_t monotonic_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static void packetloss_reset_window(mesh_packetloss_state_t *state, uint64_t now_ms)
{
    state->window_start_ms = now_ms;
    state->window_received = 0;
    state->window_lost = 0;
}

static double calculate_packetloss_percent(const char *node_id, uint32_t seq)
{
    if (seq == 0) {
        return 0.0;
    }

    mesh_packetloss_state_t *state = packetloss_state_for_node(node_id);
    uint64_t now_ms = monotonic_ms();
    if (state->window_start_ms == 0 ||
        (now_ms - state->window_start_ms) >= PROCESSING_DATA_MESH_PACKETLOSS_WINDOW_MS ||
        (state->last_seq != 0 && seq <= state->last_seq)) {
        packetloss_reset_window(state, now_ms);
    }

    if (state->last_seq != 0 && seq > (state->last_seq + 1U)) {
        state->window_lost += seq - state->last_seq - 1U;
    }
    state->window_received++;
    state->last_seq = seq;

    uint32_t expected = state->window_received + state->window_lost;
    if (expected == 0) {
        return 0.0;
    }
    return ((double)state->window_lost * 100.0) / (double)expected;
}

system_err_t ProcessingDataMesh_ProcessFrame(const uint8_t *input,
                                             size_t input_len,
                                             char *output,
                                             size_t output_cap,
                                             size_t *output_len)
{
    if (input == NULL || input_len == 0 || output == NULL || output_cap == 0) {
        return MRS_ERR_CORE_INVALID_PARAM;
    }

    cJSON *root = cJSON_ParseWithLength((const char *)input, input_len);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return MRS_ERR_CORE_INVALID_PARAM;
    }

    const char *ip = json_get_string_or_default(root, "i", "0.0.0.0");
    uint32_t seq = json_get_u32_or_zero(root, "seq");
    double packetloss_percent = calculate_packetloss_percent(ip, seq);

    const cJSON *err_item = json_get_required_item(root, "err");
    const cJSON *ports_item = json_get_required_item(root, "p");
    char *err_json = err_item != NULL ? cJSON_PrintUnformatted((cJSON *)err_item) : NULL;
    char *ports_json = ports_item != NULL ? cJSON_PrintUnformatted((cJSON *)ports_item) : NULL;
    if ((err_item != NULL && err_json == NULL) || (ports_item != NULL && ports_json == NULL)) {
        cJSON_free(err_json);
        cJSON_free(ports_json);
        cJSON_Delete(root);
        return MRS_ERR_CORE_OUT_OF_MEMORY;
    }
    const char *err_text = err_json != NULL ? err_json : "[]";
    const char *ports_text = ports_json != NULL ? ports_json : "[]";

    int written = snprintf(output, output_cap,
                           "{\"v\":%d,\"packetloss\":%.2f,\"n\":%d,"
                           "\"i\":\"%s\",\"t\":\"%s\",\"ver\":\"%s\","
                           "\"err\":%s,\"p\":%s}",
                           json_get_int_or_default(root, "v", 0),
                           packetloss_percent,
                           json_get_int_or_default(root, "n", 0),
                           ip,
                           json_get_string_or_default(root, "t", "1970-01-01T00:00:00"),
                           json_get_string_or_default(root, "ver", "0.0.0"),
                           err_text,
                           ports_text);

    cJSON_free(err_json);
    cJSON_free(ports_json);
    cJSON_Delete(root);

    if (written < 0 || (size_t)written >= output_cap) {
        return MRS_FAIL;
    }

    if (output_len != NULL) {
        *output_len = (size_t)written;
    }

    return MRS_OK;
}
