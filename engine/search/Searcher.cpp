#include "Searcher.hpp"
#include "SearchObserver.hpp"

#include <iostream>
#include<algorithm>

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

#include "BoardState.hpp"
#include "LegalMoveGenerator.hpp"
#include "PseudoLegalMoveGenerator.hpp"
#include "AttackDetector.hpp"
#include "Bitboard.hpp"


namespace {
	constexpr Move CAPTURE_FLAG = Move{ 1 } << 18;

	bool isCapture(Move m) {
		return (m & CAPTURE_FLAG) != 0;
	}

	PieceType pieceTypeAt(const Position& pos, Color color, Square sq) {
		for (int type = PAWN; type <= KING; ++type)
			if (pos.pieces[color][type] & BB::oneSquareBB(sq))
				return static_cast<PieceType>(type);
		return NONE;
	}

	int moveOrderScore(Move m, const Position& pos) {
		if (!isCapture(m))
			return 0;
		PieceType victim = pieceTypeAt(pos, !pos.side_to_move, (m >> 6) & 0x3f);
		if (victim == NONE)
			victim = PAWN;	//en passant: the destination square is empty
		const PieceType attacker = pieceTypeAt(pos, pos.side_to_move, m & 0x3f);
		return (victim + 1) * 8 - attacker;
	}

	void orderMoves(MoveList& list, const Position& pos) {
		int scores[MAX_MOVES];
		for (std::size_t i = 0; i < list.count; ++i)
			scores[i] = moveOrderScore(list.moves[i], pos);

		for (std::size_t i = 1; i < list.count; ++i) {
			const Move move = list.moves[i];
			const int score = scores[i];
			std::size_t j = i;
			for (; j > 0 && scores[j - 1] < score; --j) {
				list.moves[j] = list.moves[j - 1];
				scores[j] = scores[j - 1];
			}
			list.moves[j] = move;
			scores[j] = score;
		}
	}

	//@return: whether m was made (if not, the board is unchanged)
	bool makeMoveIfLegal(BoardState& board, Move m) {
		const Color moving_side = board.getPosition().side_to_move;
		if (isCastlingThroughCheck(m, board.getPosition()))
			return false;

		board.makeMove(m);
		if (isInCheck(moving_side, board.getPosition())) {
			board.unmakeMove();
			return false;
		}
		return true;
	}
}//!namespace


//"I" refers to the view of the currently examined player
// -> not necessarily the player to move (at root level)!
EvalValue Searcher::recursiveSearch(SearchInfo current_info) {

	//am I a leaf vertex?
	if (current_info.remainingDepth == 0)
		return quiescenceRecursiveSearch(current_info);

	//guess I'll have to do the work if im here
	//...
	//is it a draw? (checked before the moves: a mate on the 100th halfmove counts as a draw here)
	if (board.getDrawStatus().reason != GameEndingReason::NONE)
		return static_cast<EvalValue>(0.0f);

	//pseudo-legal moves, legality is only checked when a move is actually tried
	MoveList moves = getPseudoLegalMovesInPosition(board.getPosition());
	orderMoves(moves, board.getPosition());
	bool found_legal_move = false;

	for (std::size_t i = 0; i < moves.count; ++i) {
		const Move move = moves.moves[i];
		if (!makeMoveIfLegal(board, move))
			continue;
		found_legal_move = true;

		SearchInfo child_node = {
			current_info.remainingDepth -1,
			current_info.currentDepth +1,
			- current_info.mostAllowedScore,		//new alpha = -beta
			- current_info.minGuaranteedScore		//and vice versa
		};

		//And way dooooown we goo
		if (observer) observer->enterNode(move, child_node);
		EvalValue search_result = - recursiveSearch(child_node);
		if (observer) observer->leaveNode(search_result);

		board.unmakeMove();

		//is the move so good my opponent won't allow it?
		// -> stop right here
		// (beta cutoff)
		if (search_result >= current_info.mostAllowedScore)
			return current_info.mostAllowedScore;

		//is this my best move so far?
		// -> raise alpha, keep searching
		if (search_result > current_info.minGuaranteedScore)
			current_info.minGuaranteedScore = search_result;
	}

	//no legal move at all: did I get checkmated, or is it stalemate?
	if (!found_legal_move) {
		const Position& pos = board.getPosition();
		if (isInCheck(pos.side_to_move, pos))
			return static_cast<EvalValue>(SearchLimits::MATE + current_info.currentDepth);
		return static_cast<EvalValue>(0.0f);
	}

	return current_info.minGuaranteedScore;
}

EvalValue Searcher::quiescenceRecursiveSearch(SearchInfo node) {
	EvalValue stand_pat = evaluator.overallScore(board.getPosition());

	if (node.currentDepth >= SearchLimits::MAX_QUIESCENCE_DEPTH)
		return stand_pat;

	//stand pat: I don't HAVE to capture
	if (stand_pat >= node.mostAllowedScore)
		return node.mostAllowedScore;
	if (stand_pat > node.minGuaranteedScore)
		node.minGuaranteedScore = stand_pat;

	MoveList moves = getPseudoLegalCapturesInPosition(board.getPosition());
	orderMoves(moves, board.getPosition());

	for (std::size_t i = 0; i < moves.count; ++i) {

		const Move move = moves.moves[i];
		if (!makeMoveIfLegal(board, move))
			continue;

		SearchInfo child = { node.remainingDepth - 1, node.currentDepth + 1,
			-node.mostAllowedScore, -node.minGuaranteedScore };
		EvalValue score = -quiescenceRecursiveSearch(child);

		board.unmakeMove();

		if (score >= node.mostAllowedScore) 
			return node.mostAllowedScore;

		if (score > node.minGuaranteedScore) 
			node.minGuaranteedScore = score;
	}
	return node.minGuaranteedScore;
}


SearchResult Searcher::searchPosition(BoardState& board, LegalMoveGenerator& gen,
	Evaluator& eval, int depth, int64_t max_time_ms) {
	int search_depth = std::min({ depth, SearchLimits::MAX_DEPTH });
	stopSearchFlag = false;
	this->board = board;
	this->generator = gen;
	this->evaluator = eval;

	const Position& pos = this->board.getPosition();
	MoveList legal_moves = generator.generateLegalMovesInPosition(pos, this->board);

	EvalValue best_score = SearchLimits::MINUS_INFINITY;	//root alpha
	Move best_move = 0;										//0 = "no move found"

	for (std::size_t i = 0; i < legal_moves.count; ++i) {
		this->board.makeMove(legal_moves.moves[i]);

		SearchInfo first_child = {
			search_depth - 1,
			1,
			-SearchLimits::PLUS_INFINITY,	//child's alpha = -root beta
			-best_score						//child's beta  = -root alpha
		};
		if (observer) observer->enterNode(legal_moves.moves[i], first_child);
		EvalValue search_result = -recursiveSearch(first_child);
		if (observer) observer->leaveNode(search_result);

		this->board.unmakeMove();

		//new best root move? -> remember the score AND the move
		if (search_result > best_score) {
			best_score = search_result;
			best_move = legal_moves.moves[i];
		}
	}

	return { best_move, best_score, search_depth };
}
