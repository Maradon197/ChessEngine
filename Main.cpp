#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include "BoardState.hpp"
#include "AttackTables.hpp"
#include "IOHandler.hpp"
#include <iostream>

//TODO next:
//fix: quiet moves that are not from pawns sometimes dont remove the old piece
//test: is string parsing or the implementation broken?
//implement: there is no way to pass something through the legal move pipeline yet

int main() {
	initAttackTables();

	BoardState board;
	board.setStartingPosition();

	IOHandler iohandler;
	iohandler.printPosition(board.getPosition());

	while (true) {
		const Move move = IOHandler::attemptMove();

		if (move == 0) {
			std::cout << "main.cpp: Invalid input\n";
			continue;
		}

		board.makeMove(move);
		IOHandler::printPosition(board.getPosition());
	}
	return 0;
}