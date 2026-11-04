#include "AttackDetector.hpp"
#include "AttackTables.hpp"
#include "Bitboard.hpp"
#include "DataTypes.hpp"
#include <cstdint>


bool isSquareAttacked(Square sq, Color byColor, const Position& pos) {
	return attackersToSquare(sq, byColor, pos) != 0;
}

Bitboard attackersToSquare(Square sq, Color attackedColor, const Position& pos) {
	
	Bitboard possibleAttackerPositions{};

	possibleAttackerPositions |= 
		getKnightAttacks(sq) | 
		getBishopAttacks(sq, pos.color_occupancy[attackedColor], pos.color_occupancy[!attackedColor]) |
		getRookAttacks(sq, pos.color_occupancy[attackedColor], pos.color_occupancy[!attackedColor]);
		//bishop & rook incluldes king and queen attacks

	return possibleAttackerPositions & pos.color_occupancy[!attackedColor];
}

bool isInCheck(Color color, const Position& pos) {
	return checkersToColor(color, pos) != 0;
}

Bitboard checkersToColor(Color color, const Position& pos) {

	Square king_square = BB::getSquareFromOneSquareBitboard(pos.pieces[color][PieceType::KING]);
	return attackersToSquare(king_square, color, pos);
}