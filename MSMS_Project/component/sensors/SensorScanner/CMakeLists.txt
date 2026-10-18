set(app_src SensorScanner.c)
set(pre_req SensorTypes SensorRegistry SensorManager DataManager MeshManager espressif__mesh_lite nvs_flash log)
idf_component_register(SRCS ${app_src}
                       INCLUDE_DIRS "."
                       REQUIRES ${pre_req})
