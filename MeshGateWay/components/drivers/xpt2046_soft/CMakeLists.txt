idf_component_register(SRCS "xpt2046_soft.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_common soft_spi)
