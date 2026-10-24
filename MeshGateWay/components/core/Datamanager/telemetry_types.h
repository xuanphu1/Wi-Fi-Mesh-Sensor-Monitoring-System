#ifndef TELEMETRY_TYPES_H
#define TELEMETRY_TYPES_H

#include <stdint.h>

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

#endif // TELEMETRY_TYPES_H
