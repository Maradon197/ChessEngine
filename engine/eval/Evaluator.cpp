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

	//piece-square tables from White's view (row 1 = rank 1). 
	//Value source: Trust me bro (I came up with these)
	//...
	//...
	//You should most definately not trust me
	constexpr int PAWN_PST[64] = {
		   0,   0,   0,   0,   0,   0,   0,   0,
		   5,   5,   5,   0,   0,   5,   5,   5,
		   0,   0,   5,  10,  10,   5,   0,   0,
		  20,  20,  30,  35,  35,  30,  20,  20,
		  25,  25,  25,  30,  30,  25,  25,  25,
		  30,  30,  40,  40,  40,  40,  30,  30,
		  40,  50,  50,  50,  50,  50,  50,  40,
		   0,   0,   0,   0,   0,   0,   0,   0 };

	constexpr int KNIGHT_PST[64] = {
		 -50,   0, -30, -30, -30, -30,   0, -50,
		 -30, -20, -20, -10, -10, -20, -20, -30,
		 -30,  25,  30,  10,  10,  30,  25, -30,
		 -20,  20,  25,  30,  30,  25,  20, -20,
		 -20,  20,  20,  25,  25,  20,  20, -20,
		 -30,  15,  15,  20,  20,  15,  15, -30,
		 -30,   0,   0,   5,   5,   0,   0, -30,
		 -50, -30, -30, -30, -30, -30, -30, -50 };

	constexpr int BISHOP_PST[64] = {
		 -10, -20,   0, -10, -10,   0, -20, -10,
		   0,  30,  15,  20,  20,  15,  30,   0,
		  20,  25,  15,  10,  10,  15,  25,  20,
		  10,  20,  30,  10,  10,  30,  20,  10,
		  10,  25,  20,  20,  20,  20,  25,  10,
		  10,  15,  15,  15,  15,  15,  15,  10,
		   0,  10,  10,  10,  10,  10,  10,   0,
		 -10, -20,   0, -10, -10,   0, -20, -10 };

	constexpr int ROOK_PST[64] = {
		   0,  10,  20,  35,  35,  20,  10,   0,
		 -20, -10,   5,  15,  15,   5, -10, -20,
		  10,  10,   0,  20,  20,   0,  10,  10,
		   0,   0,   0,   0,   0,   0,   0,   0,
		   0,   0,   0,   0,   0,   0,   0,   0,
		   0,   0,   0,   0,   0,   0,   0,   0,
		  15,  15,  15,  25,  25,  15,  15,  15,
		 -20, -10,   0,  15,  15,   0, -10, -20 };

	constexpr int QUEEN_PST[64] = {};

	constexpr int KING_MID_PST[64] = {	//castle! only king safety signal for now
		  20,  60,  20, -20,   0, -30,  60,  40,
		 -20, -20, -20, -20, -20, -20, -20, -20,
		 -30, -30, -30, -30, -30, -30, -30, -30,
		 -40, -40, -40, -40, -40, -40, -40, -40,
		 -50, -50, -50, -50, -50, -50, -50, -50,
		 -60, -60, -60, -60, -60, -60, -60, -60,
		 -60, -60, -60, -60, -60, -60, -60, -60,
		 -60, -60, -60, -60, -60, -60, -60, -60 };

	constexpr int KING_END_PST[64] = {	//walk to the center
		 -30, -25, -20, -15, -15, -20, -25, -30,
		 -15, -10,  -5,   0,   0,  -5, -10, -15,
		   0,   5,  10,  15,  15,  10,   5,   0,
		   0,  10,  20,  30,  30,  20,  10,   0,
		   0,  10,  20,  30,  30,  20,  10,   0,
		   0,   5,  10,  15,  15,  10,   5,   0,
		 -15, -10,  -5,   0,   0,  -5, -10, -15,
		 -30, -25, -20, -15, -15, -20, -25, -30 };

	constexpr const int* PST[5] = { PAWN_PST, KNIGHT_PST, BISHOP_PST, ROOK_PST, QUEEN_PST };

	//offset comes from boards being uneven in evaluation
	constexpr int PST_OFFSET[5] = { 24, -6, 9, 5, 0 };

	constexpr int PHASE_WEIGHT[6] = { 0, 1, 1, 2, 4, 0 };	//24 = all pieces on board = pure middlegame
	constexpr int BISHOP_PAIR_BONUS = 30;
}//!namespace

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
				score[color] += MATERIAL[type] + PST[type][sq] - PST_OFFSET[type];
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
