idf_component_register(
    SRCS "UartToGateWay.c"
    INCLUDE_DIRS "."
    REQUIRES DataManager ErrorCodes InternetManager MeshManager PinManager ProcessingDataMesh TimeManager json driver FOTAManager
)
