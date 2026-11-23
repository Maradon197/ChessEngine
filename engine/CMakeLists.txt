add_library(ChessEngine STATIC
    EngineTypes.hpp
    eval/Evaluator.hpp                              eval/Evaluator.cpp
    eval/principles/PrincipleFunctions.hpp
    eval/principles/PrincipleRegistry.hpp           eval/principles/PrincipleRegistry.cpp
    eval/principles/DevelopmentPrinciples.cpp
    eval/principles/CenterPrinciples.cpp
    eval/principles/PawnStructurePrinciples.cpp
    eval/principles/PieceActivityPrinciples.cpp
    eval/principles/AttackPrinciples.cpp
    eval/principles/EndgamePrinciples.cpp
    search/Searcher.hpp                             search/Searcher.cpp
    search/SearchObserver.hpp
 "search/Searcher.cpp")

target_include_directories(ChessEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/eval
    ${CMAKE_CURRENT_SOURCE_DIR}/eval/principles
    ${CMAKE_CURRENT_SOURCE_DIR}/search
)

target_link_libraries(ChessEngine PUBLIC ChessCore)
