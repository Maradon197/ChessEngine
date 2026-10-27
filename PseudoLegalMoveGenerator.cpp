#include "DataTypes.h"
#include "PseudoLegalMoveGenerator.h"
#include "AttackTables.h"
#include "Bitboard.h"

//the moves generated here are PSEUDO legal. This means:
//We account for: 			piece movement rules, board boundaries & occupancy, 
//							general castling rights, en passant
//We do not account for: 	(double) checks, pins, ...

MoveList PseudoLegalMoveGenerator::getMovesAtPosition(const Position& pos, Square source) {
	MoveList pseudo_legal_piece_moves{};

	return pseudo_legal_piece_moves;
}

MoveList PseudoLegalMoveGenerator::getAllMovesForPieceType(const Position& pos, PieceType pieceType) {
	Bitboard side_to_moves_pieces = pos.pieces[pos.side_to_move][pieceType];
	MoveList pseudo_legal_piece_moves{};

	switch(pieceType) {
		case PieceType::PAWN:
		//TODO: use AttackTables to generate pawn attacks! (no color distinction necessary, color is passable)
		if(pos.side_to_move == WHITE) {

			Bitboard currentSquare;
			while(side_to_moves_pieces) {
				Move current_move_candidate{};

				//get current square and consume it
				currentSquare = BB::getLSBSquareBitboard(side_to_moves_pieces);
				side_to_moves_pieces &= !currentSquare;


				if(pos.total_occupancy & currentSquare << 8) //is square in front of pawn occupied: no move allowed
					continue;

				else {//is square empty: allow move
					//TODO: build correct move encoding!
					pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;

					if(currentSquare & (0b11111111 << 8) && 
						!(pos.total_occupancy & (currentSquare << 16) )) {//is on 2nd rank: allow double move if not occupied!
						//TODO: build correct move encoding!
						pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;
					}
				}
			}
		}
		else {
			//TODO: black pawns mirror logic
		}

		break;

		case PieceType::KNIGHT:
		while(side_to_moves_pieces) {
			Move current_move_candidate;

			//get current square and consume it
			Square current_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(side_to_moves_pieces));
			side_to_moves_pieces &= !BB::getLSBSquareBitboard(side_to_moves_pieces);

			Bitboard attackPattern = getKnightAttacks(current_square);

			//TODO: intersect with !(own occupancy)
			//		add corresponding moves to the remaining squares to move list
		}
		case PieceType::BISHOP:
		//TODO
		case PieceType::ROOK:
		//TODO
		case PieceType::QUEEN:
		//TODO
		case PieceType::KING:
		//TODO
		default:
		break;

	}

	return pseudo_legal_piece_moves;
}


MoveList PseudoLegalMoveGenerator::getPseudoLegalMoves(const Position& pos) {
	MoveList pseudoLegalMoves{}; 
	MoveList nextMoveList{};

	//TODO: add all piece moves to the list
	//nextMoveList = ...
	for(std::size_t i = 0; i < nextMoveList.count; ++i) {
		pseudoLegalMoves.moves[pseudoLegalMoves.count++] = nextMoveList.moves[i];
	}

	return pseudoLegalMoves;
}