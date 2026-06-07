idf_component_register(
    SRCS "UartToNode.c"
    INCLUDE_DIRS "."
    REQUIRES Datamanager driver esp_timer json
)
