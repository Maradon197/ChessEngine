#include "DataTypes.hpp"
#include "MoveGenerator.hpp"
#include "PseudoLegalMoveGenerator.hpp"
#include "AttackDetector.hpp"
#include "Bitboard.hpp"

//MASSIVE TODO: Scan Castling moves for checks!

MoveList generateLegalMovesForSquare(const Position& pos, Square sq, BoardState& board_state) {

	MoveList legal_moves{};
	MoveList move_candidates = getPseudoLegalMovesAtSquare(pos, sq);

	for(int i = 0; i < move_candidates.count; ++i) {
		board_state.makeMove(move_candidates.moves[i]);

		if(! isInCheck(pos.side_to_move, board_state.getPosition())) {
			legal_moves.moves[legal_moves.count++] = move_candidates.moves[i];
		}

		board_state.unmakeMove();
	}

	return move_candidates;
}

MoveList generateLegalMovesInPosition(const Position& pos, BoardState& board_state) {

	MoveList legal_moves{};
	MoveList move_candidates = getPseudoLegalMovesInPosition(pos);
	
	for(int i = 0; i < move_candidates.count; ++i) {
		board_state.makeMove(move_candidates.moves[i]);

		if(! isInCheck(pos.side_to_move, board_state.getPosition())) {
			legal_moves.moves[legal_moves.count++] = move_candidates.moves[i];
		}

		board_state.unmakeMove();
	}

	return move_candidates;
}