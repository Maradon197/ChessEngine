#ifndef INCLUDED__CHESS_ENGINE__STRESSED_RHEA_HPP
#define INCLUDED__CHESS_ENGINE__STRESSED_RHEA_HPP

//Why is this inclusion guard called "stressed rhea"?
//---------------------------------------------------
//	I want to give the engine some name with "Rhea" in it.
//	While writing the (highly performance critical) code here,
//	I often pictured a Rhea frantically trying to process as much
//	code as possible as quickly as possible, scrambling to find
//	a move that doesn't lose.
//	...
//	In my imagination, the rhea wasn't too happy.

//	(and calling my search class "stressed rhea" as a whole wouldn't
//	 have been too good for readability and navigatability, especially
//	 when *you* are reading the code and not me.)
//	Enjoy!


#include "EngineTypes.hpp"
#include "DataTypes.hpp"
#include "BoardState.hpp"
#include "LegalMoveGenerator.hpp"
#include "Evaluator.hpp"

struct SearchLimits {
	//no search, not even iterative, should go beyond this 
	// -> signal value for iterative search and general limit in one value
	static constexpr int ITERATIVE_DEEPENING = 128;	
	static constexpr uint64_t NO_SEARCH_NODE_LIMIT = 0;
	static constexpr int64_t NO_SEARCH_TIME_LIMIT = 0;

	static constexpr EvalValue MATE = -1000.0f;
	static constexpr EvalValue MINUS_INFINITY = -1001.0f;
	static constexpr EvalValue PLUS_INFINITY = 1001.0f;
	
	int maxDepth = ITERATIVE_DEEPENING;
	int64_t timeMs = NO_SEARCH_TIME_LIMIT;
	uint64_t maxNodes = NO_SEARCH_NODE_LIMIT;
};


//To be passed down recursive search calls
struct SearchInfo {
	int remainingDepth;
	int currentDepth;

	EvalValue min_guaranteed_score;	//alpha
	EvalValue most_allowed_score;	//beta
};

//to be returned
struct SearchResult {
	Move bestMove;
	EvalValue evaluation;
	int calculatedDepth;
	//to be added to this: probably lots
};

class Searcher {
private:
	bool stopSearchFlag;
	SearchLimits limits;
	BoardState board;
	LegalMoveGenerator generator;
	Evaluator evaluator;
	EvalValue recursiveSearch(SearchInfo current_info);
	EvalValue quiescenceSearch(SearchInfo current_info);

public:
	void setStopSearchFlag();
	SearchResult searchPosition(BoardState& board, LegalMoveGenerator& gen, 
		Evaluator& eval, int depth, int64_t max_time_ms);
};

//TODO: create source file, implement recursive search!

#endif //!INCLUDED__CHESS_ENGINE__STRESSED_RHEA_HPP
