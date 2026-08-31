idf_component_register(
    SRCS "FOTAManager.c"
    INCLUDE_DIRS "."
    REQUIRES DataManager InternetManager WifiManager MeshManager espressif__mesh_lite esp_http_client esp_https_ota app_update esp_wifi json mbedtls nvs_flash ScreenManager
)
