#ifndef MOVEGENERATOR
#define MOVEGENERATOR

#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include "AttackTables.hpp"
#include "BoardState.hpp"

MoveList generateLegalMovesInPosition(const Position& pos, BoardState& board_state);
MoveList generateLegalMovesForSquare(const Position& pos, Square sq, BoardState& board_state);

#endif