#include "DataTypes.h"
#include "PseudoLegalMoveGenerator.h"
#include "AttackTables.h"
#include "Bitboard.h"

//the moves generated here are PSEUDO legal. This means:
//We account for: 			piece movement rules, board boundaries & occupancy, 
//							general castling rights, en passant
//We do not account for: 	(double) checks, pins, 

MoveList getPawnMoves(const Position& pos, Color side_to_move) {
	Bitboard side_to_moves_pawns = pos.pieces[pos.side_to_move][PAWN];
	MoveList pseudo_legal_pawn_moves{};

	if(pos.side_to_move == WHITE) {

		Bitboard currentSquareBB;
		while(side_to_moves_pawns) {
			Move current_move_candidate;

			//get current square and consume it
			currentSquareBB = BB::getLSBSquareBitboard(side_to_moves_pawns);
			side_to_moves_pawns &= !currentSquareBB;


			if(pos.total_occupancy & currentSquareBB << 8) {//is square in front of pawn occupied: no move allowed
				continue;
			}
			else {//is square empty: allow move
				//TODO: build correct move encoding!
				pseudo_legal_pawn_moves.moves[pseudo_legal_pawn_moves.count++] = current_move_candidate;

				if(currentSquareBB & (0b11111111 << 8) && 
					!(pos.total_occupancy & (currentSquareBB << 16) )) {//is on 2nd rank: allow double move if not occupied!
					//TODO: build correct move encoding!
					pseudo_legal_pawn_moves.moves[pseudo_legal_pawn_moves.count++] = current_move_candidate;
				}
			}
			
		}
	}
	else {
		//TODO: black pawns mirror logic
	}

	//TODO: use AttackTables to generate pawn attacks! (no color distinction necessary, color is passable)

	return pseudo_legal_pawn_moves;
}

MoveList getKnightMoves(const Position& pos, Color side_to_move) {
	MoveList pseudoLegalKnightMoves{};
	//TODO
	return pseudoLegalKnightMoves;

}

MoveList getBishopMoves(const Position& pos, Color side_to_move) {
	MoveList pseudoLegalBishopMoves{};
	//TODO
	return pseudoLegalBishopMoves;
}

MoveList getQueenMoves(const Position& pos, Color side_to_move) {
	MoveList pseudoLegalQueenMoves{};
	//TODO
	return pseudoLegalQueenMoves;
}

MoveList getKingMoves(const Position& pos, Color side_to_move) {
	MoveList pseudoLegalKingMoves{};
	//TODO
	return pseudoLegalKingMoves;
}

MoveList PseudoLegalMoveGenerator::getPseudoLegalMoves(const Position& pos) {
	MoveList pseudoLegalMoves{}; 
	MoveList nextMoveList{};

	//add pawn moves to the list
	nextMoveList = getPawnMoves(pos, pos.side_to_move);
	for(std::size_t i = 0; i < nextMoveList.count; ++i) {
		pseudoLegalMoves.moves[pseudoLegalMoves.count++] = nextMoveList.moves[i];
	}

	//TODO: all pieces
	return pseudoLegalMoves;
}