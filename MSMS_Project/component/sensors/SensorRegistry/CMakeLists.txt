idf_component_register(
    SRCS "SensorRegistry.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "internal"
    REQUIRES "SensorTypes" "SensorConfig" "ErrorCodes"
)
