#ifndef PSEUDOLEGALMOVEGENERATOR
#define PSEUDOLEGALMOVEGENERATOR

#include "DataTypes.h"
#include "Bitboard.h"
#include "AttackTables.h"
#include "BoardState.h"

class PseudoLegalMoveGenerator {
private:
	
	MoveList getAllMovesForPieceType(const Position& pos, PieceType pieceType);
public:
	MoveList getMovesAtPosition(const Position& pos, Square source);

	MoveList getPseudoLegalMoves(const Position& pos);
};

#endif
