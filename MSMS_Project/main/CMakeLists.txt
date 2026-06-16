idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES spiffs nvs_flash WifiManager InternetManager DataManager ButtonManager MenuSystem BME280 BatteryManager DS3231 SD_Card PinManager UartToGateWay pcf8574 TimeManager StorageManager
                    )
