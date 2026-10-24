#ifndef SCREEN_DASHBOARD_H
#define SCREEN_DASHBOARD_H

#include "screen_internal.h"
#include "WSHandle.h"
#include <stdint.h>

const char *screen_dashboard_get_weekday_name(uint8_t wday);
extern const char *screen_dashboard_month_names[];
const char *screen_dashboard_get_reset_reason_name(void);
void screen_dashboard_render_target_button(websocket_target_t target);
void screen_dashboard_render_static(void);

#endif // SCREEN_DASHBOARD_H
