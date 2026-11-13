#ifndef MOVEBITS_HPP
#define MOVEBITS_HPP

#include "DataTypes.hpp"

namespace moveBits {

constexpr Move SQUARE_MASK = 0x3F;
constexpr int DESTINATION_SHIFT = 6;
constexpr Move FROM_TO_MASK = 0xFFF;
constexpr Move CASTLE_FLAG = 1u << 12;
constexpr Move EN_PASSANT_FLAG = 1u << 13;
constexpr Move PROMOTION_FLAG = 1u << 14;
constexpr int PROMOTION_TYPE_SHIFT = 15;
constexpr Move PROMOTION_TYPE_MASK = 0x7;
constexpr Move CAPTURE_FLAG = 1u << 18;

constexpr Square getSource(Move move) {
	return static_cast<Square>(move & SQUARE_MASK);
}

constexpr Square getDestination(Move move) {
	return static_cast<Square>((move >> DESTINATION_SHIFT) & SQUARE_MASK);
}

constexpr bool isCastle(Move move) {
	return (move & CASTLE_FLAG) != 0;
}

constexpr bool isEnPassant(Move move) {
	return (move & EN_PASSANT_FLAG) != 0;
}

constexpr bool isPromotion(Move move) {
	return (move & PROMOTION_FLAG) != 0;
}

constexpr PieceType getPromotionType(Move move) {
	return static_cast<PieceType>((move >> PROMOTION_TYPE_SHIFT) & PROMOTION_TYPE_MASK);
}

constexpr bool isCapture(Move move) {
	return (move & CAPTURE_FLAG) != 0;
}

constexpr Move makeMove(Square source, Square destination) {
	return (static_cast<Move>(source) & SQUARE_MASK)
		| ((static_cast<Move>(destination) & SQUARE_MASK) << DESTINATION_SHIFT);
}

constexpr Move getFromTo(Move move) {
	return move & FROM_TO_MASK;
}

constexpr bool isValidSquare(Square square) {
	return square < 64;
}

constexpr int getFile(Square square) {
	return square & 7;
}

constexpr int getRank(Square square) {
	return square >> 3;
}

constexpr Square makeSquare(int file, int rank) {
	return static_cast<Square>(rank * 8 + file);
}

constexpr Bitboard getSquareBit(Square square) {
	return Bitboard{1} << square;
}

} // !namespace moveBits

#endif
