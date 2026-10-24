/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WS_TRANSPORT_H
#define WS_TRANSPORT_H

#include "ws_internal.h"

int ws_send_text_locked(const char *text, size_t len, TickType_t timeout);
bool ws_client_can_send(void);
const char *ws_transport_resolve_local_url(const char *url);
const char *ws_transport_get_url_path(const char *url);
void ws_transport_destroy_current(void);
void ws_transport_start(void);
void ws_transport_event_handler(void *arg, esp_event_base_t base,
                             int32_t event_id, void *event_data);

#endif // WS_TRANSPORT_H
