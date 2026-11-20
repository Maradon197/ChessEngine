#include "Evaluator.hpp"

#include "DataTypes.hpp"
#include "EngineTypes.hpp"
#include "Bitboard.hpp"

EvalValue Evaluator::overallScore(const Position& pos) {
	//this is a very shit implementation. The overall score needs
	//the principle evaluator network online to function properly. 
	//Then, it'll be independant from the weighted material assesor,
	//which simply becomes one input to the overall score.
	//for now, I just steal the weighted material evaluation.

	return Evaluator::weightedMaterial(pos);
}

namespace {
	constexpr int MATERIAL[6] = { 100, 320, 330, 500, 900, 0 };

	//piece-square tables from White's view, Values: Michniewski's "simplified evaluation function"
	constexpr int PAWN_PST[64] = {
		  0,  0,  0,  0,  0,  0,  0,  0,
		  5, 10, 10,-20,-20, 10, 10,  5,
		  5, -5,-10,  0,  0,-10, -5,  5,
		  0,  0,  0, 20, 20,  0,  0,  0,
		  5,  5, 10, 25, 25, 10,  5,  5,
		 10, 10, 20, 30, 30, 20, 10, 10,
		 50, 50, 50, 50, 50, 50, 50, 50,
		  0,  0,  0,  0,  0,  0,  0,  0 };

	constexpr int KNIGHT_PST[64] = {
		-50,-40,-30,-30,-30,-30,-40,-50,
		-40,-20,  0,  5,  5,  0,-20,-40,
		-30,  5, 10, 15, 15, 10,  5,-30,
		-30,  0, 15, 20, 20, 15,  0,-30,
		-30,  5, 15, 20, 20, 15,  5,-30,
		-30,  0, 10, 15, 15, 10,  0,-30,
		-40,-20,  0,  0,  0,  0,-20,-40,
		-50,-40,-30,-30,-30,-30,-40,-50 };

	constexpr int BISHOP_PST[64] = {
		-20,-10,-10,-10,-10,-10,-10,-20,
		-10,  5,  0,  0,  0,  0,  5,-10,
		-10, 10, 10, 10, 10, 10, 10,-10,
		-10,  0, 10, 10, 10, 10,  0,-10,
		-10,  5,  5, 10, 10,  5,  5,-10,
		-10,  0,  5, 10, 10,  5,  0,-10,
		-10,  0,  0,  0,  0,  0,  0,-10,
		-20,-10,-10,-10,-10,-10,-10,-20 };

	constexpr int ROOK_PST[64] = {
		  0,  0,  0,  5,  5,  0,  0,  0,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		 -5,  0,  0,  0,  0,  0,  0, -5,
		  5, 10, 10, 10, 10, 10, 10,  5,
		  0,  0,  0,  0,  0,  0,  0,  0 };

	constexpr int QUEEN_PST[64] = {};

	constexpr int KING_MID_PST[64] = {
		 20, 30, 10,  0,  0, 10, 30, 20,
		 20, 20,  0,  0,  0,  0, 20, 20,
		-10,-20,-20,-20,-20,-20,-20,-10,
		-20,-30,-30,-40,-40,-30,-30,-20,
		-30,-40,-40,-50,-50,-40,-40,-30,
		-30,-40,-40,-50,-50,-40,-40,-30,
		-30,-40,-40,-50,-50,-40,-40,-30,
		-30,-40,-40,-50,-50,-40,-40,-30 };

	constexpr int KING_END_PST[64] = {	//walk to the center
		-50,-30,-30,-30,-30,-30,-30,-50,
		-30,-30,  0,  0,  0,  0,-30,-30,
		-30,-10, 20, 30, 30, 20,-10,-30,
		-30,-10, 30, 40, 40, 30,-10,-30,
		-30,-10, 30, 40, 40, 30,-10,-30,
		-30,-10, 20, 30, 30, 20,-10,-30,
		-30,-20,-10,  0,  0,-10,-20,-30,
		-50,-40,-30,-20,-20,-30,-40,-50 };

	constexpr const int* PST[5] = { PAWN_PST, KNIGHT_PST, BISHOP_PST, ROOK_PST, QUEEN_PST };

	constexpr int PHASE_WEIGHT[6] = { 0, 1, 1, 2, 4, 0 };	//24 = all pieces on board = pure middlegame
	constexpr int BISHOP_PAIR_BONUS = 30;
}

//@return: pawns (1.0 = one pawn) from the side to move's perspective, so negamax can use it directly
float Evaluator::weightedMaterial(const Position& pos) {
	int score[2] = { 0, 0 };		//W, B
	int kingMid[2] = { 0, 0 }, kingEnd[2] = { 0, 0 };
	int phase = 0;

	for (int color = WHITE; color <= BLACK; ++color) {
		for (int type = PAWN; type <= KING; ++type) {
			Bitboard bb = pos.pieces[color][type];
			phase += PHASE_WEIGHT[type] * BB::popCount(bb);
			while (bb) {
				Square sq = BB::getSquareFromOneSquareBitboard(BB::getLSBSquareBitboard(bb));
				bb &= bb - 1;
				if (color == BLACK) sq ^= 56;	//mirror rank so black can use the tables as well
				if (type == KING) { kingMid[color] = KING_MID_PST[sq]; kingEnd[color] = KING_END_PST[sq]; continue; }
				score[color] += MATERIAL[type] + PST[type][sq];
			}
		}
		if (BB::popCount(pos.pieces[color][BISHOP]) >= 2) score[color] += BISHOP_PAIR_BONUS;
	}

	//taper king between "hide" and "centralize" as pieces come off the board
	if (phase > 24) phase = 24;
	for (int color = WHITE; color <= BLACK; ++color)
		score[color] += (kingMid[color] * phase + kingEnd[color] * (24 - phase)) / 24;

	const int whiteView = score[WHITE] - score[BLACK];
	const int stmView = pos.side_to_move == WHITE ? whiteView : -whiteView;
	return (EvalValue) stmView / 100.0f;
}
