#ifndef INCLUDED__CHESS_ENGINE__ENGINE_TYPES_HPP
#define INCLUDED__CHESS_ENGINE__ENGINE_TYPES_HPP

#include <string_view>

#include "DataTypes.hpp"


enum class PrincipleDifficulty { BEGINNER, INTERMEDIATE, ADVANCED, EXPERT, COUNT };
enum class GamePart { OPENING, MIDGAME, ATTACKING_MIDGAME, DEFENDING_MIDGAME, ENDGAME, COUNT };
enum class PrincipleFocus { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, SPACE, STRATEGY, COORDINATION, COUNT };


//This struct is more important than it looks!
//The order here determines the order in:
//		namespace
//		function declarations
//		final output vector!
enum class PrincipleId { THIS_RETURNS_ZERO_POINT_FIVE, COUNT };


constexpr PrincipleDifficulty getPrincipleDifficulty(int spotElo) {
	return spotElo <= 600 ? PrincipleDifficulty::BEGINNER
		: spotElo <= 1200 ? PrincipleDifficulty::INTERMEDIATE
		: spotElo <= 1800 ? PrincipleDifficulty::ADVANCED
		: PrincipleDifficulty::EXPERT;
}


struct EvalContext {
	//TODO: hashed stuff
	const Position& pos;
};


//@return: float[-1,+1] in the perspective of the side to move
using PrincipleEvalFunction = float (*)(const EvalContext&);


struct PrincipleInfo {
	PrincipleId id = PrincipleId::COUNT;	//primary key
	std::string_view title;
	std::string_view imagePath;
	int spotElo = 0;
	std::string_view description;
	std::string_view explanation;	//optional
	GamePart gamePart = GamePart::COUNT;
	PrincipleFocus focus = PrincipleFocus::COUNT;
	PrincipleEvalFunction calculateScore = nullptr;
};

#endif //!INCLUDED__CHESS_ENGINE__ENGINE_TYPES_HPP
