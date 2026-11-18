#include <algorithm>
#include <bit>
#include <filesystem>
#include <string>
#include <vector>

#include <QApplication>

#include "DataTypes.hpp"

#include "AttackDetector.hpp"
#include "AttackTables.hpp"
#include "BoardState.hpp"
#include "Doorman.hpp"
#include "LegalMoveGenerator.hpp"
#include "MainWindow.hpp"
#include "MoveNotation.hpp"
#include "PrincipleLoader.hpp"

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

//data/ next to the executable for installed builds, otherwise source tree so edits show up on refresh
std::filesystem::path findPrinciplesFile() {
	const std::filesystem::path installed = std::filesystem::path(QCoreApplication::applicationDirPath().toStdU16String()) / "data" / "principles.json";
	std::error_code error;
	if (std::filesystem::exists(installed, error)) {
		return installed;
	}
	return std::filesystem::path(CHESS_DATA_DIR) / "principles.json";
}

void loadPrinciples(Doorman& doorman) {
	const PrincipleFile file = PrincipleLoader::load(findPrinciplesFile());
	for (const std::string& problem : file.problems) {
		qWarning("principles: %s", problem.c_str());
	}
	doorman.setPrinciples(file.principles);
	doorman.setPrincipleProblems(file.problems);
}

//placeholder: generate simple values for the eval square (piece value + weighted attacks)
//to be replaced by a similar concept but using the engine searches best move line
void showDemoValues(Doorman& doorman, BoardState& board, LegalMoveGenerator& generator) {
	constexpr float PIECE_VALUES[] = {2, 5, 6, 10, 18};
	const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
	SquareValues values{};
	for (Square square = 0; square < 64; ++square) {
		const Bitboard bit = Bitboard{1} << square;
		for (int type = PAWN; type < KING; ++type) {
			if (position.pieces[WHITE][type] & bit) values[square] += PIECE_VALUES[type];
			if (position.pieces[BLACK][type] & bit) values[square] -= PIECE_VALUES[type];
		}
	}
	const auto getType = [&](Color color, Square square) {
		for (int type = PAWN; type < KING; ++type) {
			if (position.pieces[color][type] & (Bitboard{1} << square)) return type;
		}
		return static_cast<int>(KING);
	};
	const auto getValue = [&](int type) { return type < KING ? PIECE_VALUES[type] : 0.0f; };
	for (Square square = 0; square < 64; ++square) {
		for (Color color : {WHITE, BLACK}) {
			const float sign = color == WHITE ? 1.0f : -1.0f;
			for (Bitboard attackers = attackersToSquare(square, color, position); attackers != 0; attackers &= attackers - 1) {
				values[square] += sign * getValue(getType(color, static_cast<Square>(std::countr_zero(attackers))));
			}
		}
	}
	const MoveList moves = generator.generateLegalMovesInPosition(position, board);
	const float pushSign = position.side_to_move == WHITE ? 1.0f : -1.0f;
	for (std::size_t index = 0; index < moves.count; ++index) {
		const Square source = moveBits::getSource(moves.moves[index]);
		const Square destination = moveBits::getDestination(moves.moves[index]);
		if (getType(position.side_to_move, source) == PAWN && moveBits::getFile(source) == moveBits::getFile(destination)) {
			values[destination] += pushSign * PIECE_VALUES[PAWN];
		}
	}
	doorman.setSquareValues(values);
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
	showDemoValues(doorman, board, legalMoveGenerator);
	loadPrinciples(doorman);	//the engine reads the current choice with doorman.getPrinciples()

	//the user pressed Refresh in the principle drawer
	QObject::connect(&doorman, &Doorman::principlesRefreshRequested, &doorman, [&] { loadPrinciples(doorman); });

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
		const std::string label = MoveNotation::buildMoveString(move, board.getPosition());	//before the move is made
		board.makeMove(move);
		doorman.playMove(move, label);				//advances or forks the move tree
		doorman.setPosition(board.getPosition());	//the core's position is authoritative
		showDemoValues(doorman, board, legalMoveGenerator);
		legalMoves.count = 0;
	});

	//the user jumped to another move in the tree: bring the core to the same position
	QObject::connect(&doorman, &Doorman::treeNodeSelected, &doorman, [&](NodeId id, const Position&) {
		replayMoves(board, doorman.getMovesToTreeNode(id));
		showDemoValues(doorman, board, legalMoveGenerator);
		legalMoves.count = 0;
	});

	window.show();
	return QApplication::exec();
}
