#ifndef MOVEGENERATOR
#define MOVEGENERATOR

#include "DataTypes.h"
#include "Bitboard.h"
#include "AttackTables.h"
#include "BoardState.h"

class MoveGenerator {
public:
	MoveList generateMoves(const Position& pos);
	MoveList generateMovesForPiece(Square sq, const Position& pos);
};

#endif