#include "DataTypes.h"
#include "Bitboard.h"
#include "AttackTables.h"

namespace {
	//lookup tables for piece attacks
	Bitboard pawn_attacks[2][64]{};
	Bitboard knight_attacks[64]{};
	Bitboard king_attacks[64]{};

	Bitboard bishop_rays[64]{};
	Bitboard rook_rays[64]{};
	Bitboard queen_rays[64]{};

	//Sliding piece magic bb?
}

void initPawnAttacks() {
	//how I would do it:
	for (Square sq = 0; sq < 64; ++sq) {
		if (sq % 8 != 0) { //not on left edge: add left attack
			pawn_attacks[WHITE][sq] |= BB::oneSquareBB(sq + 7);
		}
		if (sq % 8 != 7) { //not on right edge: add right attack
			pawn_attacks[WHITE][sq] |= BB::oneSquareBB(sq + 9);
		}
		if (sq % 8 != 0) { //not on left edge: add left attack
			pawn_attacks[BLACK][sq] |= BB::oneSquareBB(sq - 9);
		}
		if (sq % 8 != 7) { //not on right edge: add right attack
			pawn_attacks[BLACK][sq] |= BB::oneSquareBB(sq - 7);
		}
	}	
}

void initKnightAttacks() {
	for (Square sq = 0; sq < 64; ++sq) {
		if (sq % 8 > 0) {
			if (sq / 8 > 1) {
				knight_attacks[sq] |= BB::oneSquareBB(sq - 17);
			}
			if (sq / 8 < 6) {
				knight_attacks[sq] |= BB::oneSquareBB(sq + 15);
			}
		}
		if (sq % 8 > 1) {
			if (sq / 8 > 0) {
				knight_attacks[sq] |= BB::oneSquareBB(sq - 10);
			}
			if (sq / 8 < 7) {
				knight_attacks[sq] |= BB::oneSquareBB(sq + 6);
			}
		}

		if(sq % 8 < 7) {
			if (sq / 8 > 1) {
				knight_attacks[sq] |= BB::oneSquareBB(sq - 15);
			}
			if (sq / 8 < 6) {
				knight_attacks[sq] |= BB::oneSquareBB(sq + 17);
			}
		}
		if (sq % 8 < 6) {
			if (sq / 8 > 0) {
				knight_attacks[sq] |= BB::oneSquareBB(sq - 6);
			}
			if (sq / 8 < 7) {
				knight_attacks[sq] |= BB::oneSquareBB(sq + 10);
			}
		}
	}
}

void initKingAttacks() {
	for (Square sq = 0; sq < 64; ++sq) {
		if (sq % 8 > 0) {
			king_attacks[sq] |= BB::oneSquareBB(sq - 1);
			if (sq / 8 > 0) {
				king_attacks[sq] |= BB::oneSquareBB(sq - 9);
			}
			if (sq / 8 < 7) {
				king_attacks[sq] |= BB::oneSquareBB(sq + 7);
			}
		}
		if (sq % 8 < 7) {
			king_attacks[sq] |= BB::oneSquareBB(sq + 1);
			if (sq / 8 > 0) {
				king_attacks[sq] |= BB::oneSquareBB(sq - 7);
			}
			if (sq / 8 < 7) {
				king_attacks[sq] |= BB::oneSquareBB(sq + 9);
			}
		}
		if (sq / 8 > 0) {
			king_attacks[sq] |= BB::oneSquareBB(sq - 8);
		}
		if (sq / 8 < 7) {
			king_attacks[sq] |= BB::oneSquareBB(sq + 8);
		}
	}
}

void initBishopRays() {
	for(Square sq = 0; sq < 64; ++sq) {
		bishop_rays[sq] = BB::getDiagonalMask(sq);
	}
}

void initRookRays() {
	for (Square sq = 0; sq < 64; ++sq) {
		rook_rays[sq] = BB::getFileRankMask(sq);
	}
}

void initQueenRays() {
	for (Square sq = 0; sq < 64; ++sq) {
		queen_rays[sq] = BB::getDiagonalMask(sq) | BB::getFileRankMask(sq);
	}
}


void initAttackTables() {
	initPawnAttacks();
	initKnightAttacks();
	initKingAttacks();

	initBishopRays();
	initRookRays();
	initQueenRays();
}


Bitboard getPawnAttacks(Color color, Square sq) {
	return pawn_attacks[color][sq];
}

Bitboard getKnightAttacks(Square sq) {
	return knight_attacks[sq];
}

Bitboard getKingAttacks(Square sq) {
	return king_attacks[sq];
}