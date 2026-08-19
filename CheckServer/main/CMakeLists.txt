idf_component_register(SRCS "CheckServer.c"
                    INCLUDE_DIRS "."
                    REQUIRES webSocket ssd1306 driver esp_wifi esp_event nvs_flash mbedtls json esp-tls)



