#ifndef PSEUDOLEGALMOVEGENERATOR
#define PSEUDOLEGALMOVEGENERATOR

#include "DataTypes.h"
#include "Bitboard.h"
#include "AttackTables.h"
#include "BoardState.h"

class PseudoLegalMoveGenerator {
public:
	MoveList getPseudoLegalMoves(const Position& pos);
	MoveList getPseudoLegalMovesForPiece(Square sq, const Position& pos);
};

#endif
