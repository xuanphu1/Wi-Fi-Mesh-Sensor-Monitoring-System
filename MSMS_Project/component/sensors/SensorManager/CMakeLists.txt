idf_component_register(
    SRCS "SensorManager.c"
    INCLUDE_DIRS "."
    REQUIRES DataManager ErrorCodes SensorTypes SensorRegistry freertos log
)
