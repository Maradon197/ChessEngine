#ifndef BOARDSTATE
#define BOARDSTATE
#include "DataTypes.hpp"

class BoardState {
private:
	Position position{};
public:
	void setStartingPosition();

	//TODO: implement
	bool makeMove(Move m);
	void unmakeMove();

	const Position& getPosition();
};
#endif