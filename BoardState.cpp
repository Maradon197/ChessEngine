#include "BoardState.hpp"
#include "Bitboard.hpp"

void BoardState::setStartingPosition() {
	//white pieces
	position.pieces[WHITE][PAWN] = 0x000000000000FF00ULL;  //Pawns
	position.pieces[WHITE][KNIGHT] = 0x0000000000000042ULL;  //Knights
	position.pieces[WHITE][BISHOP] = 0x0000000000000024ULL;  //Bishops
	position.pieces[WHITE][ROOK] = 0x0000000000000081ULL;  //Rooks
	position.pieces[WHITE][QUEEN] = 0x0000000000000008ULL;  //Queens
	position.pieces[WHITE][KING] = 0x0000000000000010ULL;  //King

	//Black pieces
	position.pieces[BLACK][PAWN] = 0x00FF000000000000ULL;
	position.pieces[BLACK][KNIGHT] = 0x4200000000000000ULL;
	position.pieces[BLACK][BISHOP] = 0x2400000000000000ULL;
	position.pieces[BLACK][ROOK] = 0x8100000000000000ULL;
	position.pieces[BLACK][QUEEN] = 0x0800000000000000ULL;
	position.pieces[BLACK][KING] = 0x1000000000000000ULL;

	//occupancy
	position.color_occupancy[WHITE] =
		position.pieces[WHITE][PAWN] |
		position.pieces[WHITE][KNIGHT] |
		position.pieces[WHITE][BISHOP] |
		position.pieces[WHITE][ROOK] |
		position.pieces[WHITE][QUEEN] |
		position.pieces[WHITE][KING];
	position.color_occupancy[BLACK] =
		position.pieces[BLACK][PAWN] |
		position.pieces[BLACK][KNIGHT] |
		position.pieces[BLACK][BISHOP] |
		position.pieces[BLACK][ROOK] |
		position.pieces[BLACK][QUEEN] |
		position.pieces[BLACK][KING];

	position.total_occupancy =
		position.color_occupancy[WHITE] |
		position.color_occupancy[BLACK];

	//misc game state
	position.side_to_move = WHITE;
	position.castling_rights = 0b1111;
	position.ep_square = NO_SQUARE;
	position.reversible_halfmove_clock = 0;

	position.fullmove_counter = 1;
}

const Position& BoardState::getPosition() {
	return position;
}

bool BoardState::makeMove(Move m) {
	UndoState undo_state{};

	//decode move
	Square source = m & 0x3f;
	Square destination = (m >> 6) & 0x3f;

	PieceType promo_piece_type = static_cast<PieceType>(((m >> 12) & 0x3) + KNIGHT);

	//TODO: assert wether this is too runtime inefficient
	PieceType moving_piece_type{};
	for (int i = 0; i < 6; ++i)
		if (position.pieces[position.side_to_move][i] & BB::oneSquareBB(source)) {
			moving_piece_type = static_cast<PieceType>(i);
			break;
		}

	PieceType captured_piece_type{};
	for (int i = 0; i < 6; ++i) {
		if (position.pieces[!position.side_to_move][i] & BB::oneSquareBB(destination)) {
			captured_piece_type = static_cast<PieceType>(i);
			break;
		}
	}

	bool is_ep = (moving_piece_type == PAWN && destination == position.ep_square);
	bool is_capture = ((m >> 14) & 0b1) && is_ep;
	bool is_castle = moving_piece_type == KING &&
		(destination == source + 2 || source == destination + 2);
	bool is_promo = moving_piece_type == PAWN &&
		(destination >= 56 || destination < 8);

	//push back move information
	undo_state.previous_move_source = source;
	undo_state.previous_destination = destination;
	undo_state.fullmove_counter = position.fullmove_counter;
	undo_state.reversible_halfmove_clock = position.reversible_halfmove_clock;
	undo_state.ep_square = position.ep_square;
	if (is_capture)
		undo_state.captured_piece = captured_piece_type;
	undo_state.castling_rights = position.castling_rights;

	//remove old piece
	position.color_occupancy[position.side_to_move] &= ~BB::oneSquareBB(source);
	position.pieces[position.side_to_move][moving_piece_type] &= ~BB::oneSquareBB(source);

	//apply move
	if (is_castle) {
		const int home_rank = position.side_to_move == WHITE ? 0 : 56;
		const bool is_castling_king_side = destination > source;
		const Square rook_source = home_rank + (is_castling_king_side ? 7 : 0);
		const Square rook_destination = home_rank + (is_castling_king_side ? 5 : 3);

		position.pieces[position.side_to_move][KING] |= BB::oneSquareBB(destination);
		position.pieces[position.side_to_move][ROOK] &= ~BB::oneSquareBB(rook_source);
		position.pieces[position.side_to_move][ROOK] |= BB::oneSquareBB(rook_destination);

		position.castling_rights &= position.side_to_move == WHITE ? REVOKE_WHITE_CASTLES : REVOKE_BLACK_CASTLES;
	}
	else if (is_promo) {
		if (is_capture)
			position.pieces[!position.side_to_move][captured_piece_type] &= ~BB::oneSquareBB(destination);
		position.pieces[position.side_to_move][promo_piece_type] |= BB::oneSquareBB(destination);
	}
	else {
		if (is_capture && !is_ep)
			position.pieces[!position.side_to_move][captured_piece_type] &= ~BB::oneSquareBB(destination);
		else if (is_ep) {
			const Square captured_square = position.side_to_move == WHITE ? destination - 8 : destination + 8;
			position.pieces[!position.side_to_move][PAWN] &= ~BB::oneSquareBB(captured_square);
		}

		//quiet move
		position.pieces[position.side_to_move][moving_piece_type] |= BB::oneSquareBB(destination);

		//keep in mind we are not castling right now
		if (moving_piece_type == KING)
			position.castling_rights &= position.side_to_move == WHITE ?
			REVOKE_WHITE_CASTLES :
			REVOKE_BLACK_CASTLES;
		else if (moving_piece_type == ROOK) {
			if (source == (position.side_to_move == WHITE ? WHITE_LEFT_ROOK_HOME_SQUARE : BLACK_LEFT_ROOK_HOME_SQUARE))
				position.castling_rights &= (position.side_to_move == WHITE) ? REVOKE_WHITE_LONG_CASTLE : REVOKE_BLACK_LONG_CASTLE;
			if (source == (position.side_to_move == WHITE ? WHITE_RIGHT_ROOK_HOME_SQUARE : BLACK_RIGHT_ROOK_HOME_SQUARE))
				position.castling_rights &= (position.side_to_move == WHITE) ? REVOKE_WHITE_SHORT_CASTLE : REVOKE_BLACK_SHORT_CASTLE;
		}
		if (is_capture && captured_piece_type == ROOK) {
			if (destination == (position.side_to_move == WHITE ? 56 : 0))
				position.castling_rights &= position.side_to_move == WHITE ? 0b1011 : 0b1110;
			if (destination == (position.side_to_move == WHITE ? 63 : 7))
				position.castling_rights &= position.side_to_move == WHITE ? 0b0111 : 0b1101;
		}
	}

	position.color_occupancy[WHITE] = position.color_occupancy[BLACK] = 0;
	for (int color = WHITE; color <= BLACK; ++color)
		for (int piece = PAWN; piece <= KING; ++piece)
			position.color_occupancy[color] |= position.pieces[color][piece];
	position.total_occupancy = position.color_occupancy[WHITE] | position.color_occupancy[BLACK];
	position.ep_square = NO_SQUARE;

	if (moving_piece_type == PAWN && destination > source + 8 &&
		destination == source + 16)
		position.ep_square = source + 8;
	else if (moving_piece_type == PAWN && source > destination + 8 &&
		source == destination + 16)
		position.ep_square = source - 8;

	if (moving_piece_type == PAWN || is_capture || is_ep)
		position.reversible_halfmove_clock = 0;
	else
		++position.reversible_halfmove_clock;

	position.side_to_move = position.side_to_move == WHITE ? BLACK : WHITE;

	if (position.side_to_move == WHITE)
		++position.fullmove_counter;

	undoable_history.push(undo_state);
	return true;
}

void BoardState::unmakeMove() {
	UndoState undo_state = undoable_history.top();

    //todo: undo actual piece movement
    position.side_to_move = !position.side_to_move;
    position.ep_square = undo_state.ep_square;
    position.castling_rights = undo_state.castling_rights;
    position.reversible_halfmove_clock = undo_state.reversible_halfmove_clock;
    position.fullmove_counter = undo_state.fullmove_counter;

    undoable_history.pop();
}