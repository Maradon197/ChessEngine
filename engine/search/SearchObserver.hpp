#ifndef INCLUDED__CHESS_ENGINE__SEARCH_OBSERVER_HPP
#define INCLUDED__CHESS_ENGINE__SEARCH_OBSERVER_HPP

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

struct SearchInfo;

//Someone watching the search from the outside, e.g. the GUI's search window.
//The Searcher calls these two functions around every recursive call, and does nothing if no observer is set.
//They run on the search thread, in the middle of the search: implementations have to be fast
//and take care of their own thread safety.
class SearchObserver {
public:
	virtual ~SearchObserver() = default;

	//a move was just made, its subtree is about to be searched with child_info
	//(alpha / beta are from the view of the player to move AFTER the move, exactly what the child gets)
	virtual void enterNode(Move move, const SearchInfo& child_info) = 0;

	//the subtree of the last entered move is done
	//score is from the view of the player who MADE the move, the value the parent compares to its alpha / beta
	virtual void leaveNode(EvalValue score) = 0;
};

#endif //!INCLUDED__CHESS_ENGINE__SEARCH_OBSERVER_HPP
