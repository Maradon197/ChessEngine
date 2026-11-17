#ifndef MOVENOTATION
#define MOVENOTATION

#include <string>

#include "DataTypes.hpp"

//short algebraic notation, e.g. "e4", "Nbd7", "Qxd4", "exd6 e.p.", "e8=Q", "0-0-0"
//pos is the position BEFORE the move is made, the move has to be legal in it
namespace MoveNotation {
	std::string buildMoveString(Move move, const Position& pos);
}

#endif
