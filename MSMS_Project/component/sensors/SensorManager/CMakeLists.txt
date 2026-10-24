idf_component_register(
    SRCS "SensorManager.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../SensorRegistry/internal"
    REQUIRES DataManager ErrorCodes SensorTypes SensorRegistry freertos log UartResourceManager
)
