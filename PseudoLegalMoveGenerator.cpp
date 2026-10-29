#include "DataTypes.hpp"
#include "PseudoLegalMoveGenerator.hpp"
#include "AttackTables.hpp"
#include "Bitboard.hpp"

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
		else {//pos.side_to_move == BLACK
			//TODO: black pawns mirror logic
		}

		break;

		case PieceType::KNIGHT:
		while(side_to_moves_pieces) {
			Move current_move_candidate{};

			//get current square and consume it
			Square current_source_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(side_to_moves_pieces));
			side_to_moves_pieces &= !BB::getLSBSquareBitboard(side_to_moves_pieces);

			//find legal moves: knight cannot capture own pieces
			Bitboard attackPattern = getKnightAttacks(current_source_square);
			Bitboard move_candidates = attackPattern & !(pos.color_occupancy[pos.side_to_move]);

			//convert bitboard to movelist entries
			while(move_candidates) {
				//get current square and consume it
				Square current_destination_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(move_candidates));
				move_candidates &= !(BB::getLSBSquareBitboard(move_candidates));

				current_destination_square & pos.color_occupancy(!pos.side_to_move) ? //placeholders for move encoding so this compiles
					/*moving on opponents piece? TODO: Build move encoding for capture!*/ pos.side_to_move = pos.side_to_move :
					/*moving on emptysquare? TODO: Build move encoding for move!*/ pos.side_to_move = pos.side_to_move ;
				
				pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;
			}
		}
		break;
		case PieceType::BISHOP:
		while(side_to_moves_pieces) {
			Move current_move_candidate{};

			//get current square and consume it
			Square current_source_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(side_to_moves_pieces));
			side_to_moves_pieces &= !BB::getLSBSquareBitboard(side_to_moves_pieces);

			//find legal moves
			Bitboard move_candiates = getBishopAttacks(current_source_square, pos.color_occupancy[pos.side_to_move],
				pos.color_occupancy[!pos.side_to_move]);

			while(move_candiates) {
				Square current_destination_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(move_candidates));
				move_candidates &= !(BB::getLSBSquareBitboard(move_candidates));

				current_destination_square & pos.color_occupancy(!pos.side_to_move) ? //placeholders for move encoding so this compiles
					/*moving on opponents piece? TODO: Build move encoding for capture!*/ pos.side_to_move = pos.side_to_move :
					/*moving on emptysquare? TODO: Build move encoding for move!*/ pos.side_to_move = pos.side_to_move ;
				
				pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;
			}
		}
		break;
		case PieceType::ROOK:
		while(side_to_moves_pieces) {
			Move current_move_candidate{};

			//get current square and consume it
			Square current_source_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(side_to_moves_pieces));
			side_to_moves_pieces &= !BB::getLSBSquareBitboard(side_to_moves_pieces);

			//find legal moves
			Bitboard move_candiates = getRookAttacks(current_source_square, pos.color_occupancy[pos.side_to_move],
				pos.color_occupancy[!pos.side_to_move]);

			while(move_candiates) {
				Square current_destination_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(move_candidates));
				move_candidates &= !(BB::getLSBSquareBitboard(move_candidates));

				current_destination_square & pos.color_occupancy(!pos.side_to_move) ? //placeholders for move encoding so this compiles
					/*moving on opponents piece? TODO: Build move encoding for capture!*/ pos.side_to_move = pos.side_to_move :
					/*moving on emptysquare? TODO: Build move encoding for move!*/ pos.side_to_move = pos.side_to_move ;
				
				pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;
			}
		}
		break;
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