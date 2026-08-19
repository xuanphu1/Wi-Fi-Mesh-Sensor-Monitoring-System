---
type: wiki entrypoint
title: ESP32 mesh sensor firmware wiki
description: Navigation and change-routing guide for the environmental sensor mesh firmware.
tags: [quickstart, esp-idf, mesh, sensors]
---

# ESP32 mesh sensor firmware

This wiki documents the first-party ESP-IDF firmware currently configured for **ESP32**, a distributed environmental sensor system. A device defaults to mesh node mode; configured node sensors build JSON telemetry and send it via TCP to a mesh root. The root aggregates frames and writes each to its UART gateway.

## Start here

- [Architecture overview](architecture/overview.md) — boundaries and component ownership.
- [Boot lifecycle and FreeRTOS tasks](architecture/lifecycle-and-tasks.md) — exact startup and task scheduling.
- [Mesh TCP](network/mesh-tcp.md) — the primary production telemetry plane.
- [Telemetry contracts and APIs](contracts/telemetry-and-apis.md) — JSON, framing and public surfaces.
- [Internet and Wi-Fi](network/internet-and-wifi.md) — mutually exclusive network modes and captive portal.
- [Sensors and drivers](hardware/sensors-and-drivers.md) — registry and hardware extension seam.
- [UI interaction](ui/interaction-and-rendering.md) and [Local services](platform/local-services.md) — OLED/menu and peripherals.
- [Configuration and persistence](operations/configuration-and-persistence.md) — sdkconfig/Kconfig, NVS, SPIFFS and security posture.
- [Build/deploy/debug](operations/build-deploy-debug.md), [File atlas](reference/file-atlas.md), and [Review items](reference/known-limitations.md).

## Task routing

| Intent | Read first | Owning source/symbols | Focused validation |
|---|---|---|---|
| change node/root TCP behavior | [Mesh TCP](network/mesh-tcp.md) | `mesh_tcp_transport.c`, `MeshManager_*` | build + root/multi-node integration |
| change telemetry fields | [Telemetry contracts](contracts/telemetry-and-apis.md) | `mesh_telemetry_build`, `SensorRegistry` | build + capture newline JSON at root UART |
| add selectable sensor | [Sensors](hardware/sensors-and-drivers.md) | `SensorConfig`, `SensorRegistry`, raw driver | build + menu select/read + telemetry |
| change network mode/provisioning | [Internet and Wi-Fi](network/internet-and-wifi.md) | `InternetManager_SwitchMode`, WifiManager handlers | build + STA/AP transition check |
| change UART gateway protocol | [Mesh TCP](network/mesh-tcp.md) | `UartToGateWay_*` | root command/heartbeat/frame forwarding |
| adjust board pins/config/flash layout | [Configuration](operations/configuration-and-persistence.md) | Kconfig, `sdkconfig`, `partitions.csv` | clean build + board smoke test |
| locate any in-scope file | [File atlas](reference/file-atlas.md) | exact path | narrow component build/integration |

## Fast build

```sh
esp
idf.py build
```

The repository directory uses `MSMS_Project`, but CMake declares `MRS_Project`; the build artifact is therefore `MRS_Project.bin`. No source-proven backlog is deferred: application components are covered; managed dependencies and Mesh-Lite `external_examples` are intentionally outside application-logic scope.