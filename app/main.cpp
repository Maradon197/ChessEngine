#include <algorithm>
#include <bit>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <iostream>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include "DataTypes.hpp"

#include "AttackDetector.hpp"
#include "AttackTables.hpp"
#include "BoardState.hpp"
#include "Doorman.hpp"
#include "LegalMoveGenerator.hpp"
#include "MainWindow.hpp"
#include "MoveNotation.hpp"
#include "PgnWriter.hpp"

#include "Evaluator.hpp"
#include "Searcher.hpp"

//temporary includes -----------------

//------------------------------------

namespace {

constexpr Color ENGINE_COLOR = BLACK;	//fixed for now, the user plays white
constexpr int ENGINE_SEARCH_DEPTH = 4;	//plies

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

	//overall balance for the map's tilt: relative material difference, (white - black) / (white + black) in [-1, 1]
	//(the plain ratio white / black would explode once a side has only its king left)
	//the emptier the board, the more a material lead decides, so the balance is boosted towards the endgame
	constexpr float START_MATERIAL = 2 * (8 * PIECE_VALUES[PAWN] + 2 * PIECE_VALUES[KNIGHT] + 2 * PIECE_VALUES[BISHOP]
		+ 2 * PIECE_VALUES[ROOK] + PIECE_VALUES[QUEEN]);
	constexpr float ENDGAME_BOOST = 2.0f;	//balance multiplier = 1 + ENDGAME_BOOST * emptiness, so up to 3x on a bare board
	float material[2] = {0, 0};
	for (Color color : {WHITE, BLACK}) {
		for (int type = PAWN; type < KING; ++type) {
			material[color] += PIECE_VALUES[type] * std::popcount(position.pieces[color][type]);
		}
	}
	const float total = material[WHITE] + material[BLACK];
	const float relative = total > 0 ? (material[WHITE] - material[BLACK]) / total : 0.0f;
	const float emptiness = 1 - std::min(total / START_MATERIAL, 1.0f);
	doorman.setSquareValues(values, relative * (1 + ENDGAME_BOOST * emptiness));
}

int criterionSquaresFromEvalValue(EvalValue score) {
	constexpr EvalValue LIMITS[] = { 0.9f, 3.0f, 5.0f };
	int crit_sq = 0;
	for (EvalValue current_limit : LIMITS) {
		if (std::abs(score) >= current_limit)
			crit_sq++;
	}
	return score < 0 ? -crit_sq : crit_sq;
}

void showEvaluation(Doorman& dm, const BoardState& state, Evaluator& eval) {
	const Position& pos = state.getPosition();
	EvalValue overall_score = eval.overallScore(pos);
	EvalValue weighted_material = eval.weightedMaterial(pos);

	if (pos.side_to_move == BLACK) {
		overall_score = -overall_score;
		weighted_material = -weighted_material;
	}
	
	dm.setCriterion(EvalCriterion::WEIGHTED_MATERIAL, criterionSquaresFromEvalValue(weighted_material));
	dm.setOverallCentipawns(static_cast<int> (overall_score * 100));

}

//a finished game goes into the app's data folder as a PGN file named after the time it ended
//(QStandardPaths::AppDataLocation, on Windows %APPDATA%/ChessEngineGUI/games)
void saveGame(const std::vector<Move>& moves, GameStatus status) {
	const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/games";
	if (!QDir().mkpath(folder)) {
		std::cerr << "Could not create the game folder " << folder.toStdString() << std::endl;
		return;
	}
	const QDateTime now = QDateTime::currentDateTime();
	PgnWriter::Headers headers;
	headers.date = now.toString("yyyy.MM.dd").toStdString();
	headers.white = "Player";
	headers.black = "Engine";
	const std::string pgn = PgnWriter::buildPgn(moves, status, headers);

	const QString path = folder + "/" + now.toString("yyyy-MM-dd_HH-mm-ss") + ".pgn";
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		std::cerr << "Could not save the game to " << path.toStdString() << std::endl;
		return;
	}
	file.write(pgn.data(), static_cast<qint64>(pgn.size()));
	std::cout << "Game saved to " << path.toStdString() << std::endl;
}

} // !namespace

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);
	QApplication::setApplicationName(SETTINGS_APPLICATION);	//names the data folder the games are saved in
	initAttackTables();

	Doorman doorman;
	MainWindow window(doorman);
	Evaluator evaluator;

	//core state
	BoardState board;
	board.setStartingPosition();
	LegalMoveGenerator legalMoveGenerator{};
	MoveList legalMoves{};
	unsigned positionVersion = 0;	//counts changes of the core's position, an engine answer for an older one is dropped

	doorman.setPosition(board.getPosition());
	showDemoValues(doorman, board, legalMoveGenerator);
	showEvaluation(doorman, board, evaluator);
	doorman.loadPrinciplesFromEngine();

	//the user picked up a piece: show its legal moves
	QObject::connect(&doorman, &Doorman::squareSelected, &doorman, [&](Square square) {
		const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
		legalMoves = legalMoveGenerator.generateLegalMovesForSquare(position, square, board);
		doorman.setLegalMoves(legalMoves);
	});

	//rule-based game end of the core's current position
	const auto updateGameStatus = [&] {
		const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
		doorman.setGameStatus(board.getGameStatus(legalMoveGenerator.generateLegalMovesInPosition(position, board)));
	};

	//a verified move, from the user or the engine
	const auto applyMove = [&](Move move) {
		const std::string label = MoveNotation::buildMoveString(move, board.getPosition());	//before the move is made
		board.makeMove(move);
		doorman.playMove(move, label);				//advances or forks the move tree
		doorman.setPosition(board.getPosition());	//the core's position is authoritative
		showDemoValues(doorman, board, legalMoveGenerator);
		showEvaluation(doorman, board, evaluator);
		legalMoves.count = 0;
		++positionVersion;
		updateGameStatus();

		//a game played in the play view just ended: keep it
		const GameStatus status = doorman.getGameStatus();
		if (status.result != GameResult::NONE && window.getViewMode() == ViewMode::PLAY) {
			saveGame(doorman.getMovesToTreeNode(doorman.getCurrentTreeNode()), status);
		}
	};

	//the user tried a move: only legal moves are played, everything else snaps back
	QObject::connect(&doorman, &Doorman::moveAttempted, &doorman, [&](Move move) {
		if (!containsMove(legalMoves, move)) {
			return;
		}
		applyMove(move);
	});

	//engine: searches a copy of the core's board in its own thread so the GUI stays responsive, the board is locked meanwhile
	//the answer comes back to the GUI thread and is played like a user move, unless the position changed in between
	Searcher searcher;
	std::thread searchThread;
	bool engineThinking = false;
	bool searchAgain = false;	//asked again while thinking, e.g. after switching views back and forth
	std::function<void()> startSearch = [&] {
		if (engineThinking) {
			searchAgain = true;
			return;
		}
		if (searchThread.joinable()) {
			searchThread.join();	//the previous search is done already, its answer has arrived
		}
		engineThinking = true;
		searchAgain = false;
		doorman.setBoardInteractive(false);
		const unsigned version = positionVersion;
		searchThread = std::thread([&, version, boardCopy = board, generatorCopy = legalMoveGenerator, evaluatorCopy = evaluator]() mutable {
			const SearchResult result = searcher.searchPosition(boardCopy, generatorCopy, evaluatorCopy,
				ENGINE_SEARCH_DEPTH, SearchLimits::NO_SEARCH_TIME_LIMIT);
			QMetaObject::invokeMethod(&doorman, [&, version, result] {
				engineThinking = false;
				if (version == positionVersion && result.bestMove != 0) {
					applyMove(result.bestMove);	//unlocks the board again through the game status
					return;
				}
				//outdated (the user jumped around in the tree meanwhile) or no move at all
				doorman.setBoardInteractive(doorman.getGameStatus().result == GameResult::NONE);
				if (searchAgain && board.getPosition().side_to_move == ENGINE_COLOR && doorman.getGameStatus().result == GameResult::NONE) {
					startSearch();
				}
			}, Qt::QueuedConnection);
		});
	};

	//play view: the engine's side is to move
	QObject::connect(&doorman, &Doorman::engineMoveRequested, &doorman, [&](const Position&) {
		if (board.getPosition().side_to_move == ENGINE_COLOR) {	//the core's position is authoritative
			startSearch();
		}
	});

	//the user jumped to another move in the tree: bring the core to the same position
	QObject::connect(&doorman, &Doorman::treeNodeSelected, &doorman, [&](NodeId id, const Position&) {
		replayMoves(board, doorman.getMovesToTreeNode(id));
		++positionVersion;
		showDemoValues(doorman, board, legalMoveGenerator);
		showEvaluation(doorman, board, evaluator);
		legalMoves.count = 0;
		updateGameStatus();
	});

	window.show();
	const int exitCode = QApplication::exec();
	if (searchThread.joinable()) {
		searchThread.join();	//the search can't be stopped yet, closing mid-search waits for it to finish
	}
	return exitCode;
}
