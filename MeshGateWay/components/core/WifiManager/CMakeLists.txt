set(SOURCES
    wifi_manager_core.c
    wifi_manager_ap.c
    wifi_manager_http.c
    wifi_manager_tasks.c
)


idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_http_server esp_system spiffs json Datamanager nvs_flash mdns
    )
