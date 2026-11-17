cmake_minimum_required(VERSION 3.19)
project(ChessEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_subdirectory(core)
add_subdirectory(util)

option(CHESS_BUILD_GUI "Build the Qt GUI (needs Qt 6.5 or newer)" ON)
if(CHESS_BUILD_GUI)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
    qt_standard_project_setup()
    add_subdirectory(gui)
    add_subdirectory(app)
endif()
