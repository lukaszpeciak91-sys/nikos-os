cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "0.2.0")
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/apps/radiolab"
    "${CMAKE_CURRENT_LIST_DIR}/apps/communicator"
    "${CMAKE_CURRENT_LIST_DIR}/apps/power_diag_app"
    "${CMAKE_CURRENT_LIST_DIR}/apps/flashlight"
    "${CMAKE_CURRENT_LIST_DIR}/apps/sound_meter"
    "${CMAKE_CURRENT_LIST_DIR}/apps/snake"
    "${CMAKE_CURRENT_LIST_DIR}/apps/mini_racer"
    "${CMAKE_CURRENT_LIST_DIR}/apps/facts"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nikos_os)
