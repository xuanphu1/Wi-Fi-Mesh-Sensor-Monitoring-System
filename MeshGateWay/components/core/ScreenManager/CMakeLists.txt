idf_component_register(
    SRCS "ScreenManager.c"
         "screen_driver.c"
         "screen_renderer.c"
         "screen_dashboard.c"
    INCLUDE_DIRS "."
    REQUIRES Datamanager
    PRIV_REQUIRES UartToNode esp_lcd esp_timer WifiManager PowerManager MemoryManager WebSocKetHandle xpt2046_soft LinkListData FOTAManager app_update esp_netif
)
