#include "LegalMoveGenerator.hpp"

#include "DataTypes.hpp"
#include "Bitboard.hpp"

#include "PseudoLegalMoveGenerator.hpp"
#include "AttackDetector.hpp"


//pseudo-legal castling only checks rights and empty squares: the king may not
//castle out of check or through an attacked square (the destination is covered by the normal check test)
static bool isCastlingThroughCheck(Move m, const Position& pos) {
	if (!(m & (static_cast<Move>(1) << 12)))
		return false;
	const Square source = m & 0x3f;
	const Square destination = (m >> 6) & 0x3f;
	const Square passed = destination > source ? source + 1 : source - 1;
	return isSquareAttacked(source, !pos.side_to_move, pos)
		|| isSquareAttacked(passed, !pos.side_to_move, pos);
}

MoveList LegalMoveGenerator::generateLegalMovesForSquare(const Position& pos, Square sq, BoardState& board_state) {

	MoveList legal_moves{};
	MoveList move_candidates = getPseudoLegalMovesAtSquare(pos, sq);

	//pos can be board_state's own position, which makeMove changes
	const Color moving_side = pos.side_to_move;

	for(int i = 0; i < move_candidates.count; ++i) {
		if (isCastlingThroughCheck(move_candidates.moves[i], pos))
			continue;
		board_state.makeMove(move_candidates.moves[i]);

		if(! isInCheck(moving_side, board_state.getPosition())) {
			legal_moves.moves[legal_moves.count++] = move_candidates.moves[i];
		}

		board_state.unmakeMove();
	}

	return legal_moves;
}

MoveList LegalMoveGenerator::generateLegalMovesInPosition(const Position& pos, BoardState& board_state) {

	MoveList legal_moves{};
	MoveList move_candidates = getPseudoLegalMovesInPosition(pos);
	
	//pos can be board_state's own position, which makeMove changes
	const Color moving_side = pos.side_to_move;

	for(int i = 0; i < move_candidates.count; ++i) {
		if (isCastlingThroughCheck(move_candidates.moves[i], pos))
			continue;
		board_state.makeMove(move_candidates.moves[i]);

		if(! isInCheck(moving_side, board_state.getPosition())) {
			legal_moves.moves[legal_moves.count++] = move_candidates.moves[i];
		}

		board_state.unmakeMove();
	}

	return legal_moves;
}
