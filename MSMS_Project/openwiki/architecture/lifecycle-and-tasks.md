---
type: lifecycle reference
title: Boot lifecycle and FreeRTOS tasks
description: Initialization ordering, created tasks, priorities, stacks, cadence, and runtime resource ownership.
tags: [lifecycle, freertos, boot, tasks]
---

# Boot lifecycle and tasks

`app_main` first initializes NVS (erases/retries on no-pages/new-version), mounts `/spiffs` with at most five files and no format-on-failure, initializes buttons and common I2C, creates SSD1306, then menu/screen if OLED creation worked. It initializes UART gateway, battery, performance, and default mesh mode; on ESP32 it next initializes DS3231 and SD storage. Finally it creates the two menu tasks. NVS/SPIFFS failures are fatal through `ESP_ERROR_CHECK`; OLED, battery, RTC, and SD failure are logged/recorded where applicable and boot continues.

| Task | Creator | Priority / stack | Cadence | Owns or does |
|---|---|---:|---|---|
| `mesh_tcp` | transport start | 5 / 6144 | select/poll 200 ms | all TCP sockets/parsers; node telemetry or root select loop |
| `uart_gateway_rx` | UART init | 6 / 4096 | event wait 500 ms | UART command line assembly and heartbeat watchdog |
| `uart_gateway_tx` | UART init | 7 / 4096 | queue wait 100 ms | root queue drain and UART TX |
| `MenuNavigation_Task` | main | 5 / 4096 | 10 ms | button/menu transitions |
| `MenuRender_Task` | main | 5 / 4096 | 10 ms | OLED renders under ScreenManager mutex |
| battery read task | BatteryManager | source-created / default | 5 s | averaged ADC voltage and `BatteryInfo_t` |
| `sys_perf` | SystemPerformance | 5 / 2048 | 1 s | CPU/RAM histories |
| storage task | StorageManager | source-created / default | 10 s | SD `datalog.txt` append |
| `wifi_connect`, `wifi_mgr` | WifiManager | 5 / 4096 each | 1 s | Wi-Fi connection/reconnect policy |
| `dns_server` | AP init | 5 / 4096 | socket receive timeout 1 s | captive DNS socket |

Tasks whose source does not capture priority/stack in the inspected snippets are intentionally marked source-created/default rather than guessed. Sensor read tasks created by FunctionManager run every second per selected port. `mesh_info` is a FreeRTOS software timer every 10 seconds, not a task.

Task interactions are intentionally queue/mutex bounded: ScreenManager serializes OLED accesses; WifiManager serializes state with a mutex/event group; I2Cdev has bus/device locks; transport keeps sockets single-owner; root gateway handoff uses a bounded queue. See [Application state](../core/application-state-and-services.md) and [Local services](../platform/local-services.md).