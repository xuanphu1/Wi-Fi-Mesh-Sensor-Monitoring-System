---
type: platform services
title: Local platform services and peripherals
description: Battery, time, SD storage, performance, pins, and UART gateway ownership and failure behavior.
tags: [platform, battery, rtc, storage, uart, performance]
---

# Local services and peripherals

| Service | Entrypoint and owned state | Behavior and failures |
|---|---|---|
| BatteryManager | `BatteryManager_Init`, `StartTask`, getters; latest globals | legacy ADC1 channel 0, 20-sample average every 5 s, divider/correction and percentage/icon mapping update `BatteryInfo_t`; init errors return, but task create result is not surfaced |
| TimeManager | `TimeManager_Init`, timestamp/epoch/tm APIs; DS3231 descriptor/init flag | configured I2C DS3231 read supplies telemetry/storage time; unready/read failure uses explicit fallback strings/error |
| StorageManager | mount flag and `StorageManager_StartTask` | SD SPI mount then 10 s appends timestamp/battery/Wi-Fi status to `datalog.txt`; background task ignores append return |
| SystemPerfomance | histories + mutex; `SystemPerformance_Init` | one-second CPU idle/runtime and heap percentage histories of 128 values |
| PinManager | compile-time macros only | maps Kconfig UART, I2C, SPI, pulse pins; no runtime manager |
| UartToGateWay | UART driver/event queue plus RX/TX tasks | configured gateway UART, commands/heartbeat/queue forward; pause/resume releases driver for UART sensor setup |

RTC and storage boot initialization is guarded by `CONFIG_IDF_TARGET_ESP32`; sdkconfig currently selects ESP32. UART is a contention boundary: SensorConfig’s PMS7003/MH-Z14A UART drivers use configured peripheral UART values, while the gateway uses PinManager gateway UART values; FunctionManager explicitly pauses the gateway only during UART sensor initialization/reset. Verify board pin assignment before enabling both.

## UART pause and sensor lifecycle contract

`UartToGateWay_Init` creates persistent RX/TX tasks. `UartToGateWay_Pause` merely delays 600 ms, deletes the driver, and nulls/replaces the event queue on resume; FunctionManager invokes it before initializing a UART sensor. RX may still be receiving, flushing, or resetting the queue, so this delay is not a safe acknowledgement. A safe extension requires: pause request → RX/TX quiesce and acknowledge → owner deletes driver → sensor owns UART → sensor releases → owner installs driver/new queue → tasks acknowledge resume. Cover partial initialization/task-create failure so a task is never left using a deleted driver or stale queue.

The local read-to-telemetry route is: raw driver → SensorConfig adapter → registry `sensor_driver_t` → FunctionManager read task or `mesh_telemetry_build` → DataManager/JSON. Registry drivers and `is_init` are global per sensor type whereas selection/data are per port: the same type can be selected on multiple ports while sharing one driver. Reads and reset/deinit have no shared lifecycle lock. Adopt per-port instances or explicit exclusivity plus reader stop/join and stale-data clearing before changing this behavior. See [Telemetry contracts](../contracts/telemetry-and-apis.md).