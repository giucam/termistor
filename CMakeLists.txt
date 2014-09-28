
cmake_minimum_required(VERSION 2.8)
project(termistor)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -g -std=c++11")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

include(wayland)

find_package(Qt5Core)
find_package(Qt5Gui)
find_package(PkgConfig "0.22" REQUIRED)

pkg_check_modules(libtsm libtsm REQUIRED)
pkg_check_modules(wayland-client wayland-client REQUIRED)

set(CMAKE_AUTOMOC ON)
foreach(dir ${Qt5Gui_INCLUDE_DIRS})
    include_directories(${dir}/${Qt5Gui_VERSION_STRING}/QtGui/)
endforeach(dir)

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
target_link_libraries(termistor util ${libtsm_LIBRARIES} ${wayland-client_LIBRARIES})

install(TARGETS termistor DESTINATION bin)
