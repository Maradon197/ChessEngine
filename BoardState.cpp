#include "BoardState.hpp"

void BoardState::setStartingPosition() {
    //white pieces
    position.pieces[WHITE][PAWN] =      0x000000000000FF00ULL;  //Pawns
    position.pieces[WHITE][KNIGHT] =    0x0000000000000042ULL;  //Knights
    position.pieces[WHITE][BISHOP] =    0x0000000000000024ULL;  //Bishops
	position.pieces[WHITE][ROOK] =      0x0000000000000081ULL;  //Rooks
	position.pieces[WHITE][QUEEN] =     0x0000000000000008ULL;  //Queens
	position.pieces[WHITE][KING] =      0x0000000000000010ULL;  //King

    //Black pieces
    position.pieces[BLACK][PAWN] =      0x00FF000000000000ULL;
    position.pieces[BLACK][KNIGHT] =    0x4200000000000000ULL;
    position.pieces[BLACK][BISHOP] =    0x2400000000000000ULL;
    position.pieces[BLACK][ROOK] =      0x8100000000000000ULL;
    position.pieces[BLACK][QUEEN] =     0x0800000000000000ULL;
    position.pieces[BLACK][KING] =      0x1000000000000000ULL;

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

const Position& BoardState::getPosition() const {
    return position;
}