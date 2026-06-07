idf_component_register(
    SRCS "MeshGateWay.c"
    INCLUDE_DIRS "."
    REQUIRES Datamanager PowerManager SystemMonitor ScreenManager SD_Card UartToNode
             nvs_flash spiffs WifiManager WebSocKetHandle
)
