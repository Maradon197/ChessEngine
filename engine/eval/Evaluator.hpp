#ifndef INCLUDED__CHESS_ENGINE__EVALUATOR_HPP
#define INCLUDED__CHESS_ENGINE__EVALUATOR_HPP

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

class Evaluator {
private:
	//zobrist hashed position values?
public:
	EvalValue overallScore(const Position& pos);
	EvalValue weightedMaterial(const Position& pos);
	EvalValue kingSafety(const Position& pos);
	EvalValue space(const Position& pos);
};

#endif //!INCLUDED__CHESS_ENGINE__EVALUATOR_HPP
