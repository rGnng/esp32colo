cmake_minimum_required(VERSION 3.16)

set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(coloplay_lcd_probe)

if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "This diagnostic uses the ColoPLAY V1 ESP32-S3 pinout")
endif()
if(NOT CONFIG_APP_BUILD_TYPE_RAM OR NOT CONFIG_APP_BUILD_TYPE_PURE_RAM_APP)
    message(FATAL_ERROR "This diagnostic must be built as a pure RAM application")
endif()
