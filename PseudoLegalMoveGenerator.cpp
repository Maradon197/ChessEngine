#include "DataTypes.hpp"
#include "PseudoLegalMoveGenerator.hpp"
#include "AttackTables.hpp"
#include "Bitboard.hpp"

//the moves generated here are PSEUDO legal. This means:
//We account for: 			piece movement rules, board boundaries & occupancy, 
//							general castling rights, en passant
//We do not account for: 	(double) checks, pins, ...

MoveList PseudoLegalMoveGenerator::getMovesAtSquare(const Position& pos, Square source) {
	MoveList pseudo_legal_piece_moves{};
	MoveList all_moves = getPseudoLegalMoves(pos);

	for(std::uint16_t i = 0; i < all_moves.count; ++i) {

		//0x3F position masks out the source from a given move
		if(static_cast<Square>(all_moves.moves[i] & 0x3F) == source) {
			pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = all_moves.moves[i];
		}
	}

	return pseudo_legal_piece_moves;
}

MoveList PseudoLegalMoveGenerator::getAllMovesForPieceType(const Position& pos, PieceType pieceType) {
	Bitboard to_consider_pieces = pos.pieces[pos.side_to_move][pieceType];
	MoveList pseudo_legal_piece_moves{};

	switch(pieceType) {
		case PieceType::PAWN:
		{
			const Bitboard own_occ = pos.color_occupancy[pos.side_to_move];
			const Bitboard enemy_occ = pos.color_occupancy[!pos.side_to_move];
			const bool is_white = (pos.side_to_move == WHITE);

			//rank masks
			constexpr Bitboard RANK_2 = 0x000000000000FF00ULL;
			constexpr Bitboard RANK_4 = 0x00000000FF000000ULL;
			constexpr Bitboard RANK_7 = 0x00FF000000000000ULL;
			constexpr Bitboard RANK_8 = 0xFF00000000000000ULL;

			while(to_consider_pieces) {
				//get current square and consume it
				Bitboard currentSquare = BB::getLSBSquareBitboard(to_consider_pieces);
				to_consider_pieces &= ~currentSquare;
				Square current_source_square = BB::getSquareFromOneSquareBitboard(currentSquare);

				//quiet moves
				Bitboard singlePushBB = is_white ? (currentSquare << 8) : (currentSquare >> 8);
				if(!(pos.total_occupancy & singlePushBB)) { //square in front is empty
					Square dest = BB::getSquareFromOneSquareBitboard(singlePushBB);
					Bitboard promotion_rank = is_white ? RANK_8 : 0x00000000000000FFULL;

					if(singlePushBB & promotion_rank) {
						//promotion
						const PieceType promo_pieces[] = { KNIGHT, BISHOP, ROOK, QUEEN };
						for(PieceType promo : promo_pieces) {
							Move m = static_cast<Move>(current_source_square)
								| (static_cast<Move>(dest) << 6)
								| (static_cast<Move>(1) << 14)
								| (static_cast<Move>(promo) << 15);
							pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
						}
					}
					else {
						Move m = static_cast<Move>(current_source_square)
							| (static_cast<Move>(dest) << 6);
						pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
					}

					//double push
					Bitboard start_rank = is_white ? RANK_2 : RANK_7;
					if(currentSquare & start_rank) {
						Bitboard doublePushBB = is_white ? (currentSquare << 16) : (currentSquare >> 16);
						if(!(pos.total_occupancy & doublePushBB)) {
							Square ddest = BB::getSquareFromOneSquareBitboard(doublePushBB);
							Move m = static_cast<Move>(current_source_square)
								| (static_cast<Move>(ddest) << 6);
							pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
						}
					}
				}

				//pawn captures (+ promotion / EP captures)
				Bitboard pawn_attacks = getPawnAttacks(pos.side_to_move, current_source_square);

				//non-EP
				Bitboard captures = pawn_attacks & enemy_occ;
				while(captures) {
					Bitboard capBB = BB::getLSBSquareBitboard(captures);
					captures &= ~capBB;
					Square dest = BB::getSquareFromOneSquareBitboard(capBB);
					Bitboard promotion_rank = is_white ? RANK_8 : 0x00000000000000FFULL;

					if(capBB & promotion_rank) {
						const PieceType promo_pieces[] = { KNIGHT, BISHOP, ROOK, QUEEN };
						for(PieceType promo : promo_pieces) {
							Move m = static_cast<Move>(current_source_square)
								| (static_cast<Move>(dest) << 6)
								| (static_cast<Move>(1) << 14)
								| (static_cast<Move>(promo) << 15)
								| (static_cast<Move>(1) << 18);
							pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
						}
					}
					else {
						Move m = static_cast<Move>(current_source_square)
							| (static_cast<Move>(dest) << 6)
							| (static_cast<Move>(1) << 18); 
						pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
					}
				}

				//EP captures
				if(pos.ep_square != NO_SQUARE && (pawn_attacks & BB::oneSquareBB(pos.ep_square))) {
					Move m = static_cast<Move>(current_source_square)
						| (static_cast<Move>(pos.ep_square) << 6)
						| (static_cast<Move>(1) << 13)
						| (static_cast<Move>(1) << 18); 
					pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
				}
			}
		}
		break;

		case PieceType::KNIGHT:
		while(to_consider_pieces) {
			//get current square and consume it
			Square current_source_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(to_consider_pieces));
			to_consider_pieces &= ~BB::getLSBSquareBitboard(to_consider_pieces);

			//find legal moves: knight cannot capture own pieces
			Bitboard attackPattern = getKnightAttacks(current_source_square);
			Bitboard move_candidates = attackPattern & ~(pos.color_occupancy[pos.side_to_move]);

			//convert bitboard to movelist entries
			while(move_candidates) {
				//get current square and consume it
				Square current_destination_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(move_candidates));
				move_candidates &= ~(BB::getLSBSquareBitboard(move_candidates));

				//set capture flag when landing on an enemy-occupied square
				Move current_move_candidate = static_cast<Move>(current_source_square)
					| (static_cast<Move>(current_destination_square) << 6);
				if(BB::oneSquareBB(current_destination_square) & pos.color_occupancy[!pos.side_to_move])
					current_move_candidate |= (static_cast<Move>(1) << 18);

				pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;
			}
		}
		break;

		case PieceType::BISHOP:
		while(to_consider_pieces) {
			//get current square and consume it
			Square current_source_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(to_consider_pieces));
			to_consider_pieces &= ~BB::getLSBSquareBitboard(to_consider_pieces);

			//find legal moves
			Bitboard move_candidates = getBishopAttacks(current_source_square, pos.color_occupancy[pos.side_to_move],
				pos.color_occupancy[!pos.side_to_move]);

			while(move_candidates) {
				Square current_destination_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(move_candidates));
				move_candidates &= ~(BB::getLSBSquareBitboard(move_candidates));

				Move current_move_candidate = static_cast<Move>(current_source_square)
					| (static_cast<Move>(current_destination_square) << 6);
				if(BB::oneSquareBB(current_destination_square) & pos.color_occupancy[!pos.side_to_move])
					current_move_candidate |= (static_cast<Move>(1) << 18);

				pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;
			}
		}
		break;

		case PieceType::ROOK:
		while(to_consider_pieces) {
			//get current square and consume it
			Square current_source_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(to_consider_pieces));
			to_consider_pieces &= ~BB::getLSBSquareBitboard(to_consider_pieces);

			//find legal moves
			Bitboard move_candidates = getRookAttacks(current_source_square, pos.color_occupancy[pos.side_to_move],
				pos.color_occupancy[!pos.side_to_move]);

			while(move_candidates) {
				Square current_destination_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(move_candidates));
				move_candidates &= ~(BB::getLSBSquareBitboard(move_candidates));

				Move current_move_candidate = static_cast<Move>(current_source_square)
					| (static_cast<Move>(current_destination_square) << 6);
				if(BB::oneSquareBB(current_destination_square) & pos.color_occupancy[!pos.side_to_move])
					current_move_candidate |= (static_cast<Move>(1) << 18);

				pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;
			}
		}
		break;

		case PieceType::QUEEN:
		while (to_consider_pieces) {
			//get current square and consume it
			Square current_source_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(to_consider_pieces));
			to_consider_pieces &= ~BB::getLSBSquareBitboard(to_consider_pieces);

			Bitboard move_candidates = 
				getRookAttacks(current_source_square, pos.color_occupancy[pos.side_to_move],
					pos.color_occupancy[!pos.side_to_move]) |
				getBishopAttacks(current_source_square, pos.color_occupancy[pos.side_to_move],
					pos.color_occupancy[!pos.side_to_move]);

			while (move_candidates) {
				Square current_destination_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(move_candidates));
				move_candidates &= ~(BB::getLSBSquareBitboard(move_candidates));

				Move current_move_candidate = static_cast<Move>(current_source_square)
					| (static_cast<Move>(current_destination_square) << 6);
				if(BB::oneSquareBB(current_destination_square) & pos.color_occupancy[!pos.side_to_move])
					current_move_candidate |= (static_cast<Move>(1) << 18);

				pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;
			}
		}
		break;

		case PieceType::KING:
		{
			Square king_sq = BB::getSquareFromOneSquareBitboard(to_consider_pieces);
			Bitboard move_candidates = getKingAttacks(king_sq) & ~(pos.color_occupancy[pos.side_to_move]);

			while (move_candidates) {
				Square current_destination_square = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(move_candidates));
				move_candidates &= ~(BB::getLSBSquareBitboard(move_candidates));

				Move current_move_candidate = static_cast<Move>(king_sq)
					| (static_cast<Move>(current_destination_square) << 6);
				if(BB::oneSquareBB(current_destination_square) & pos.color_occupancy[!pos.side_to_move])
					current_move_candidate |= (static_cast<Move>(1) << 18);

				pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = current_move_candidate;
			}

			//castling: check rights from position and required empty squares
			//squares: E1=4, F1=5, G1=6, D1=3, C1=2, B1=1 | E8=60, F8=61, G8=62, D8=59, C8=58, B8=57
			const Bitboard occ = pos.total_occupancy;
			if(pos.side_to_move == WHITE) {
				//white short castle (bit 1)
				if((pos.castling_rights & 0x2) &&
					!(occ & (BB::oneSquareBB(5) | BB::oneSquareBB(6)))) {
					Move m = static_cast<Move>(4) | (static_cast<Move>(6) << 6) | (static_cast<Move>(1) << 12);
					pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
				}
				//white long castle (bit 0)
				if((pos.castling_rights & 0x1) &&
					!(occ & (BB::oneSquareBB(1) | BB::oneSquareBB(2) | BB::oneSquareBB(3)))) {
					Move m = static_cast<Move>(4) | (static_cast<Move>(2) << 6) | (static_cast<Move>(1) << 12);
					pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
				}
			}
			else {
				//black short castle (bit 3)
				if((pos.castling_rights & 0x8) &&
					!(occ & (BB::oneSquareBB(61) | BB::oneSquareBB(62)))) {
					Move m = static_cast<Move>(60) | (static_cast<Move>(62) << 6) | (static_cast<Move>(1) << 12);
					pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
				}
				//black long castle (bit 2)
				if((pos.castling_rights & 0x4) &&
					!(occ & (BB::oneSquareBB(57) | BB::oneSquareBB(58) | BB::oneSquareBB(59)))) {
					Move m = static_cast<Move>(60) | (static_cast<Move>(58) << 6) | (static_cast<Move>(1) << 12);
					pseudo_legal_piece_moves.moves[pseudo_legal_piece_moves.count++] = m;
				}
			}
		}
		break;
		default:
		break;
	}

	return pseudo_legal_piece_moves;
}


MoveList PseudoLegalMoveGenerator::getPseudoLegalMoves(const Position& pos) {
	MoveList pseudoLegalMoves{};

	for(int piece_type = PAWN; piece_type <= KING; ++piece_type) {

		MoveList pieceMoves = getAllMovesForPieceType(pos, static_cast<PieceType>(piece_type));

		for(std::uint16_t i = 0; i < pieceMoves.count; ++i) {
			pseudoLegalMoves.moves[pseudoLegalMoves.count++] = pieceMoves.moves[i];
		}
	}

	return pseudoLegalMoves;
}