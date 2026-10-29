#ifndef ATTACKTABLES
#define ATTACKTABLES

#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include <cstdint>

void initAttackTables();

Bitboard getPawnAttacks(Color color, Square sq);
Bitboard getKnightAttacks(Square sq);
Bitboard getKingAttacks(Square sq);

Bitboard getBishopAttacks(Square sq, Bitboard own_occupancy, Bitboard enemy_occupancy);
Bitboard getRookAttacks(Square sq, Bitboard own_occupancy, Bitboard enemy_occupancy);
Bitboard getQueenAttacks(Square sq, Bitboard own_occupancy, Bitboard enemy_occupancy);

#endif
