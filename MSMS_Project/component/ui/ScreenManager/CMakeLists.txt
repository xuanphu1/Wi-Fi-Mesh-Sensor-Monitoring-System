idf_component_register(
    SRCS "ScreenManager.c"
    INCLUDE_DIRS "."
    REQUIRES "ssd1306" "DataManager" "BitManager" "WifiManager" "ErrorCodes" "MeshManager"
)
