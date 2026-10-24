#ifndef DATAMANAGER_H
#define DATAMANAGER_H

/**
 * Datamanager.h - Compatibility Facade
 *
 * Re-exports all shared data structures and constants split by domain:
 * - hardware_state.h : Hardware configs, ADC, DS3231 and dm_hw_t
 * - system_metrics.h : Status_t, ui_metrics_t, CPU, LVGL, metrics structs
 * - app_state.h      : Wi-Fi, WebSocket, System info runtime state
 * - telemetry_types.h: Telemetry readings and packet counters
 * - uart_protocol.h  : UART queue items, handshake constants and context
 */

#include "hardware_state.h"
#include "system_metrics.h"
#include "app_state.h"
#include "telemetry_types.h"
#include "uart_protocol.h"

#endif // DATAMANAGER_H
