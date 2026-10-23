#ifndef ATTACKDETECTOR
#define ATTACKDETECTOR

#include "DataTypes.h"
class AttackDetector {
	bool isSquareAttacked(Square sq, Color byColor, const Position& pos);
	Bitboard attackersToSquare(Square sq, Color byColor, const Position& pos);

	bool isInCheck(Color color, const Position& pos);
	Bitboard checkersToColor(Color color, const Position& pos);
};

#endif
