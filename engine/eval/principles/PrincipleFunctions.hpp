#ifndef INCLUDED__CHESS_ENGINE__PRINCIPLE_FUNCTINOS_HPP
#define INCLUDED__CHESS_ENGINE__PRINCIPLE_FUNCTINOS_HPP

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

namespace principles {
	//OpeningPrinciples.cpp
	float thisReturnsZeroPointFive(const EvalContext& ctx);
	float contestTheCenter(const EvalContext& ctx);
	float developKnightsBeforeBishops(const EvalContext& ctx);
	float dontMoveSamePieceTwice(const EvalContext& ctx);
	float kickOutPieces(const EvalContext& ctx);
	float noEarlyQueenMoves(const EvalContext& ctx);
	float castleOftenEarly(const EvalContext& ctx);
	float centralizeRooks(const EvalContext& ctx);
	float unpinKnightsWithBishops(const EvalContext& ctx);

	//MidgamePrinciples.cpp
	float tradeWhenUpMaterial(const EvalContext& ctx);
	float ruleOfPlusTwo(const EvalContext& ctx);
	float createThreats(const EvalContext& ctx);
	float keepAttackers(const EvalContext& ctx);
	float coordinateAttackers(const EvalContext& ctx);
	float tempoWinningMoves(const EvalContext& ctx);
	float lockDownCenter(const EvalContext& ctx);
	float openUpCenter(const EvalContext& ctx);
	float rookLifts(const EvalContext& ctx);
	float takePawnSpace(const EvalContext& ctx);
	float strongPawnChains(const EvalContext& ctx);
	float avoidUninfluentialDoubledPawns(const EvalContext& ctx);
	float allowInfluentialDoubledPawns(const EvalContext& ctx);
	float pawnBreaks(const EvalContext& ctx);
	float attackBackwardsPawns(const EvalContext& ctx);
	float blockIsolatedPawns(const EvalContext& ctx);
	float createKingLuft(const EvalContext& ctx);
	float cAndDPawnsTogether(const EvalContext& ctx);
	float keepKingFilesClosed(const EvalContext& ctx);
	float fAndHPawnStorms(const EvalContext& ctx);
	float outpostSquares(const EvalContext& ctx);
	float knightReroutes(const EvalContext& ctx);
	float blockadeSquares(const EvalContext& ctx);
	float fianchettoSquares(const EvalContext& ctx);
	float bishopQueenBatteries(const EvalContext& ctx);
	float createPins(const EvalContext& ctx);
	float bishopBlockadeSquares(const EvalContext& ctx);
	float rooksOnOpenFiles(const EvalContext& ctx);
	float rooksOnSemiOpenFiles(const EvalContext& ctx);

	//EndgamePrinciples.cpp
	float activateKingWithoutQueens(const EvalContext& ctx);
	float createPassedPawns(const EvalContext& ctx);
	float pushBadBishopPawns(const EvalContext& ctx);
	float pushPassedPawns(const EvalContext& ctx);
	float rooksBehindPassedPawns(const EvalContext& ctx);
	float kingOffBackRank(const EvalContext& ctx);
}

#endif //!INCLUDED__CHESS_ENGINE__PRINCIPLE_FUNCTINOS_HPP
