add_library(ChessCore STATIC
    DataTypes.hpp
    Bitboard.hpp
    AttackTables.hpp            AttackTables.cpp
    AttackDetector.hpp          AttackDetector.cpp
    BoardState.hpp              BoardState.cpp
    PseudoLegalMoveGenerator.hpp PseudoLegalMoveGenerator.cpp
    LegalMoveGenerator.hpp      LegalMoveGenerator.cpp
    IOHandler.hpp               IOHandler.cpp
    DebugTestStuff.hpp          DebugTestStuff.cpp
)

target_include_directories(ChessCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
