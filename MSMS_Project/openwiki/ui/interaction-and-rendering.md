---
type: user interface subsystem
title: Buttons, menus, and OLED rendering
description: UI input modes, menu callback flow, ScreenManager rendering synchronization, and state dependencies.
tags: [ui, oled, buttons, menu, freertos]
---

# Interaction and rendering

The UI is a three-part pipeline: `ButtonManager` decodes button presses, `MenuSystem` maintains navigation/dispatches callbacks, and `ScreenManager` renders DataManager state to SSD1306. `main` initializes the display before menu/screen objects and starts navigation/render tasks only when display creation succeeds.

Button build configuration chooses either ADC resistor-ladder input (default) or digital inputs. Digital mode checks DOWN, SELECT, BACK with configured active level/debounce; ADC mode averages eight samples and maps ranges to logical buttons. Both emit edge transitions rather than a continuous held key. UI code has no direct sensor protocol call: actions pass through FunctionManager and SensorRegistry.

MenuSystem creates root NetworkCfg, Sensors, Application, Actuators, Battery, and Information entries. It dynamically creates per-port lists categorized UART, I2C, SPI, Pulse from registry entries. Navigation runs at 10 ms: dashboard select/back moves pages, up/down enters menu; after three seconds idle it returns to dashboard. Dynamic sensor menu allocation failure logs but is not propagated.

ScreenManager owns an OLED mutex and a retained `ssd1306_handle_t`. Rendering reads DataManager plus TimeManager, WifiManager, MeshManager and SystemPerfomance to show dashboard, port summaries, performance histories, messages, live sensor values, and root status. Its public rendering APIs return custom errors for uninitialized display, invalid input, mutex acquisition failure or refresh failure; some dashboard helpers are void and silently return. The mutex protects the display, not every I2C device; I2Cdev has separate locking for driver traffic.

Use [Local services](../platform/local-services.md) for hardware dependencies and [Sensors](../hardware/sensors-and-drivers.md) to add a selectable driver.