idf_component_register(
    SRCS "UartToGateWay.c"
    INCLUDE_DIRS "."
    REQUIRES DataManager ErrorCodes FunctionManager InternetManager PinManager ProcessingDataMesh driver
)
