---
type: hardware reference
title: Sensors, registry, and hardware drivers
description: Sensor selection and wrapper lifecycle, registered transports, driver scope, bus sharing, and extension procedure.
tags: [sensors, drivers, i2c, uart, hardware]
---

# Sensors and drivers

`SensorTypes.h` defines three logical ports, `SensorData_t` (five float/32/16/8-bit slots) and `sensor_driver_t` ABI. `SensorRegistry.c` statically registers BME280, MH-Z14A, PMS7003, DHT22, AHT10, DHT11, and HTU21D. Registry entries provide name, field labels/units, interface and wrapper init/read/deinit pointers. No BMP280, ADS1115, DS3231, PCF8574, SD card, SSD1306 or generic WebSocket component is a selectable registry sensor; those are available drivers/services.

| Registered type | Interface | Wrapper/raw driver output |
|---|---|---|
| BME280 | I2C | temperature, pressure, humidity |
| AHT10 | I2C | temperature, humidity |
| HTU21D | I2C | temperature, humidity |
| MH-Z14A | UART or PWM by Kconfig | CO2 and UART temperature where available |
| PMS7003 | UART | PM1.0, PM2.5, PM10 |
| DHT11 / DHT22 | pulse | temperature, humidity; reads cached for two seconds |

The extension seam is evidence-backed: implement raw driver support if needed, add SensorConfig wrapper functions using `SensorData_t` and `system_err_t`, add a `sensor_driver_t` record in `SensorRegistry.c`, and validate selection/reset plus telemetry against a real device. A registered driver’s `initialized` flag controls lifecycle. Generic `SensorConfigInit`/`SensorConfigRead` are currently stubs and `SensorConfigDeinit` handles only BME280, so extend the concrete registry wrapper path rather than relying on generic helpers.

I2Cdev is shared legacy I2C infrastructure with port/device locking and PinManager settings. SSD1306 uses its own I2C code and only ScreenManager’s display mutex, so it is not protected by I2Cdev’s lock. Raw drivers include BMP/BME, AHT, Si7021/HTU21D, DHT, DS3231, ADS1115, MH-Z14A, PMS7003, PCF8574, SD, SSD1306 and WebSocket. The [File atlas](../reference/file-atlas.md) maps their exact public structures and files.

Driver failure results are normalized by wrappers where registered: null data, uninitialized device, init, communication and sensor-read errors feed ErrorCodes. The telemetry builder omits a failed port while preserving errors, rather than emitting fabricated values.