#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include "BoardState.hpp"
#include "AttackTables.hpp"
#include "IOHandler.hpp"
#include "LegalMoveGenerator.hpp"
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
	LegalMoveGenerator legal_move_generator{};

	while (true) {

		const Move input_move = IOHandler::attemptMove();

		if (input_move == 0) {
			std::cout << "main.cpp: Invalid input\n";
			continue;
		}
		MoveList legal_moves = legal_move_generator.generateLegalMovesForSquare(board.getPosition(), static_cast<Square>(input_move >>6), board);

		for (int i = 0; i < legal_moves.count; ++i) {
			if (legal_moves.moves[i] == input_move) {
				board.makeMove(input_move);
				break;
			}
			//not in legal moves:
			std::cout << "main.cpp: Illegal move\n";
		}
		
		IOHandler::printPosition(board.getPosition());
	}
	return 0;
}