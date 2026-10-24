/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WS_TIME_SYNC_H
#define WS_TIME_SYNC_H

#include "ws_internal.h"
#include <stdbool.h>

void ws_send_time_sync_request(void);
void ws_handle_sync_time(cJSON *root);
bool websocket_is_time_synced(void);

#endif // WS_TIME_SYNC_H
