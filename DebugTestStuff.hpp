#ifndef DEBUGTESTSTUFF
#define DEBUGTESTSTUFF

#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include <iostream>

Bitboard getBishopAttacksSlow(Square source, Bitboard own_occupancy, Bitboard enemy_occupancy);
Bitboard getRookAttacksSlow(Square source, Bitboard own_occupancy, Bitboard enemy_occupancy);
Bitboard getQueenAttacksSlow(Square source, Bitboard own_occupancy, Bitboard enemy_occupancy);

#endif