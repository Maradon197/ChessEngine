#ifndef BOARDSTATE
#define BOARDSTATE

#include <vector>

#include "DataTypes.hpp"

class BoardState {
private:
	Position position{};
	std::vector<Position> undoable_history{};

	void updateOccupanciesToMatchPieceBB();
public:
	void setStartingPosition();

	bool makeMove(Move m);
	int unmakeMove();

	const Position& getPosition() const;

	//rule-based game end of the current position, legalMoves must be ALL legal moves of it
	//(user bound game ending reasons are not the core's business, See GUI)
	GameStatus getGameStatus(const MoveList& legalMoves) const;

	//only the draw rules (insufficient material, fifty moves, repetition), no legal moves needed
	//-> checkmate / stalemate are NOT detected here, see getGameStatus
	GameStatus getDrawStatus() const;
};
#endif
