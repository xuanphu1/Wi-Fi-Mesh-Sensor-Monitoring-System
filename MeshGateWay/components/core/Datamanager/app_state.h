#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "system_metrics.h"

typedef struct
{
    Status_t status;

    /** Đồng bộ Wi‑Fi STA (cập nhật từ WifiManager). */
    bool sta_connected;
} dm_wifi_t;

typedef struct
{
    /** Client WebSocket đã kết nối server (WSHandle). */
    bool connected;

    /** URI WS đang dùng (cache NVS / mặc định). */
    char url_cached[128];
} dm_ws_t;

typedef struct
{
    uint8_t version[3];
} dm_system_t;

#endif // APP_STATE_H
