idf_component_register(
    SRCS "ScreenManager.c"
    INCLUDE_DIRS "."
    REQUIRES Datamanager UartToNode esp_lcd esp_timer WifiManager PowerManager MemoryManager WebSocKetHandle xpt2046_soft LinkListData FOTAManager app_update esp_netif
)
