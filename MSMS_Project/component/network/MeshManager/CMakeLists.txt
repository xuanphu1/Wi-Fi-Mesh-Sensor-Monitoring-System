idf_component_register(
    SRCS
        "MeshManager.c"
        "private/mesh_network.c"
        "private/mesh_stream_parser.c"
        "private/mesh_tcp_transport.c"
        "private/mesh_telemetry.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private"
    REQUIRES espressif__mesh_lite espressif__iot_bridge DataManager SensorRegistry TimeManager json
)
