#ifndef IOHANDLER
#define IOHANDLER
#include <string>

#include "DataTypes.hpp"
#include "Bitboard.hpp"

class IOHandler {
public:
	static std::string buildStringFromSquare(Square);
	static Square buildSquareFromString(const std::string&);

	static void printBitboard(Bitboard bb);
	static void printPosition(Position pos);

	Square getSquareInput();
	static Move attemptMove();
};
#endif
