#ifndef DATATYPES
#define DATATYPES

#include <cstdint>

//MEMORY LAYOUT OF BITBOARD:
// one bit per square
// when a val is 1 == occupied (usually)
//	Squares:	Maps to:
//	A1			0
//	H1			7
//	A2			8
//		  ...
//	A8			56
//	H8			63	
using Bitboard = std::uint64_t;

//MEMORY LAYOUT OF MOVE:
// 0-5:		source square (0-63)
// 6-11:	destination square (0-63)
// 12:		is Castle
// 13:		is En Passant
// 14:		is Promotion
// 15-17:	promotion piece type (0 for no promotion)
// 18+:		unused
using Move = std::uint32_t;

//MEMORY LAYOUT OF CASTLING RIGHTS:
// 0 / 1:	allow White long (0) / short (1) castle
// 2 / 3:	allow Black long (2) / short (3) castle
// 4+:		unused
using CastlingRights = std::uint8_t;

//MEMORY LAYOUT OF SQUARE:
// 0-5:		source square (0-63)
// 6:		flag: is EP allowed? (NO_SQUARE)
// 7+:		must be zero! otherwise BB logic might break
using Square = std::uint8_t;
constexpr Square NO_SQUARE = 64;

//simple integer counting up moves for ply / fullmove clock
//the hypothetical maximum is ~8k, so std::uint8_t is not sufficient
//even though more than 255 moves are extremely unlikely
using Clock = std::uint16_t;

enum Color {WHITE, BLACK};
enum PieceType {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NONE};

struct Position {
	Bitboard pieces[2][6];

	Bitboard total_occupancy;
	Bitboard color_occupancy[2];
	
	Color side_to_move;
	CastlingRights castling_rights;
	Square ep_square;

	Clock reversible_halfmove_clock;
	Clock fullmove_counter;
};

struct UndoState {
	Square previous_move_source;
	Square previous_destination;

	PieceType captured_piece;
	Square en_passant_capture;

	CastlingRights castling_rights;

	Clock reversible_halfmove_clock;
	Clock fullmove_counter;
};
#endif
