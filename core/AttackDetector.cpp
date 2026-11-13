#include "AttackDetector.hpp"

#include <cstdint>

#include "DataTypes.hpp"
#include "Bitboard.hpp"

#include "AttackTables.hpp"

bool isSquareAttacked(Square sq, Color byColor, const Position& pos) {
	return attackersToSquare(sq, byColor, pos) != 0;
}

Bitboard attackersToSquare(Square sq, Color byColor, const Position& pos) {
	const Bitboard* attackers = pos.pieces[byColor];
	const Bitboard attacker_occ = pos.color_occupancy[byColor];
	const Bitboard blocker_occ = pos.total_occupancy & ~attacker_occ;

	//look from sq outwards, each piece type only counts on its own pattern
	const Bitboard diagonals = getBishopAttacks(sq, blocker_occ, attacker_occ);
	const Bitboard lines = getRookAttacks(sq, blocker_occ, attacker_occ);

	return (getPawnAttacks(!byColor, sq) & attackers[PAWN])
		| (getKnightAttacks(sq) & attackers[KNIGHT])
		| (diagonals & (attackers[BISHOP] | attackers[QUEEN]))
		| (lines & (attackers[ROOK] | attackers[QUEEN]))
		| (getKingAttacks(sq) & attackers[KING]);
}

bool isInCheck(Color color, const Position& pos) {
	return checkersToColor(color, pos) != 0;
}

Bitboard checkersToColor(Color color, const Position& pos) {

	Square king_square = BB::getSquareFromOneSquareBitboard(pos.pieces[color][PieceType::KING]);
	return attackersToSquare(king_square, !color, pos);
}
