idf_component_register(
    SRCS "UartToGateWay.c"
    INCLUDE_DIRS "."
    REQUIRES DataManager ErrorCodes FunctionManager InternetManager MeshManager PinManager ProcessingDataMesh driver
)
