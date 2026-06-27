idf_component_register(SRCS "pcf8574.c"
                    INCLUDE_DIRS "include"
                    REQUIRES i2cdev log esp_idf_lib_helpers)
