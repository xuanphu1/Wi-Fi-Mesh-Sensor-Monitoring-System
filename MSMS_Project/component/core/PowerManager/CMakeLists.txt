idf_component_register(
    SRCS "PowerManager.c"
    INCLUDE_DIRS "."
    REQUIRES "PinManager" "driver" "esp_timer" "freertos"
)
