# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
# PlatformIO needs the full component graph (incl. __pio_env) for its CMake code model.
if(NOT BUILD_WITH_PLATFORMIO)
    idf_build_set_property(MINIMAL_BUILD ON)
endif()
project(dji_camera_bluetooth_control)
