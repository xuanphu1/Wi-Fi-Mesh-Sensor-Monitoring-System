if(CONFIG_MSMS_ENABLE_TESTS)
file(TO_CMAKE_PATH "$ENV{IDF_PATH}" IDF_PATH_CMAKE)
idf_component_register(
    SRCS "app_test.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS
        "../../sensors/SensorRegistry/internal"
        "../../sensors/SensorRegistry/include"
        "../../sensors/SensorManager"
        "../../sensors/SensorScanner"
        "../../sensors/SensorTypes"
        "../../core/DataManager"
        "../../core/FunctionManager"
        "../../core/ActuatorManager"
        "../../core/BatteryManager"
        "../../core/UartToGateWay"
        "../../core/UartResourceManager/include"
        "../../core/PinManager"
        "../../network/InternetManager"
        "../../network/MeshManager/include"
        "../../network/WifiManager"
        "../../ui/ScreenManager"
        "../../utils/ErrorCodes"
        "../../utils/BitManager"
        "${IDF_PATH_CMAKE}/components/esp_wifi/include"
        "${IDF_PATH_CMAKE}/components/esp_event/include"
        "${IDF_PATH_CMAKE}/components/esp_netif/include"
        "${IDF_PATH_CMAKE}/components/driver/uart/include"
        "${IDF_PATH_CMAKE}/components/driver/gpio/include"
        "${IDF_PATH_CMAKE}/components/driver/i2c/include"
        "${IDF_PATH_CMAKE}/components/driver/spi/include"
        "${IDF_PATH_CMAKE}/components/driver/include"
        "${IDF_PATH_CMAKE}/components/esp_ringbuf/include"
    REQUIRES
        DataManager ErrorCodes SensorTypes SensorRegistry SensorManager SensorScanner FunctionManager ActuatorManager UartToGateWay UartResourceManager ScreenManager BatteryManager InternetManager MeshManager WifiManager BitManager PinManager esp_wifi esp_event esp_netif nvs_flash driver freertos esp_ringbuf
)
else()
idf_component_register(
    INCLUDE_DIRS "."
)
endif()
