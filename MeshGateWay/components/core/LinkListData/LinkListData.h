#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_LIST_NODE_ID_MAX 32
#define LINK_LIST_NODE_MAX 16
#define LINK_LIST_NODE_PORT_MAX 3

typedef struct {
    int port;
    int type;
    char name[16];
} link_list_sensor_port_t;

typedef struct {
    char id[LINK_LIST_NODE_ID_MAX];
    uint8_t level;
    uint8_t battery_pct;
    bool is_low_bat;
    uint8_t sensor_count;
    link_list_sensor_port_t ports[LINK_LIST_NODE_PORT_MAX];
} link_list_node_snapshot_t;

void link_list_data_init(void);
void link_list_data_ingest_uart_chunk(const uint8_t *data, size_t len);
uint32_t link_list_data_get_version(void);
size_t link_list_data_get_count(void);
uint8_t link_list_data_get_max_level(void);
size_t link_list_data_get_low_battery_count(void);
bool link_list_data_build_dropdown_options(char *buf, size_t buf_size);
bool link_list_data_select_node_by_index(uint16_t index);
bool link_list_data_get_selected_node(link_list_node_snapshot_t *out);

#ifdef __cplusplus
}
#endif
