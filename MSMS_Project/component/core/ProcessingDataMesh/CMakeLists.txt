idf_component_register(
    SRCS "ProcessingDataMesh.c"
    INCLUDE_DIRS "."
    REQUIRES DataManager ErrorCodes esp_timer json
)
