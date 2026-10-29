#ifndef BOARDSTATE
#define BOARDSTATE
#include "DataTypes.hpp"

class BoardState {
private:
	Position position{};
public:
	void setStartingPosition();
	//void setNewWholePosition(Position new_position);

	//void acceptMove(Move);

	const Position& getPosition() const;
};
#endif