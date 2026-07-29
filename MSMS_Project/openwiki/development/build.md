# Build System

## Overview
The project uses ESP-IDF's CMake-based build system.

## Key Files
- `/CMakeLists.txt`: Main project configuration
- `/main/CMakeLists.txt`: Application component
- `/component/*/CMakeLists.txt`: Component-specific

## Build Steps
1. Set up ESP-IDF v5.2.5
2. Configure via menuconfig:
```
idf.py menuconfig
```
3. Build and flash:
```
idf.py build flash
```

## Partition Table
- OTA dual partition
- SPIFFS for web interface
- Defined in `/partitions.csv`

## Adding Components
1. Create directory in `/component/`
2. Add CMakeLists.txt
3. Register in main CMakeLists.txt

## Coding Conventions
- Module prefixes (e.g., `MenuSystem_`)
- Error codes by module
- Header guards with path