#include "Searcher.hpp"

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

#include "BoardState.hpp"
#include "LegalMoveGenerator.hpp"


//"I" refers to the view of the currently examined player
// -> not necessarily the player to move (at root level) right now!
EvalValue Searcher::recursiveSearch(SearchInfo current_info) {
	const Position& pos = board.getPosition();
	MoveList legal_moves = generator.generateLegalMovesInPosition(pos, board);

	GameStatus status = board.getGameStatus(legal_moves);

	//did I get checkmated?
	if (status.result == GameResult::WHITE_WIN || status.result == GameResult::BLACK_WIN)
		return static_cast<EvalValue>(SearchLimits::MATE + current_info.currentDepth);

	//is it a draw?
	if (status.reason != GameEndingReason::NONE)
		return static_cast<EvalValue>(0.0f);

	//am I a leaf vertex?
	if (current_info.remainingDepth == 0)
		return quiescenceSearch(current_info);

	for (std::size_t i = 0; i < legal_moves.count; ++i) {
		board.makeMove(legal_moves.moves[i]);

		SearchInfo shove_down_there = {
			current_info.remainingDepth -1,
			current_info.currentDepth +1,
			- current_info.most_allowed_score,		//new alpha = -beta
			- current_info.min_guaranteed_score		//and vice versa
		};

		//And way dooooown we goo (my RAM will hate me)
		EvalValue search_result = - recursiveSearch(shove_down_there);

		board.unmakeMove();

		//is the move so good my opponent won't allow it?
		// -> stop right here
		// (beta cutoff)
		if (search_result > current_info.most_allowed_score)
			return current_info.most_allowed_score;

		//is this my best move so far?
		// -> raise alpha, keep searching
		if (search_result > current_info.min_guaranteed_score)
			current_info.min_guaranteed_score = search_result;
	}

	return current_info.min_guaranteed_score;
}


//TODO: Actual quiescene search
//this is just a lazy workaround to have the right 
//structure established in recursiveSearch
EvalValue Searcher::quiescenceSearch(SearchInfo) {
	return evaluator.overallScore(board.getPosition());
}


SearchResult Searcher::searchPosition(BoardState& board, LegalMoveGenerator& gen,
	Evaluator& eval, int depth, int64_t max_time_ms) {
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
			depth - 1,
			1,
			-SearchLimits::PLUS_INFINITY,	//child's alpha = -root beta
			-best_score						//child's beta  = -root alpha
		};
		EvalValue search_result = -recursiveSearch(first_child);

		this->board.unmakeMove();

		//new best root move? -> remember the score AND the move
		if (search_result > best_score) {
			best_score = search_result;
			best_move = legal_moves.moves[i];
		}
	}

	return { best_move, best_score, depth };
}
