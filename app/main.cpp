#include <algorithm>
#include <vector>

#include <QApplication>

#include "DataTypes.hpp"

#include "AttackTables.hpp"
#include "BoardState.hpp"
#include "Doorman.hpp"
#include "LegalMoveGenerator.hpp"
#include "MainWindow.hpp"

namespace {

bool containsMove(const MoveList& moves, Move move) {
	const Move* end = moves.moves + moves.count;
	return std::find(moves.moves, end, move) != end;
}


void replayMoves(BoardState& board, const std::vector<Move>& moves) {
	board = BoardState{};
	board.setStartingPosition();
	for (Move move : moves) {
		board.makeMove(move);
	}
}

} // !namespace

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);
	initAttackTables();

	Doorman doorman;
	MainWindow window(doorman);

	//core state
	BoardState board;
	board.setStartingPosition();
	LegalMoveGenerator legalMoveGenerator{};
	MoveList legalMoves{};

	doorman.setPosition(board.getPosition());

	//the user picked up a piece: show its legal moves
	QObject::connect(&doorman, &Doorman::squareSelected, &doorman, [&](Square square) {
		const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
		legalMoves = legalMoveGenerator.generateLegalMovesForSquare(position, square, board);
		doorman.setLegalMoves(legalMoves);
	});

	//the user tried a move: only legal moves are played, everything else snaps back
	QObject::connect(&doorman, &Doorman::moveAttempted, &doorman, [&](Move move) {
		if (!containsMove(legalMoves, move)) {
			return;
		}
		board.makeMove(move);
		doorman.playMove(move);						//advances or forks the move tree
		doorman.setPosition(board.getPosition());	//the core's position is authoritative
		legalMoves.count = 0;
	});

	//the user jumped to another move in the tree: bring the core to the same position
	QObject::connect(&doorman, &Doorman::treeNodeSelected, &doorman, [&](NodeId id, const Position&) {
		replayMoves(board, doorman.getMovesToTreeNode(id));
		legalMoves.count = 0;
	});

	window.show();
	return QApplication::exec();
}
