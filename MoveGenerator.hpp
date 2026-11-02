#ifndef MOVEGENERATOR
#define MOVEGENERATOR

#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include "AttackTables.hpp"
#include "BoardState.hpp"

class MoveGenerator {
public:
	MoveList generateLegalMoves(const Position& pos);
	MoveList generateLegalMovesForSquare(const Position& pos, Square sq);
};

#endif