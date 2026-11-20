#ifndef INCLUDED__CHESS_ENGINE__ENGINE_TYPES_HPP
#define INCLUDED__CHESS_ENGINE__ENGINE_TYPES_HPP

#include <cstdint>
#include <string_view>

#include "DataTypes.hpp"


enum class PrincipleDifficulty { BEGINNER, INTERMEDIATE, ADVANCED, EXPERT, COUNT };
enum class GamePart { OPENING, MIDGAME, ATTACKING_MIDGAME, ENDGAME, COUNT };
enum class PrincipleFocus { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, SPACE, STRATEGY, COORDINATION, COUNT };


//Set of enum values stored as bits: bit i is set iff enum value i is contained
//The enums themselves stay plain indices, so COUNT keeps working for loops / arrays
//Usage: GamePart::OPENING | GamePart::MIDGAME, flags.has(GamePart::ENDGAME)
template <typename E>
struct EnumFlags {
	static_assert(static_cast<unsigned>(E::COUNT) <= 32, "EnumFlags: enum has too many values for 32 bits");

	std::uint32_t bits = 0;

	constexpr EnumFlags() = default;
	constexpr EnumFlags(E value) : bits(1u << static_cast<unsigned>(value)) {}	//implicit on purpose: single values are valid sets

	constexpr bool has(E value) const { return (bits & EnumFlags(value).bits) != 0; }
	constexpr bool empty() const { return bits == 0; }

	constexpr EnumFlags operator|(EnumFlags other) const {
		EnumFlags result;
		result.bits = bits | other.bits;
		return result;
	}
};

using GamePartFlags = EnumFlags<GamePart>;
using PrincipleFocusFlags = EnumFlags<PrincipleFocus>;

constexpr GamePartFlags operator|(GamePart a, GamePart b) { return GamePartFlags(a) | b; }
constexpr PrincipleFocusFlags operator|(PrincipleFocus a, PrincipleFocus b) { return PrincipleFocusFlags(a) | b; }


//This struct is more important than it looks!
//The order here determines the order in:
//		namespace
//		function declarations
//		final output vector!
enum class PrincipleId {
	//Development
	THIS_RETURNS_ZERO_POINT_FIVE,
	DEVELOP_KNIGHTS_BEFORE_BISHOPS,
	DONT_MOVE_SAME_PIECE_TWICE,
	NO_EARLY_QUEEN_MOVES,
	CASTLE_OFTEN_EARLY,
	CENTRALIZE_ROOKS,
	UNPIN_KNIGHTS_WITH_BISHOPS,
	FIANCHETTO_SQUARES,

	//Center & Space
	CONTEST_THE_CENTER,
	KICK_OUT_PIECES,
	TAKE_PAWN_SPACE,
	C_AND_D_PAWNS_TOGETHER,

	//Pawn Structure
	STRONG_PAWN_CHAINS,
	AVOID_UNINFLUENTIAL_DOUBLED_PAWNS,
	ALLOW_INFLUENTIAL_DOUBLED_PAWNS,
	PAWN_BREAKS,
	ATTACK_BACKWARDS_PAWNS,
	BLOCK_ISOLATED_PAWNS,
	KEEP_KING_FILES_CLOSED,
	CREATE_KING_LUFT,

	//Piece Activity
	OUTPOST_SQUARES,
	KNIGHT_REROUTES,
	BISHOP_QUEEN_BATTERIES,
	CREATE_PINS,
	ROOKS_ON_OPEN_FILES,
	ROOKS_ON_SEMI_OPEN_FILES,

	//Attack & Initiative
	CREATE_THREATS,
	TEMPO_WINNING_MOVES,
	RULE_OF_PLUS_TWO,
	KEEP_ATTACKERS,
	COORDINATE_ATTACKERS,
	LOCK_DOWN_CENTER,
	OPEN_UP_CENTER,
	ROOK_LIFTS,
	F_AND_H_PAWN_STORMS,

	//Endgame & Conversion
	TRADE_WHEN_UP_MATERIAL,
	ACTIVATE_KING_WITHOUT_QUEENS,
	KING_OFF_BACK_RANK,
	CREATE_PASSED_PAWNS,
	PUSH_PASSED_PAWNS,
	ROOKS_BEHIND_PASSED_PAWNS,
	BLOCKADE_SQUARES,
	BISHOP_BLOCKADE_SQUARES,
	PUSH_BAD_BISHOP_PAWNS,

	COUNT
};


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
	GamePartFlags gamePart;			//empty = unset
	PrincipleFocusFlags focus;		//empty = unset
	PrincipleEvalFunction calculateScore = nullptr;
};

#endif //!INCLUDED__CHESS_ENGINE__ENGINE_TYPES_HPP
