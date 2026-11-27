#ifndef LEGALMOVEGENERATOR
#define LEGALMOVEGENERATOR

#include "DataTypes.hpp"
#include "Bitboard.hpp"

#include "AttackTables.hpp"
#include "BoardState.hpp"
//pseudo-legal castling only checks rights and empty squares: true if castling move m
//starts in or passes through check in pos (the position BEFORE m is made)
bool isCastlingThroughCheck(Move m, const Position& pos);

class LegalMoveGenerator {
public:
	MoveList generateLegalMovesInPosition(const Position& pos, BoardState& board_state);
	MoveList generateLegalMovesForSquare(const Position& pos, Square sq, BoardState& board_state);
};
#endif
