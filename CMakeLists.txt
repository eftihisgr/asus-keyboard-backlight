# Author:      Eftychios Papasifakis
# File:        CMakeLists.txt
# Project:     ASUS Keyboard Backlight (asus-keyboard-backlight)
# SPDX-License-Identifier: MIT
#
# Description:
#   Build configuration with these targets
#     kbd                      C library that talks to the driver
#     asus-kbd-backlight       command line tool in C
#     asus-kbd-backlight-gui   Qt6 window in C++ and can be turned off with -DBUILD_GUI=OFF

cmake_minimum_required(VERSION 3.16)
project(asus-keyboard-backlight VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(BUILD_GUI "Build the Qt6 GUI" ON)

# Core library in C that reads and writes the asus-wmi sysfs files
# Both the CLI and the GUI link against it
add_library(kbd STATIC src/kbd.c)
target_include_directories(kbd PUBLIC src)
target_compile_options(kbd PRIVATE -Wall -Wextra)

# Command line tool in C
add_executable(asus-kbd-backlight src/cli.c)
target_link_libraries(asus-kbd-backlight PRIVATE kbd)
target_compile_options(asus-kbd-backlight PRIVATE -Wall -Wextra)
install(TARGETS asus-kbd-backlight)

# GUI in C++ with Qt6 plus its app menu entry
if(BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_executable(asus-kbd-backlight-gui src/gui/main.cpp)
    target_link_libraries(asus-kbd-backlight-gui PRIVATE kbd Qt6::Widgets)
    target_compile_options(asus-kbd-backlight-gui PRIVATE -Wall -Wextra)
    install(TARGETS asus-kbd-backlight-gui)
    install(FILES data/asus-keyboard-backlight.desktop DESTINATION share/applications)
endif()

# udev rule that lets normal users write the keyboard backlight files
# It uses an absolute path because udev only reads rules from /etc and /usr
install(FILES udev/99-asus-kbd-backlight.rules DESTINATION /etc/udev/rules.d)
