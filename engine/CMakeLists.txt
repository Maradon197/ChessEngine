add_library(ChessEngineLib STATIC
    EngineTypes.hpp
    eval/principles/Principles.hpp
    eval/principles/PrincipleRegistry.hpp
    eval/principles/OpeningPrinciples.cpp
)

target_include_directories(ChessEngineLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/eval/principles
)

target_link_libraries(ChessEngineLib PUBLIC ChessCore)
