#include "Searcher.hpp"
#include "SearchObserver.hpp"

#include <iostream>
#include<algorithm>

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

#include "BoardState.hpp"
#include "LegalMoveGenerator.hpp"


//"I" refers to the view of the currently examined player
// -> not necessarily the player to move (at root level)!
EvalValue Searcher::recursiveSearch(SearchInfo current_info) {

	//am I a leaf vertex?
	if (current_info.remainingDepth == 0)
		//TODO: performance fix return quiescenceRecursiveSearch(current_info);
		return evaluator.overallScore(board.getPosition());

	//guess I'll have to do the work if im here
	//...
	//get information about my current position
	const Position& pos = board.getPosition();
	MoveList legal_moves = generator.generateLegalMovesInPosition(pos, board);

	GameStatus status = board.getGameStatus(legal_moves);

	//did I get checkmated?
	if (status.result == GameResult::WHITE_WIN || status.result == GameResult::BLACK_WIN)
		return static_cast<EvalValue>(SearchLimits::MATE + current_info.currentDepth);

	//is it a draw?
	if (status.reason != GameEndingReason::NONE)
		return static_cast<EvalValue>(0.0f);

	for (std::size_t i = 0; i < legal_moves.count; ++i) {
		board.makeMove(legal_moves.moves[i]);

		SearchInfo child_node = {
			current_info.remainingDepth -1,
			current_info.currentDepth +1,
			- current_info.mostAllowedScore,		//new alpha = -beta
			- current_info.minGuaranteedScore		//and vice versa
		};

		//And way dooooown we goo
		if (observer) observer->enterNode(legal_moves.moves[i], child_node);
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

	MoveList legal_moves = generator.generateLegalMovesInPosition(board.getPosition(), board);

	for (std::size_t i = 0; i < legal_moves.count; ++i) {

		const Move move = legal_moves.moves[i];
		if (!(move & (Move{ 1 } << 18))) continue;	//is capture?

		board.makeMove(move);

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
