# Architecture Overview

## Layered Architecture
MRS_Project follows a layered architecture with clear domain separation:

1. **Core Layer**
   - Business logic and data management
   - Components: DataManager, FunctionManager

2. **UI Layer**
   - User interface and navigation
   - Components: MenuSystem, ScreenManager, ButtonManager

3. **Driver Layer**
   - Hardware abstraction
   - Components: I2C drivers, sensor interfaces

4. **Network Layer**
   - Wi-Fi and mesh networking
   - Web configuration interface

## Component Diagram
Reference diagrams:
- High-level: `/component_diagram.puml`
- Detailed: `/component_diagram_detailed.puml`

## Initialization Flow
(from `/main/main.c`)
```
NVS flash → LED RGB → ButtonManager → I2C → OLED → ScreenManager → MenuSystem → Tasks
```

## Key Architectural Patterns
1. **Registry Pattern** (Sensors)
   - Central registry for sensor management
   - Flexible addition/removal of sensors

2. **Callback System**
   - FunctionManager handles menu actions
   - Decouples UI from business logic

3. **State Management**
   - DataManager maintains global application state
   - ScreenState_t for UI rendering

## Dependencies
- ESP-IDF v5.2.5
- Components in `/component/` directory
- Managed components in `/managed_components/`