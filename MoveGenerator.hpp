#ifndef MOVEGENERATOR
#define MOVEGENERATOR

#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include "AttackTables.hpp"
#include "BoardState.hpp"

class MoveGenerator {
public:
	MoveList generateMoves(const Position& pos);
	MoveList generateMovesForPiece(Square sq, const Position& pos);
};

#endif