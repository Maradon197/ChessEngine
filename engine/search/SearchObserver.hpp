#ifndef INCLUDED__CHESS_ENGINE__SEARCH_OBSERVER_HPP
#define INCLUDED__CHESS_ENGINE__SEARCH_OBSERVER_HPP

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

struct SearchInfo;

class SearchObserver {
public:
	virtual ~SearchObserver() = default;

	//a move was just made, its subtree is about to be searched with child_info
	//(alpha / beta are from the view of the player to move AFTER the move)
	virtual void enterNode(Move move, const SearchInfo& child_info) = 0;

	//the subtree of the last entered move is done
	virtual void leaveNode(EvalValue score) = 0;
};

#endif //!INCLUDED__CHESS_ENGINE__SEARCH_OBSERVER_HPP
