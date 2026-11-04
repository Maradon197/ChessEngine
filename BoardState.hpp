#ifndef BOARDSTATE
#define BOARDSTATE
#include "DataTypes.hpp"
#include <stack>

class BoardState {
private:
	Position position{};
	std::stack<UndoState> undoable_history{};
public:
	void setStartingPosition();

	//TODO: implement
	bool makeMove(Move m);
	void unmakeMove();

	const Position& getPosition();
};
#endif