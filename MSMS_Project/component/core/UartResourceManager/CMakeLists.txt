idf_component_register(
    SRCS "UartResourceManager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer ErrorCodes freertos esp_ringbuf
)
