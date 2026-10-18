idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES spiffs nvs_flash InternetManager DataManager ButtonManager MenuSystem BatteryManager DS3231 SD_Card PinManager PowerManager pcf8574 TimeManager StorageManager FOTAManager MeshManager FunctionManager
                    )
