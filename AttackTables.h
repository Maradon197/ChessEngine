#ifndef ATTACKTABLES
#define ATTACKTABLES

#include "DataTypes.h"
#include "Bitboard.h"
#include <cstdint>

void initAttackTables();

Bitboard getPawnAttacks(Color color, Square sq);
Bitboard getKnightAttacks(Square sq);
Bitboard getKingAttacks(Square sq);

Bitboard getBishopAttacks(Square sq, Bitboard occupancy);
Bitboard getRookAttacks(Square sq, Bitboard occupancy);
Bitboard getQueenAttacks(Square sq, Bitboard occupancy);

#endif
