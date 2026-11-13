#include "LegalMoveGenerator.hpp"

#include "DataTypes.hpp"
#include "Bitboard.hpp"

#include "PseudoLegalMoveGenerator.hpp"
#include "AttackDetector.hpp"

//MASSIVE TODO: Scan Castling moves for checks in the way!

MoveList LegalMoveGenerator::generateLegalMovesForSquare(const Position& pos, Square sq, BoardState& board_state) {

	MoveList legal_moves{};
	MoveList move_candidates = getPseudoLegalMovesAtSquare(pos, sq);

	for(int i = 0; i < move_candidates.count; ++i) {
		board_state.makeMove(move_candidates.moves[i]);

		if(! isInCheck(pos.side_to_move, board_state.getPosition())) {
			legal_moves.moves[legal_moves.count++] = move_candidates.moves[i];
		}

		board_state.unmakeMove();
	}

	return legal_moves;
}

MoveList LegalMoveGenerator::generateLegalMovesInPosition(const Position& pos, BoardState& board_state) {

	MoveList legal_moves{};
	MoveList move_candidates = getPseudoLegalMovesInPosition(pos);
	
	for(int i = 0; i < move_candidates.count; ++i) {
		board_state.makeMove(move_candidates.moves[i]);

		if(! isInCheck(pos.side_to_move, board_state.getPosition())) {
			legal_moves.moves[legal_moves.count++] = move_candidates.moves[i];
		}

		board_state.unmakeMove();
	}

	return legal_moves;
}
