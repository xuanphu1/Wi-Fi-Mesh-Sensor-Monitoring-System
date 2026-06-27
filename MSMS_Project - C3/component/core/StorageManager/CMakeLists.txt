idf_component_register(
    SRCS "StorageManager.c"
    INCLUDE_DIRS "."
    REQUIRES SD_Card TimeManager DataManager
)
