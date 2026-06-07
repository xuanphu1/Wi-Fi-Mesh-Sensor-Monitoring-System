#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "Datamanager.h"
#include <stdbool.h>

void screen_manager_start(dm_metrics_t *metrics, dm_lvgl_t *lvgl, UBaseType_t priority, BaseType_t core_id);
void screen_manager_set_ws_status(bool connected);

#endif // SCREEN_MANAGER_H
