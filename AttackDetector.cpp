#include "AttackDetector.hpp"
#include "AttackTables.hpp"
#include "Bitboard.hpp"
#include "DataTypes.hpp"
#include <cstdint>


bool AttackDetector::isSquareAttacked(Square sq, Color byColor, const Position& pos) {
	return attackersToSquare(sq, byColor, pos) != 0;
}

Bitboard AttackDetector::attackersToSquare(Square sq, Color byColor, const Position& pos) {
	//TODO: implement logic to find attackers to a square
	return BB::oneSquareBB(0);
}

bool AttackDetector::isInCheck(Color color, const Position& pos) {
	return checkersToColor(color, pos) != 0;
}

Bitboard AttackDetector::checkersToColor(Color color, const Position& pos) {
	//TODO: implement logic to find checkers to a color
	return BB::oneSquareBB(0);
}