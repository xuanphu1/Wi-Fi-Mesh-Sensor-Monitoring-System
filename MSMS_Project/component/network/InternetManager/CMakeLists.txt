idf_component_register(
    SRCS "InternetManager.c"
    INCLUDE_DIRS "."
    REQUIRES DataManager ErrorCodes WifiManager MeshManager esp_wifi esp_netif espressif__iot_bridge
)
