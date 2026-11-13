cmake_minimum_required(VERSION 3.19)
project(ChessEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# core:    Qt-free engine library (board, move generation, ...)
# console: the console test program
add_subdirectory(core)
add_subdirectory(console)
