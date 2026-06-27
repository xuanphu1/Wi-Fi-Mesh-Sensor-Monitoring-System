#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "i2cdev.h"
#include "ds3231.h"
#include "sdkconfig.h"
#include <stddef.h>

// ===== HW Config =====
#ifdef CONFIG_DS3231_I2C_PORT
#define DS3231_I2C_PORT       CONFIG_DS3231_I2C_PORT
#else
#define DS3231_I2C_PORT       0
#endif

#ifdef CONFIG_DS3231_SDA_GPIO
#define DS3231_SDA_GPIO       CONFIG_DS3231_SDA_GPIO
#else
#define DS3231_SDA_GPIO       21
#endif

#ifdef CONFIG_DS3231_SCL_GPIO
#define DS3231_SCL_GPIO       CONFIG_DS3231_SCL_GPIO
#else
#define DS3231_SCL_GPIO       22
#endif

// GPIO34 on ESP32 => ADC1 channel 6
#define BAT_ADC_CHANNEL       ADC1_CHANNEL_6

// Voltage divider: R_TOP (pin -> ADC), R_BOTTOM (ADC -> GND)
// Bạn dùng 2 điện trở 20k - 20k
#define BAT_R_TOP_OHMS        20000
#define BAT_R_BOTTOM_OHMS     20000

// ADC mV calibration scale (integer ratio).
// Đo thực tế của bạn: Vbat=4.02V, divider 20k/20k => Vadc_expected=4.02/2=2.01V
// Log ADC: Vadc_measured ≈ 2.05V => scale ~= 2.01/2.05 ≈ 0.9805
#define BAT_ADC_MV_SCALE_NUM  201
#define BAT_ADC_MV_SCALE_DEN  205

// Battery voltage range (mV) before divider
#define BAT_VMIN_MV           3300
#define BAT_VMAX_MV           4200

#define BAT_ADC_ATTEN         ADC_ATTEN_DB_11
#define BAT_ADC_WIDTH         ADC_WIDTH_BIT_12


typedef enum
{
    STATUS_OK = 0,
    STATUS_ERROR = 1,
    STATUS_WARNING = 2,
    STATUS_INFO = 3,
    STATUS_DEBUG = 4,
    STATUS_TRACE = 5,
    STATUS_FATAL = 6,
    STATUS_UNKNOWN = 7,


} Status_t;

typedef struct
{
    bool rtc_ok;
    struct tm rtc_time;
    int battery_pct;
    /** Ước lượng điện áp gói pin (mV) từ ADC chân chia áp — dùng cho telemetry/WS. */
    uint32_t battery_pack_mv;
    uint32_t sd_total_kb;
    uint32_t sd_free_kb;
    uint32_t sd_used_percent;
    uint32_t ram_free_percent;
    uint32_t ram_free_permille; // 0..1000
    uint32_t ram_free_kb;
    uint32_t ram_used_kb;
    uint32_t cpu_idle_permille; // 0..1000
    uint32_t cpu_load_permille; // 0..1000
    uint32_t fps;
    /** Thời gian chạy từ lúc reset/cấp nguồn (esp_timer, giây). */
    uint32_t uptime_s;
    /** Nhiệt độ chip °C — chỉ hợp lệ khi chip_temp_valid (SoC có driver cảm biến nội bộ). */
    float chip_temp_c;
    bool chip_temp_valid;
    /** True nếu IDF/SoC có driver nhiệt nội bộ (CONFIG_SOC_TEMP_SENSOR_SUPPORTED). ESP32 classic = false → chip_temp_c thường null. */
    bool chip_temp_internal_supported;
} ui_metrics_t;

typedef struct
{
    // Runtime sensor/io state
    i2c_dev_t rtc_dev;
    bool rtc_ready;
    esp_adc_cal_characteristics_t adc_chars;
    bool adc_ready;
} dm_hw_t;

typedef struct
{
    // CPU runtime stats state
    uint64_t prev_total_runtime;
    uint64_t prev_idle_runtime;
    TaskHandle_t idle_task_0;
    TaskHandle_t idle_task_1;
} dm_cpu_t;

typedef struct
{
    // FPS helper (incremented by LVGL task)
    volatile uint32_t loop_counter;
    uint32_t prev_loop_counter;
} dm_lvgl_t;

typedef struct
{
    // Shared metrics for UI
    ui_metrics_t value;
    SemaphoreHandle_t mutex;
} dm_metrics_t;

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

typedef struct
{
    /** Chuỗi gửi lên WebSocket JSON `Time` — SystemMonitor ghi khi có RTC. */
    char timestamp[36];

    /** Giá trị telemetry gửi qua WSHandle `DataToSever`. */
    float temperature;
    float humidity;
    float pressure;
    float pm1_0;
    float pm2_5;
    float pm10;

    /** Thống kê số lượng gói tin để tính throughput và RX/TX rate */
    uint32_t rx_packet_count;
    uint32_t tx_packet_count;
    uint32_t rx_byte_count;
    uint32_t tx_byte_count;
} dm_telemetry_t;

typedef struct
{
    /**
     * Hàng đợi RX từ node (UartToNode đẩy vào) — WebSocket_Handler đọc và gửi lên server.
     * Khởi tạo trong app_main (`xQueueCreate`). Mỗi phần tử ≤ UART_NODE_RX_DATA_MAX byte.
     */
    QueueHandle_t uplink_queue;
} dm_uart_t;

/** Payload một lần đọc UART (chia nhỏ nếu chunk > UART_NODE_RX_DATA_MAX). */
#define UART_NODE_RX_DATA_MAX 512
#define UART_NODE_RX_QUEUE_DEPTH 8

typedef struct {
    size_t len;
    uint8_t data[UART_NODE_RX_DATA_MAX];
} uart_node_rx_item_t;

// ===== UART ↔ Node (handshake wire + context task UartToNode) =====

/** Bitmask ngắt RX UART (ESP32 HAL); dùng với `uart_intr_config`. */
#ifndef UART_TO_NODE_INTR_RXFIFO_FULL
#define UART_TO_NODE_INTR_RXFIFO_FULL (1U << 0)
#define UART_TO_NODE_INTR_RXFIFO_TOUT (1U << 8)
#endif

/** Chuỗi wire handshake đầy đủ — khớp byte-for-byte với node (`Switching...` + \\r\\n). */
#define UART_TO_NODE_HS_WIRE       "Switching to root mode...\r\n"
#define UART_TO_NODE_HS_LEN        (sizeof(UART_TO_NODE_HS_WIRE) - 1)

/** Buffer trượt khớp handshake (phải ≥ UART_TO_NODE_HS_LEN). */
#define UART_TO_NODE_HS_ROLL_BYTES 48

/** Dòng UART / giá trị JSON `type` (“No Node”); macro có \\n cho đúng wire; khi so JSON dùng phần chữ trước \\n. */
#define UART_NODE_MSG_TYPE_NO_NODE "No Node\n"

typedef enum {
    UART_TO_NODE_STATE_START_ROOT = 0,
    UART_TO_NODE_STATE_CONNECTED = 1,
} uart_to_node_state_t;

typedef struct {
    uart_to_node_state_t state;
    TickType_t last_rx_tick;
    TickType_t next_tx_tick;
    uint8_t hs_roll[UART_TO_NODE_HS_ROLL_BYTES];
    size_t hs_fill;
} uart_to_node_ctx_t;

_Static_assert(UART_TO_NODE_HS_LEN <= UART_TO_NODE_HS_ROLL_BYTES,
               "UART handshake wire longer than hs_roll buffer");

#endif // DATAMANAGER_H
