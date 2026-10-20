#include "DataTypes.h"
#include "Bitboard.h"
#include "BoardState.h"
#include "AttackTables.h"
#include "IOHandler.h"
#include <iostream>

int main() {
	initAttackTables();

	BoardState board;
	board.setStartingPosition();

	IOHandler iohandler;
	return 0;
}