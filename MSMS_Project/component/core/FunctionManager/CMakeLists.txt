idf_component_register(
    SRCS "FunctionManager.c"
    INCLUDE_DIRS "."
    REQUIRES UartToGateWay DataManager ErrorCodes BatteryManager MeshManager InternetManager WifiManager SensorManager SensorRegistry SensorScanner driver
)
