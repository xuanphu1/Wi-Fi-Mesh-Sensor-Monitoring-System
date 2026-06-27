idf_component_register(
    SRCS "SensorConfig.c"
    INCLUDE_DIRS "."
    REQUIRES "SensorTypes" "BME280" "ErrorCodes" "AHT10" "PMS7003" "PinManager" "MH-Z14A" "HTU21D" "DHT11"
)


