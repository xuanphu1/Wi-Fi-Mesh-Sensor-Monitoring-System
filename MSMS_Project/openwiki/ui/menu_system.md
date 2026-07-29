# Menu System

## Overview
The MenuSystem (`/component/ui/MenuSystem/`) provides hierarchical navigation on the OLED display.

## Key Features
- Dynamic menu generation from SensorRegistry
- Pagination for long lists
- Callback system for menu actions

## Data Structures
```c
typedef struct {
    const char *title;
    menu_item_t *items;
    uint8_t item_count;
    uint8_t selected_index;
} menu_t;

typedef struct {
    const char *text;
    menu_callback_t callback;
    void *user_data;
} menu_item_t;
```

## Main Components
1. **MenuNavigation_Task**
   - Handles button input for navigation
   - Manages current menu state

2. **Menu Rendering**
   - Uses ScreenManager for display
   - Shows current selection and scrolls long lists

3. **Sensor Integration**
   - Automatically creates menu items from registered sensors
   - Calls sensor read functions when selected

## Usage Example
```c
// Initialize menu system
MenuSystemInit();

// Create navigation task
xTaskCreate(MenuNavigation_Task, "MenuNav", 4096, NULL, 5, NULL);
```