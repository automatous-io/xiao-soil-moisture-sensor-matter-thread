# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(NOT "${IDF_TARGET}" STREQUAL "esp32c6" AND NOT "${IDF_TARGET}" STREQUAL "")
    message(FATAL_ERROR "Unsupported IDF_TARGET: this firmware targets the XIAO ESP32-C6 only")
endif()

set(PROJECT_VER "0.3.0")
set(PROJECT_VER_NUMBER 768)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(soil-sensor)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
# For RISCV chips, project_include.cmake sets -Wno-format, but does not clear various
# flags that depend on -Wformat
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)
