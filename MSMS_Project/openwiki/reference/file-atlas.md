---
type: source atlas
title: First-party file atlas
description: Exact-path inventory of in-scope first-party source, headers, manifests, configuration, and SPIFFS assets with responsibilities and dependencies.
tags: [source-map, files, components, configuration]
---

# First-party file atlas

Scope is root production artifacts, `main`, and first-party files under `component/{core,network,sensors,ui,drivers,utils}`. Excluded: `managed_components`, `component/drivers/espressif__mesh_lite/external_examples`, generated `build`, and their dependency implementations. The Mesh-Lite component itself is an external dependency surface, not application logic.

## Root and main

| Exact path | Core responsibility / inbound → outbound |
|---|---|
| `CMakeLists.txt` | registers first-party component roots, calls `project(MRS_Project)`, builds/flashes SPIFFS image from WebConfigWifi |
| `partitions.csv` | NVS, OTA metadata, two 0x1C0000 app slots, 0x20000 SPIFFS layout |
| `sdkconfig` | effective ESP32/IDF/LwIP/Kconfig configuration |
| `dependencies.lock` | managed dependency lock, not application logic |
| `main/CMakeLists.txt` | registers `main.c` and direct component requirements |
| `main/main.h` | static global `DataManager` and `MainScreen` declarations |
| `main/main.c` | `app_main`, NVS/SPIFFS and service composition |

## Core and utilities

| Exact path(s) | Core responsibility / public surface |
|---|---|
| `component/core/DataManager/{DataManager.c,DataManager.h,CMakeLists.txt}` | shared `DataManager_t`, UI/sensor/status/task/error structures; source file is type-only |
| `component/core/FunctionManager/{FunctionManager.c,FunctionManager.h,CMakeLists.txt}` | menu callbacks, mesh role/Wi-Fi actions, sensor/read task, GPIO actuator lifecycle |
| `component/core/BatteryManager/{BatteryManager.c,BatteryManager.h,CMakeLists.txt}` | ADC battery init/task/getters/display update |
| `component/core/PinManager/{PinManager.c,PinManager.h,Kconfig.projbuild,CMakeLists.txt}` | Kconfig pin/baud/I2C/SPI/pulse macros; no runtime state |
| `component/core/ProcessingDataMesh/{ProcessingDataMesh.c,ProcessingDataMesh.h,CMakeLists.txt}` | JSON frame transform and MAC/sequence packet-loss window |
| `component/core/StorageManager/{StorageManager.c,StorageManager.h,CMakeLists.txt}` | SD mount/append and periodic log task |
| `component/core/SystemPerfomance/{SystemPerfomance.c,SystemPerfomance.h,CMakeLists.txt}` | CPU/RAM history mutex/task/getters; directory spelling is source truth |
| `component/core/TimeManager/{TimeManager.c,TimeManager.h,CMakeLists.txt}` | DS3231 initialization and timestamp/time API |
| `component/core/UartToGateWay/{UartToGateWay.c,UartToGateWay.h,CMakeLists.txt}` | gateway UART init, RX/TX tasks, role command and mesh frame output |
| `component/utils/ErrorCodes/{ErrorCodes.c,ErrorCodes.h,CMakeLists.txt}` | encoded custom error definitions, names/module helpers and rolling history append |
| `component/utils/BitManager/{BitManager.c,BitManager.h,CMakeLists.txt}` | shared bit-array helpers and component build declaration |

## Network

| Exact path(s) | Core responsibility / public surface |
|---|---|
| `component/network/InternetManager/{InternetManager.c,InternetManager.h,CMakeLists.txt}` | `INTERNET_MODE_*` state, cleanup/start/switch/status API |
| `component/network/MeshManager/{MeshManager.c,Kconfig.projbuild,CMakeLists.txt,include/MeshManager.h}` | public lifecycle/role/status/gateway frame API; owns private context/queue |
| `component/network/MeshManager/private/{mesh_network.c,mesh_network.h}` | bridge Wi-Fi, Mesh-Lite init/roles/topology timer |
| `component/network/MeshManager/private/{mesh_tcp_transport.c,mesh_tcp_transport.h}` | single `mesh_tcp` owner of node/listener/client descriptors, select and retries |
| `component/network/MeshManager/private/{mesh_stream_parser.c,mesh_stream_parser.h}` | per-client newline parser, CRLF/oversize behavior |
| `component/network/MeshManager/private/{mesh_telemetry.c,mesh_telemetry.h}` | compact telemetry builder and origin extractor |
| `component/network/WifiManager/{WifiManager.h,wifi_manager_internal.h,wifi_manager_core.c,wifi_manager_ap.c,wifi_manager_http.c,wifi_manager_tasks.c,CMakeLists.txt,Kconfig.projbuild}` | STA/AP, event state, captive DNS, HTTP routes, persistence and reconnect tasks |
| `component/network/WebSocKetHandle/{WSHandle.c,WSHandle.h,CMakeLists.txt}` | mutex-guarded WebSocket wrapper, callback dispatch, NVS URL persistence |
| `component/network/WebConfigWifi/{index.html,redirect.html}` | SPIFFS captive portal content embedded by root CMake |

## Sensors and UI

| Exact path(s) | Core responsibility / public surface |
|---|---|
| `component/sensors/SensorTypes/{SensorTypes.h,CMakeLists.txt}` | port/type/interface enums and `sensor_driver_t` ABI |
| `component/sensors/SensorRegistry/{SensorRegistry.c,SensorRegistry.h,CMakeLists.txt}` | static selectable drivers and lookup/group APIs |
| `component/sensors/SensorConfig/{SensorConfig.c,SensorConfig.h,CMakeLists.txt}` | adapters to raw drivers, init/read/deinit lifecycle |
| `component/ui/ButtonManager/{ButtonManager.c,ButtonManager.h,Kconfig.projbuild,CMakeLists.txt}` | ADC/digital button decoding |
| `component/ui/MenuSystem/{MenuSystem.c,MenuSystem.h,CMakeLists.txt}` | dynamic sensor menu construction and navigation task |
| `component/ui/ScreenManager/{ScreenManager.c,ScreenManager.h,CMakeLists.txt}` | OLED mutex/render tasks/pages |

## Drivers

| Exact path(s) | Core responsibility / I/O |
|---|---|
| `component/drivers/ads1115/{ads1115.c,include/ads1115.h,CMakeLists.txt}` | ADS1115 I2C ADC configuration, reads and conversion |
| `component/drivers/AHT10/{aht.c,aht.h,CMakeLists.txt}` | AHT I2C descriptor/status/temperature-humidity |
| `component/drivers/BME280/{bme280.c,bme280.h,Kconfig.projbuild,CMakeLists.txt}` | BME wrapper/probe and measurements over BMP base |
| `component/drivers/BMP280/{bmp280.c,bmp280.h,CMakeLists.txt}` | BMP/BME calibration/configuration/compensation core |
| `component/drivers/DHT11/{dht.c,dht.h,CMakeLists.txt}` | timing-critical GPIO DHT reads and CRC |
| `component/drivers/DS3231/{ds3231.c,ds3231.h,CMakeLists.txt}` | DS3231 I2C RTC/time/alarm/temp |
| `component/drivers/HTU21D/{si7021.c,si7021.h,CMakeLists.txt}` | Si7021/HTU I2C measure/CRC/configuration |
| `component/drivers/i2cdev/{i2cdev.c,i2cdev.h,CMakeLists.txt}` | shared legacy I2C config/locking/read/write |
| `component/drivers/MH-Z14A/{mhz14a.c,mhz14a.h,Kconfig.projbuild,CMakeLists.txt}` | CO2 UART/PWM protocol and calibration |
| `component/drivers/PMS7003/{pms7003.c,pms7003.h,CMakeLists.txt}` | particulate UART command/frame parsing |
| `component/drivers/pcf8574/{pcf8574.c,include/pcf8574.h,Kconfig.i2c,CMakeLists.txt}` | I2C GPIO expander descriptors/port I/O |
| `component/drivers/SD_Card/{SD_Card.c,SD_Card.h,Kconfig.projbuild,CMakeLists.txt}` | SPI SD mount/FAT file helpers |
| `component/drivers/ssd1306/{ssd1306.c,ssd1306.h,ssd1306_fonts.c,ssd1306_fonts.h,CMakeLists.txt}` | SSD1306 framebuffer primitives and font resources |
| `component/drivers/esp_idf_lib_helpers/{esp_idf_lib_helpers.h,ets_sys.h}` | SDK/target compile-time compatibility headers |
| `component/drivers/webSocket/{esp_websocket_client.c,esp_websocket_client.h,CMakeLists.txt}` | vendored WebSocket client used by WSHandle |

Each CMake/Kconfig file named above is part of this atlas: it declares build dependency/configuration behavior for the sibling implementation. For cross-file change recipes see the canonical subsystem pages, not this navigation inventory.