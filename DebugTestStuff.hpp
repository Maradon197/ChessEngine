#ifndef DEBUGTESTSTUFF
#define DEBUGTESTSTUFF

#include "DataTypes.h"
#include "Bitboard.h"
#include <iostream>

Bitboard getBishopAttacksSlow(Square source, Bitboard occupancy);
Bitboard getRookAttacksSlow(Square source, Bitboard occupancy);
Bitboard getQueenAttacksSlow(Square source, Bitboard occupancy);

#endif