idf_component_register(SRCS "MeshManager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES espressif__mesh_lite espressif__iot_bridge DataManager SensorRegistry
                    )
