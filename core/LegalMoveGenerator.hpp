#ifndef LEGALMOVEGENERATOR
#define LEGALMOVEGENERATOR

#include "DataTypes.hpp"
#include "Bitboard.hpp"

#include "AttackTables.hpp"
#include "BoardState.hpp"
class LegalMoveGenerator {
public:
	MoveList generateLegalMovesInPosition(const Position& pos, BoardState& board_state);
	MoveList generateLegalMovesForSquare(const Position& pos, Square sq, BoardState& board_state);
};
#endif
