add_library(ChessUtil STATIC
    MoveNotation.hpp            MoveNotation.cpp
)

target_include_directories(ChessUtil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ChessUtil PUBLIC ChessCore)
