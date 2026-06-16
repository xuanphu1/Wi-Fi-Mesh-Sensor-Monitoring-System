idf_component_register(
    SRCS "MenuSystem.c"
    INCLUDE_DIRS "."
    REQUIRES "ssd1306" "DataManager" "ButtonManager" "FunctionManager" "WifiManager" "InternetManager" "BitManager" "ScreenManager" "SensorRegistry"
)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-missing-field-initializers")
