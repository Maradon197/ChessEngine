#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
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
#include <QTimer>

#include "DataTypes.hpp"

#include "AttackDetector.hpp"
#include "AttackTables.hpp"
#include "BoardState.hpp"
#include "Doorman.hpp"
#include "LegalMoveGenerator.hpp"
#include "MoveBits.hpp"
#include "MainWindow.hpp"
#include "MoveNotation.hpp"
#include "PgnWriter.hpp"
#include "TutorialOverlay.hpp"

#include "Evaluator.hpp"
#include "Searcher.hpp"
#include "SearchRecorder.hpp"
#include "SearchTreeModel.hpp"

//temporary includes -----------------

//------------------------------------

namespace {

constexpr Color ENGINE_COLOR = BLACK;	//fixed for now, the user plays white

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

struct ControlValues {
	SquareValues values{};
	float balance = 0;
};

//placeholder: generate simple values for the eval square (piece value + weighted attacks)
//blended along the engine's best line by showBestLineValues()
ControlValues computeControlValues(BoardState& board, LegalMoveGenerator& generator) {
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
	return {values, relative * (1 + ENDGAME_BOOST * emptiness)};
}

void showDemoValues(Doorman& doorman, BoardState& board, LegalMoveGenerator& generator) {
	const ControlValues control = computeControlValues(board, generator);
	doorman.setSquareValues(control.values, control.balance);
}

//the board control map with the next plies of the engine's best line (quiescence captures included) mixed in, so a
//piece that is about to be taken already fades out. Nothing is searched or evaluated for this: each position on the
//line gets the same vector as the current one, the vectors are averaged with these weights, then drawn as usual
//	PLY_WEIGHTS[k]		the position after k plies of the line, k = 0 is the one on the board
//	QUIET_FACTOR		a later position counts this much less as long as no capture happened on the way there:
//						quiet plans of the engine shouldn't repaint the board, exchanges should
//	UNSETTLED_FACTOR	a position whose next move on the line is a capture is mid-exchange and counts this much less,
//						the position after the capture takes over
//e.g. QxQ with the recapture coming: the current map only keeps ~25%, the queen's area turns to the recapturing side
//e.g. a quiet line: the current map keeps ~75%
void showBestLineValues(Doorman& doorman, const std::shared_ptr<const SearchSnapshot>& snapshot, const LegalMoveGenerator& generator) {
	constexpr std::array<float, 4> PLY_WEIGHTS = {1.0f, 0.55f, 0.3f, 0.2f};
	constexpr float QUIET_FACTOR = 0.35f;
	constexpr float UNSETTLED_FACTOR = 0.35f;

	const SearchTreeModel model(snapshot, nullptr);
	std::vector<Move> line;
	for (const std::vector<int>* part : {&model.getBestLine(), &model.getQuiescenceTail()}) {
		for (int node : *part) {
			if (line.size() + 1 < PLY_WEIGHTS.size()) {
				line.push_back(model.getNode(node).move);
			}
		}
	}

	BoardState board = snapshot->root;
	LegalMoveGenerator lineGenerator = generator;
	ControlValues mixed;
	float totalWeight = 0;
	bool captured = false;	//a capture happened between the board and this position
	for (std::size_t ply = 0; ply <= line.size(); ++ply) {
		float weight = PLY_WEIGHTS[ply];
		if (ply > 0 && !captured) weight *= QUIET_FACTOR;
		if (ply < line.size() && moveBits::isCapture(line[ply])) weight *= UNSETTLED_FACTOR;

		const ControlValues control = computeControlValues(board, lineGenerator);
		for (std::size_t square = 0; square < control.values.size(); ++square) {
			mixed.values[square] += weight * control.values[square];
		}
		mixed.balance += weight * control.balance;
		totalWeight += weight;

		if (ply < line.size()) {
			captured = captured || moveBits::isCapture(line[ply]);
			board.makeMove(line[ply]);
		}
	}
	for (float& value : mixed.values) value /= totalWeight;
	doorman.setSquareValues(mixed.values, mixed.balance / totalWeight);
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

//the evaluator's criteria; the overall score comes from the engine's search instead, see showSearchScore()
void showEvaluation(Doorman& dm, const BoardState& state, Evaluator& eval) {
	const Position& pos = state.getPosition();
	EvalValue weighted_material = eval.weightedMaterial(pos);

	if (pos.side_to_move == BLACK) {
		weighted_material = -weighted_material;
	}
	
	dm.setCriterion(EvalCriterion::WEIGHTED_MATERIAL, criterionSquaresFromEvalValue(weighted_material));
}

//a search score (view of the side to move) as the overall card's centipawns or mate, from white's view
void showSearchScore(Doorman& doorman, EvalValue score, Color sideToMove) {
	constexpr EvalValue MATE_THRESHOLD = 900.0f;	//|score| above this is a mate, see SearchLimits::MATE
	const EvalValue white = sideToMove == WHITE ? score : -score;
	if (std::abs(white) >= MATE_THRESHOLD && std::abs(white) <= -SearchLimits::MATE) {
		const int plies = static_cast<int>(std::lround(-SearchLimits::MATE - std::abs(white)));
		const int moves = (plies + 1) / 2;
		doorman.setOverallMate(white > 0 ? moves : -moves);
		return;
	}
	doorman.setOverallCentipawns(static_cast<int>(std::lround(white * 100)));
}

//analysis with arrows drawn: only the moves the user drew are searched, each from the position after it, one ply
//less deep. The searcher can't restrict its root moves, so the root is done here; the recorder is told about it
//like the searcher would, so the search window shows the same tree shape
SearchResult searchCandidates(Searcher& searcher, SearchObserver& observer, BoardState board, LegalMoveGenerator generator,
	Evaluator evaluator, int depth, const std::vector<Move>& candidates) {
	SearchResult best{0, SearchLimits::MINUS_INFINITY, depth};
	for (Move move : candidates) {
		const SearchInfo info{depth - 1, 1, -SearchLimits::PLUS_INFINITY, -best.evaluation, std::nullopt};
		observer.enterNode(move, info);
		board.makeMove(move);
		const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
		const GameStatus status = board.getGameStatus(generator.generateLegalMovesInPosition(position, board));
		EvalValue score = 0;
		if (status.result == GameResult::WHITE_WIN || status.result == GameResult::BLACK_WIN) {
			score = -(SearchLimits::MATE + 1);	//the opponent is mated right away
		} else if (status.reason != GameEndingReason::NONE) {
			score = 0;
		} else if (depth <= 1) {
			score = -evaluator.overallScore(position);
		} else {
			score = -searcher.searchPosition(board, generator, evaluator, depth - 1, SearchLimits::NO_SEARCH_TIME_LIMIT).evaluation;
		}
		board.unmakeMove();
		observer.leaveNode(score);
		if (score > best.evaluation) {
			best.bestMove = move;
			best.evaluation = score;
		}
	}
	return best;
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
	unsigned positionVersion = 0;	//counts changes that outdate a running search (position, arrows drawn), its answer is dropped then
	unsigned scoreVersion = 0;	//the version the overall score was last shown for
	Move engineArrowMove = 0;	//analysis: the engine's best move for that version, drawn if the user wants the arrow
	bool replayingLine = false;	//a line from the search window is being played: no searches in between
	std::function<void()> requestSearch;	//below: whatever the shown view wants from the engine for the core's position

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

	//the core's position changed: the engine's arrow is outdated right away, the score once a new one is overdue
	const auto onPositionChanged = [&] {
		showDemoValues(doorman, board, legalMoveGenerator);
		showEvaluation(doorman, board, evaluator);
		legalMoves.count = 0;
		++positionVersion;
		updateGameStatus();
		doorman.clearEngineAnnotations();
		engineArrowMove = 0;
		QTimer::singleShot(300, &doorman, [&, version = positionVersion] {
			if (version == positionVersion && scoreVersion != version && window.getViewMode() == ViewMode::ANALYSIS) {
				doorman.clearOverallEval();
			}
		});
		requestSearch();
	};

	//a verified move, from the user or the engine
	const auto applyMove = [&](Move move) {
		const std::string label = MoveNotation::buildMoveString(move, board.getPosition());	//before the move is made
		board.makeMove(move);
		doorman.playMove(move, label);				//advances or forks the move tree
		doorman.setPosition(board.getPosition());	//the core's position is authoritative
		onPositionChanged();

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

	//engine: one search at a time, on a copy of the core's board in its own thread so the GUI stays responsive
	//	play view		the engine's side is to move: the answer is played like a user move, the board is locked meanwhile
	//	analysis view	every position is searched: overall score and an arrow for the best move, both live while searching
	//the answer comes back to the GUI thread and is dropped if it is outdated; a request while a search runs is
	//remembered and started afterwards (searches can't be stopped yet)
	enum class SearchPurpose { PLAY, ANALYSIS };
	Searcher searcher;
	SearchRecorder searchRecorder;	//writes down what the search does, for the search window (W) and the live arrow
	searcher.setObserver(&searchRecorder);
	window.setSearchRecorder(searchRecorder);
	std::thread searchThread;
	bool engineThinking = false;
	bool searchAgain = false;	//asked again while thinking
	SearchPurpose runningPurpose = SearchPurpose::PLAY;
	std::vector<Move> searchedCandidates;	//analysis: the moves of the last search, empty for all

	const auto showEngineArrow = [&](Move move) {
		engineArrowMove = move;
		if (move != 0 && window.isEngineArrowShown()) {
			doorman.setEngineArrows({Arrow(move)});
		}
	};

	const auto getViewPurpose = [&] {
		return window.getViewMode() == ViewMode::PLAY ? SearchPurpose::PLAY : SearchPurpose::ANALYSIS;
	};

	//analysis: the legal moves the user drew arrows for
	const auto getCandidates = [&] {
		std::vector<Move> candidates;
		const std::vector<Arrow> arrows = doorman.getUserArrows();
		if (arrows.empty()) {
			return candidates;
		}
		const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
		const MoveList legal = legalMoveGenerator.generateLegalMovesInPosition(position, board);
		for (std::size_t index = 0; index < legal.count; ++index) {
			const Move move = legal.moves[index];
			const bool drawn = std::any_of(arrows.begin(), arrows.end(), [move](const Arrow& arrow) {
				return arrow.toMove() == moveBits::getFromTo(move);
			});
			if (drawn) {
				candidates.push_back(move);	//all promotions of an arrow
			}
		}
		return candidates;
	};

	const auto startSearch = [&](SearchPurpose purpose) {
		if (searchThread.joinable()) {
			searchThread.join();	//the previous search is done already, its answer has arrived
		}
		engineThinking = true;
		searchAgain = false;
		runningPurpose = purpose;
		const unsigned version = positionVersion;
		const int depth = purpose == SearchPurpose::PLAY ? window.getPlayDepth() : window.getAnalysisDepth();
		const NodeId origin = doorman.getCurrentTreeNode();
		searchedCandidates = purpose == SearchPurpose::ANALYSIS ? getCandidates() : std::vector<Move>{};
		const int quiescenceDepth = purpose == SearchPurpose::PLAY ? window.getPlayQuiescenceDepth() : window.getAnalysisQuiescenceDepth();
		searcher.setMaxQuiescenceDepth(quiescenceDepth);
		searchThread = std::thread([&, version, depth, quiescenceDepth, origin, purpose, candidates = searchedCandidates,
			boardCopy = board, generatorCopy = legalMoveGenerator, evaluatorCopy = evaluator]() mutable {
			searchRecorder.begin(boardCopy, depth, origin, version, candidates, quiescenceDepth);
			const SearchResult result = candidates.empty()
				? searcher.searchPosition(boardCopy, generatorCopy, evaluatorCopy, depth, SearchLimits::NO_SEARCH_TIME_LIMIT)
				: searchCandidates(searcher, searchRecorder, boardCopy, generatorCopy, evaluatorCopy, depth, candidates);
			searchRecorder.finish(result);
			QMetaObject::invokeMethod(&doorman, [&, version, purpose, result] {
				engineThinking = false;
				const bool again = searchAgain;
				searchAgain = false;
				const bool current = version == positionVersion && purpose == getViewPurpose();
				if (current && purpose == SearchPurpose::PLAY && result.bestMove != 0) {
					applyMove(result.bestMove);	//unlocks the board again through the game status
					return;
				}
				if (current && purpose == SearchPurpose::ANALYSIS) {
					showSearchScore(doorman, result.evaluation, board.getPosition().side_to_move);
					scoreVersion = version;
					showEngineArrow(result.bestMove);
					//the board control map looks a few plies down the best line now
					const std::shared_ptr<const SearchSnapshot> snapshot = searchRecorder.takeSnapshot(0);
					if (snapshot && snapshot->finished && snapshot->tag == version) {
						showBestLineValues(doorman, snapshot, legalMoveGenerator);
					}
				}
				//outdated (position or view changed meanwhile), no move at all, or an analysis
				if (getViewPurpose() == SearchPurpose::PLAY || purpose == SearchPurpose::PLAY) {
					doorman.setBoardInteractive(doorman.getGameStatus().result == GameResult::NONE);
				}
				if (again || !current) {
					requestSearch();
				}
			}, Qt::QueuedConnection);
		});
	};

	requestSearch = [&] {
		if (replayingLine) {
			return;
		}
		const bool over = doorman.getGameStatus().result != GameResult::NONE;
		const SearchPurpose purpose = getViewPurpose();
		if (purpose == SearchPurpose::PLAY) {
			if (over || board.getPosition().side_to_move != ENGINE_COLOR) {
				return;
			}
			doorman.setBoardInteractive(false);	//right away, also while an earlier search still has to finish
		} else if (over) {
			return;
		}
		if (engineThinking) {
			searchAgain = true;
			return;
		}
		startSearch(purpose);
	};

	//analysis: the best move so far as an arrow and its score, twice a second while searching
	QTimer liveTimer;
	std::uint64_t liveGeneration = 0;
	liveTimer.setInterval(500);
	QObject::connect(&liveTimer, &QTimer::timeout, &doorman, [&] {
		if (!engineThinking || runningPurpose != SearchPurpose::ANALYSIS || window.getViewMode() != ViewMode::ANALYSIS) {
			return;
		}
		if (const std::shared_ptr<const SearchSnapshot> snapshot = searchRecorder.takeSnapshot(liveGeneration)) {
			liveGeneration = snapshot->generation;
			if (snapshot->tag == positionVersion && !snapshot->finished && snapshot->bestMoveSoFar != 0) {
				showEngineArrow(snapshot->bestMoveSoFar);
				showSearchScore(doorman, snapshot->bestScoreSoFar, snapshot->root.getPosition().side_to_move);
				scoreVersion = positionVersion;
			}
		}
		searchRecorder.requestSnapshot();	//picked up at the next tick
	});
	liveTimer.start();

	//play view: the engine's side is to move (the depth is read from the window when the search starts)
	QObject::connect(&doorman, &Doorman::engineMoveRequested, &doorman, [&](const Position&, int) {
		requestSearch();
	});

	//analysis: arrows drawn or deleted change which moves are searched
	QObject::connect(&doorman, &Doorman::userAnnotationsChanged, &doorman, [&](const std::vector<Highlight>&, const std::vector<Arrow>&) {
		if (window.getViewMode() != ViewMode::ANALYSIS || getCandidates() == searchedCandidates) {
			return;
		}
		++positionVersion;	//the running search answers another question now
		requestSearch();
	});

	QObject::connect(&window, &MainWindow::engineArrowShownChanged, &doorman, [&](bool shown) {
		if (shown && engineArrowMove != 0 && window.getViewMode() == ViewMode::ANALYSIS) {
			doorman.setEngineArrows({Arrow(engineArrowMove)});
		} else {
			doorman.clearEngineAnnotations();
		}
	});

	QObject::connect(&window, &MainWindow::analysisDepthChanged, &doorman, [&](int) {
		++positionVersion;
		requestSearch();
	});
	QObject::connect(&window, &MainWindow::analysisQuiescenceDepthChanged, &doorman, [&](int) {
		++positionVersion;
		requestSearch();
	});

	//the view changed: the analysis searches every position, the play view only when it is the engine's turn
	QObject::connect(&window, &MainWindow::viewModeChanged, &doorman, [&](ViewMode mode) {
		doorman.clearEngineAnnotations();
		engineArrowMove = 0;
		if (mode == ViewMode::ANALYSIS) {
			doorman.setBoardInteractive(doorman.getGameStatus().result == GameResult::NONE);	//a play search may still run
		}
		requestSearch();
	});

	//the user jumped to another move in the tree: bring the core to the same position
	QObject::connect(&doorman, &Doorman::treeNodeSelected, &doorman, [&](NodeId id, const Position&) {
		replayMoves(board, doorman.getMovesToTreeNode(id));
		onPositionChanged();
	});

	//a move was clicked in the search window: play the line up to it in the analysis
	QObject::connect(&window, &MainWindow::searchLineChosen, &doorman, [&](NodeId origin, const std::vector<Move>& moves) {
		replayingLine = true;
		window.setViewMode(ViewMode::ANALYSIS);
		doorman.setCurrentTreeNode(origin);
		replayMoves(board, doorman.getMovesToTreeNode(origin));
		onPositionChanged();
		for (Move move : moves) {
			const Position position = board.getPosition();	//copy: the generator makes and unmakes moves on board
			if (!containsMove(legalMoveGenerator.generateLegalMovesInPosition(position, board), move)) {
				break;	//the tree changed since that search
			}
			applyMove(move);
		}
		replayingLine = false;
		doorman.setBoardInteractive(doorman.getGameStatus().result == GameResult::NONE);
		requestSearch();
	});

	window.show();
	requestSearch();	//the analysis of the starting position
	//first start, or the marker file next to the saved games is gone or was changed
	if (!TutorialOverlay::isMarkerCurrent()) {
		QTimer::singleShot(400, &window, &MainWindow::startTutorial);	//once the window has settled
	}
	const int exitCode = QApplication::exec();
	if (searchThread.joinable()) {
		searchThread.join();	//the search can't be stopped yet, closing mid-search waits for it to finish
	}
	return exitCode;
}
