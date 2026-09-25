set(app_src ip5306.c)
set(pre_req i2cdev log nvs_flash esp_idf_lib_helpers PinManager)
idf_component_register(SRCS ${app_src}
                       INCLUDE_DIRS "."
                       REQUIRES ${pre_req})
