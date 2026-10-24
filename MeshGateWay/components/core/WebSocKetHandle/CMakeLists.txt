idf_component_register(SRCS "WSHandle.c"
                             "ws_transport.c"
                             "ws_protocol.c"
                             "ws_time_sync.c"
                             "ws_ota.c"
                       INCLUDE_DIRS "."
                       REQUIRES webSocket Datamanager
                       PRIV_REQUIRES WifiManager json mdns FOTAManager UartToNode DS3231 esp_netif esp_wifi)
