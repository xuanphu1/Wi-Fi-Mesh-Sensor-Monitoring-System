---
type: operations reference
title: Build configuration, persistence, and provisioning security
description: ESP-IDF build inputs, target configuration, NVS and SPIFFS persistence, partition layout, and configuration ingress controls.
tags: [configuration, sdkconfig, kconfig, nvs, spiffs, security]
---

# Configuration, persistence, and provisioning

## Effective configuration sources

Root `CMakeLists.txt` adds six application component roots, invokes ESP-IDF project machinery as `MRS_Project`, and creates a flashed SPIFFS image from `component/network/WebConfigWifi`. `sdkconfig` is the committed effective configuration: target `esp32` / Xtensa, TCP port 8070, max TCP clients 5, send interval 1000 ms, connect timeout 5000 ms, and LwIP max sockets 10. Component `Kconfig.projbuild` files define configurable pin, button, Wi-Fi/AP, mesh TCP, MH-Z14A and SD values; `PinManager.h` maps many directly to macros.

Compile-time `#if CONFIG_IDF_TARGET_ESP32` enables RTC/SD startup and actuator pins. Do not infer target from README: use sdkconfig. CMake project name governs `MRS_Project.bin`.

**Target review boundary:** `app_main` gates RTC/SD, but starts BatteryManager on every target. BatteryManager uses legacy `driver/adc.h`, `ADC1_CHANNEL_0`, and a classic ESP32 voltage/calibration model. PinManager Kconfig is the hardware source for UART, I2C, SPI and pulse consumers (gateway, sensor wrappers, RTC, OLED, SD); validate target-compatible peripheral/pin mappings at build configuration time. Unsupported target paths should be compiled out or return an explicit unsupported error before starting their task, rather than rely on a target-invalid driver call.

## Persistent state and partitions

| Partition | Offset / size | Role |
|---|---|---|
| `nvs` | `0x9000` / `0x5000` | NVS credentials/configuration namespaces |
| `otadata` | `0xe000` / `0x2000` | OTA selection metadata |
| `ota_0` | `0x10000` / `0x1C0000` | first application image |
| `ota_1` | next / `0x1C0000` | second application image |
| `spiffs` | next / `0x20000` | portal `index.html` and `redirect.html` image |

At boot NVS is initialized; only no-free-pages/new-version causes erase and retry. SPIFFS mounts at `/spiffs`, does not format if mount fails, and the portal returns errors if expected assets cannot be opened.

| Storage | Namespace / keys | Writer / reader |
|---|---|---|
| AP addressing | `wifi_ap`: `ap_ip`, `ap_netmask`, `ap_gateway` | portal AP helpers |
| HTTP endpoint | `http_cfg`: `http_ip`, `http_port` | `/configure`, Wi-Fi startup/status |
| WebSocket URL | `ws_handle`: `url` | `/configure`, WSHandle |
| SD log | mounted SD `datalog.txt` | StorageManager task |

Runtime configured Wi-Fi credentials are held in WifiManager RAM fields; they are not persisted by the inspected source. Startup Wi-Fi Kconfig values and hard-coded fallback list are source literals; do not place production secrets in source/config tracked by version control.

## Security boundary

The captive portal runs an open AP and unauthenticated HTTP routes. `/configure` can accept credentials, endpoint and WebSocket URL; no source-backed authorization, TLS, or input semantic validation beyond basic JSON/string/buffer checks exists. Treat it as local commissioning only. The configured `http_port` is status/config data; actual captive HTTP binds port 80. See [Internet and Wi-Fi](../network/internet-and-wifi.md) for request behavior.