# MRS_Project Quickstart

## Overview
MRS_Project is an ESP-IDF application for ESP32-C6 (and other ESP32 series) with:
- **UI System**: OLED SSD1306 display with hierarchical menu
- **Sensor Management**: Flexible sensor registry pattern
- **Wi-Fi**: AP/STA with web configuration interface
- **Network**: Mesh networking (in development)
- **Hardware**: WS2812B RGB LED, DS3231 RTC, I2C sensors (BME280, BMP280)

## Key Components
1. **Core** (`/component/core/`)
   - DataManager: Global state management
   - FunctionManager: Callback and business logic

2. **UI** (`/component/ui/`)
   - MenuSystem: Hierarchical navigation
   - ScreenManager: OLED display rendering
   - ButtonManager: Input handling

3. **Sensors**
   - Registry pattern for flexible sensor management

4. **Network**
   - Wi-Fi AP/STA with captive portal
   - Web configuration interface (SPIFFS)

## Getting Started
1. **Setup**
   - ESP-IDF v5.2.5
   - Target: ESP32-C6
   - Build system: CMake

2. **Initialization Flow** (from `/main/main.c`)
   - NVS flash → LED RGB → ButtonManager → I2C → OLED → ScreenManager → MenuSystem → Tasks

3. **First Run**
   - Configure Wi-Fi via web interface
   - Navigate menu to view sensor data

## Next Steps
- [Architecture Overview](architecture/overview.md)
- [UI System Details](ui/)
- [Sensor Management](sensors/)
- [Network Configuration](network/)