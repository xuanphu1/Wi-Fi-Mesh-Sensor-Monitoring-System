idf_component_register(
    SRCS "SystemMonitor.c"
    INCLUDE_DIRS "."
    REQUIRES Datamanager SD_Card PowerManager driver
)
