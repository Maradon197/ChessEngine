#ifndef BOARDSTATE
#define BOARDSTATE

#include <stack>

#include "DataTypes.hpp"

class BoardState {
private:
	Position position{};
	std::stack<Position> undoable_history{};

	void updateOccupanciesToMatchPieceBB();
public:
	void setStartingPosition();

	bool makeMove(Move m);
	int unmakeMove();

	const Position& getPosition();
};
#endif
