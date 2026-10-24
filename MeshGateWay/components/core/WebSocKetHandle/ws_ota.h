/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WS_OTA_H
#define WS_OTA_H

#include "ws_internal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

void ws_ota_progress_callback(const char *job_id, const char *status,
                              uint8_t percent, uint32_t bytes_read,
                              uint32_t total_bytes,
                              const char *running_part,
                              const char *target_part,
                              const char *msg);
void ws_send_ota_gateway_progress(void);
bool ws_json_type_matches(const cJSON *type);
const char *ws_json_get_ota_url(cJSON *root);
void ws_normalize_ota_url(const char *raw_url, char *out_url, size_t out_len);
void ws_handle_ota_command(cJSON *root);
void ws_ota_check_node_timeout(void);
void ws_ota_handle_node_progress_from_uart(const char *payload);

#endif // WS_OTA_H
