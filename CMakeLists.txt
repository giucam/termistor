
cmake_minimum_required(VERSION 2.8)
project(termistor)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -g -std=c++11")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

include(wayland)

find_package(Qt5Core)
find_package(Qt5Gui)
find_package(PkgConfig "0.22" REQUIRED)

pkg_check_modules(wayland-client wayland-client REQUIRED)
pkg_check_modules(xkbcommon xkbcommon REQUIRED)

set(CMAKE_AUTOMOC ON)
foreach(dir ${Qt5Gui_INCLUDE_DIRS})
    include_directories(${dir}/${Qt5Gui_VERSION_STRING}/QtGui/)
endforeach(dir)
include_directories(${wayland-client_INCLUDE_DIRS} ${xkbcommon_INCLUDE_DIRS}
                    libtsm libtsm/src/tsm libtsm/src/shared)

set(libtsm_SOURCES
    libtsm/src/tsm/tsm-vte.c
    libtsm/src/tsm/tsm-render.c
    libtsm/src/tsm/tsm-screen.c
    libtsm/src/tsm/tsm-selection.c
    libtsm/src/tsm/tsm-unicode.c
    libtsm/src/tsm/tsm-vte-charsets.c
    libtsm/src/shared/shl-htable.c
    libtsm/src/shared/shl-pty.c
    libtsm/src/shared/shl-ring.c
    libtsm/external/wcwidth.c)
add_library(tsm ${libtsm_SOURCES})

set(SOURCES
    src/main.cpp
    src/vte.cpp
    src/terminal.cpp
    src/screen.cpp)

wayland_add_protocol_client(SOURCES
    ${CMAKE_SOURCE_DIR}/protocol/orbital-dropdown.xml
    dropdown
)

add_executable(termistor ${SOURCES})
qt5_use_modules(termistor Gui)
target_link_libraries(termistor util tsm ${wayland-client_LIBRARIES})

install(TARGETS termistor DESTINATION bin)
