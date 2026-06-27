idf_component_register(
    SRCS "FunctionManager.c"
    INCLUDE_DIRS "."
    REQUIRES "UartToGateWay" "ScreenManager" "DataManager" "BitManager" "SensorConfig" "SensorRegistry" "ErrorCodes" "BatteryManager" "MeshManager" "InternetManager"
)
