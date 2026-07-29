# Sensor Registry System

## Overview
The sensor registry pattern provides a flexible way to manage multiple sensors with a unified interface.

## Key Features
- Central registry for all sensors
- Standardized interface for reading data
- Automatic menu generation

## Registration Flow
1. Sensor implements required functions
2. Registers with SensorRegistry
3. Appears automatically in UI menu

## Data Flow
```
Sensor → Registry → DataManager → ScreenManager → OLED
```

## Adding a New Sensor
1. Implement sensor interface:
   - `init()`
   - `read()`
   - `get_name()`
   - `get_unit()`

2. Register in initialization:
```c
SensorRegistry_Add(&my_sensor_interface);
```

3. Data appears in:
   - Main menu
   - ScreenManager rotation

## Supported Sensors
- BME280 (Temp/Humidity/Pressure)
- BMP280 (Temp/Pressure)
- DS3231 (RTC)

## Error Handling
- Unified `system_err_t` codes
- Module-specific error ranges