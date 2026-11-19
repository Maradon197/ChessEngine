add_library(ChessEngine STATIC
    EngineTypes.hpp
    eval/principles/PrincipleFunctions.hpp
    eval/principles/PrincipleRegistry.hpp
    eval/principles/PrincipleRegistry.cpp
    eval/principles/OpeningPrinciples.cpp
)

target_include_directories(ChessEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/eval/principles
)

target_link_libraries(ChessEngine PUBLIC ChessCore)
