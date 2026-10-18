idf_component_register(
    SRCS "MenuSystem.c"
    INCLUDE_DIRS "."
    REQUIRES DataManager ButtonManager FunctionManager ScreenManager
)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-missing-field-initializers")
