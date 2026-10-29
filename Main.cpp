#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include "BoardState.hpp"
#include "AttackTables.hpp"
#include "IOHandler.hpp"
#include <iostream>

int main() {
	initAttackTables();

	BoardState board;
	board.setStartingPosition();

	IOHandler iohandler;
	return 0;
}