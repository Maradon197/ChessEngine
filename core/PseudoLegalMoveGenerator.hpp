#ifndef PSEUDOLEGALMOVEGENERATOR
#define PSEUDOLEGALMOVEGENERATOR

#include "DataTypes.hpp"
#include "Bitboard.hpp"

MoveList getPseudoLegalMovesAtSquare(const Position& pos, Square source);
MoveList getPseudoLegalMovesInPosition(const Position& pos);
MoveList getPseudoLegalCapturesInPosition(const Position& pos);	//only captures (incl. en passant), e.g. for quiescence

#endif
