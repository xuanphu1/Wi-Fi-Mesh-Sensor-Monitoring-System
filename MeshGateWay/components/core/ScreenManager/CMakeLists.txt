idf_component_register(
    SRCS "ScreenManager.c"
    INCLUDE_DIRS "."
    REQUIRES Datamanager UartToNode ui lvgl lvgl_esp32_drivers esp_timer WifiManager PowerManager MemoryManager
)
