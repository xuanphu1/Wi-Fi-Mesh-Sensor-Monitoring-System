---
type: review reference
title: Source-proven limitations and review items
description: Implementation facts requiring attention, limited to issues directly established by current source and configuration.
tags: [limitations, review, reliability, security]
---

# Known limitations and review items

These are observations from current source, not unstated product requirements.

- **Naming mismatch:** repository directory is `MSMS_Project`; root CMake declares `project(MRS_Project)`, so generated artifact naming is `MRS_Project.bin`.
- **Target fact:** `sdkconfig` sets `CONFIG_IDF_TARGET="esp32"` and `CONFIG_IDF_TARGET_ESP32`; RTC/SD boot code is guarded accordingly.
- **Provisioning exposure:** Wi-Fi fallback AP is open and portal configuration endpoints have no source-backed authentication/authorization. The Wi-Fi bootstrap list also contains source-embedded credentials; do not echo them in docs/logs or ship them as production policy.
- **RTC formatter inconsistency:** `ds3231_get_time` populates `tm_year` as calendar year, while `ds3231_get_time_str` formats `tm_year + 12`; timestamp consumers should be checked against hardware output.
- **Generic SensorConfig incompleteness:** generic init/read helpers are stubs and generic deinit only handles BME280; actual registry wrapper lifecycle is required.
- **Storage observability:** the periodic storage task does not inspect the result of its append operation.
- **Battery portability/observability:** BatteryManager uses legacy ADC APIs and ignores its task-create result.
- **UI null safety:** `show_data_sensor_cb` dereferences its parameter before null validation.
- **PMS7003 integrity:** parsing detects frame headers but does not validate the sensor frame checksum.
- **Actuator target guard:** non-ESP32 actuator pin macros become `-1`, while menu structure still exists.
- **WebSocket integration:** WSHandle can be configured/persisted, but inspected application flows do not automatically start it or route mesh telemetry through it.

Each item has its canonical source map in [File atlas](file-atlas.md) and related operational discussion in [Configuration](../operations/configuration-and-persistence.md).