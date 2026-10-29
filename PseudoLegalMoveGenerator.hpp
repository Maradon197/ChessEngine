#ifndef PSEUDOLEGALMOVEGENERATOR
#define PSEUDOLEGALMOVEGENERATOR

#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include "AttackTables.hpp"
#include "BoardState.hpp"

class PseudoLegalMoveGenerator {
private:
	
	MoveList getAllMovesForPieceType(const Position& pos, PieceType pieceType);
public:
	MoveList getMovesAtPosition(const Position& pos, Square source);

	MoveList getPseudoLegalMoves(const Position& pos);
};

#endif
