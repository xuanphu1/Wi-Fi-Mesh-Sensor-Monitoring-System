idf_component_register(
    SRCS "TimeManager.c"
    INCLUDE_DIRS "."
    REQUIRES DS3231 PinManager
)
