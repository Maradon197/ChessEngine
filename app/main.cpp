#include <algorithm>
#include <bit>
#include <string>
#include <vector>
#include <iostream>

#include <QApplication>

#include "DataTypes.hpp"

#include "AttackDetector.hpp"
#include "AttackTables.hpp"
#include "BoardState.hpp"
#include "Doorman.hpp"
#include "LegalMoveGenerator.hpp"
#include "MainWindow.hpp"
#include "MoveNotation.hpp"

#include "Evaluator.hpp"

//temporary includes -----------------

//------------------------------------

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

} // !namespace

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);
	initAttackTables();

	Doorman doorman;
	MainWindow window(doorman);
	Evaluator evaluator;

	//core state
	BoardState board;
	board.setStartingPosition();
	LegalMoveGenerator legalMoveGenerator{};
	MoveList legalMoves{};

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

	//a verified move, from the user or (later) the engine
	const auto applyMove = [&](Move move) {
		const std::string label = MoveNotation::buildMoveString(move, board.getPosition());	//before the move is made
		board.makeMove(move);
		doorman.playMove(move, label);				//advances or forks the move tree
		doorman.setPosition(board.getPosition());	//the core's position is authoritative
		showDemoValues(doorman, board, legalMoveGenerator);
		showEvaluation(doorman, board, evaluator);
		legalMoves.count = 0;
		updateGameStatus();
	};

	//the user tried a move: only legal moves are played, everything else snaps back
	QObject::connect(&doorman, &Doorman::moveAttempted, &doorman, [&](Move move) {
		if (!containsMove(legalMoves, move)) {
			return;
		}
		applyMove(move);
	});

	//play view: the engine's side is to move. There is no search yet, so nothing happens and the user can move for it.
	//later: search this position (off the GUI thread), then pass the best move to applyMove in the GUI thread
	QObject::connect(&doorman, &Doorman::engineMoveRequested, &doorman, [&](const Position& position) {
		Q_UNUSED(position);
	});

	//the user jumped to another move in the tree: bring the core to the same position
	QObject::connect(&doorman, &Doorman::treeNodeSelected, &doorman, [&](NodeId id, const Position&) {
		replayMoves(board, doorman.getMovesToTreeNode(id));
		showDemoValues(doorman, board, legalMoveGenerator);
		showEvaluation(doorman, board, evaluator);
		legalMoves.count = 0;
		updateGameStatus();
	});

	window.show();
	return QApplication::exec();
}
