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

	bool makeMove(Move m);

	//TODO: implement
	void unmakeMove();

	const Position& getPosition();
};
#endif