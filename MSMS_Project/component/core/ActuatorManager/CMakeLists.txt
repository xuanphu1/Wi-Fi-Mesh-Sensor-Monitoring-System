idf_component_register(
    SRCS "ActuatorManager.c"
    INCLUDE_DIRS "."
    REQUIRES "ErrorCodes" "DataManager" "driver" "log"
)
