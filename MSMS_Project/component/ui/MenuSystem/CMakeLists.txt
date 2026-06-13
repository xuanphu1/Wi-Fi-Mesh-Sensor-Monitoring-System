idf_component_register(
    SRCS "MenuSystem.c"
    INCLUDE_DIRS "."
    REQUIRES "ssd1306" "DataManager" "ButtonManager" "FunctionManager" "WifiManager" "BitManager" "ScreenManager" "SensorRegistry"
)
