# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/component/core
    ${CMAKE_CURRENT_LIST_DIR}/component/ui
    ${CMAKE_CURRENT_LIST_DIR}/component/drivers
    ${CMAKE_CURRENT_LIST_DIR}/component/network
    ${CMAKE_CURRENT_LIST_DIR}/component/sensors
    ${CMAKE_CURRENT_LIST_DIR}/component/utils
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(MRS_Project)
# Add SPIFFS configuration
spiffs_create_partition_image(spiffs component/network/WebConfigWifi FLASH_IN_PROJECT)
