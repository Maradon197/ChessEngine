#ifndef INCLUDED__CHESS_ENGINE__PRINCIPLE_REGISTRY_HPP
#define INCLUDED__CHESS_ENGINE__PRINCIPLE_REGISTRY_HPP

#include <array>
#include <cstddef>

#include "DataTypes.hpp"
#include "EngineTypes.hpp"

#include "PrincipleFunctions.hpp"

std::array<float, size_t(PrincipleId::COUNT)> evaluateAll(const EvalContext& ctx);

inline constexpr std::array PRINCIPLES = {
	//Development
	PrincipleInfo{
		.id = PrincipleId::THIS_RETURNS_ZERO_POINT_FIVE,
		.title = "Test function: return 0.5f",
		.imagePath = "",
		.spotElo = 600,
		.description = "This is the description",
		.gamePart = GamePart::OPENING,
		.focus = PrincipleFocus::STRATEGY,
		.calculateScore = principles::thisReturnsZeroPointFive,
	},

	PrincipleInfo{
		.id = PrincipleId::DEVELOP_KNIGHTS_BEFORE_BISHOPS,
		.title = "Develop knights before bishops",
		.imagePath = "",
		.spotElo = 850,
		.description = "Bishops are more valuable down the line",
		.gamePart = GamePart::OPENING,
		.focus = PrincipleFocus::KNIGHT | PrincipleFocus::BISHOP | PrincipleFocus::COORDINATION,
		.calculateScore = principles::developKnightsBeforeBishops,
	},

	PrincipleInfo{
		.id = PrincipleId::DONT_MOVE_SAME_PIECE_TWICE,
		.title = "Don't move the same piece twice",
		.imagePath = "",
		.spotElo = 650,
		.description = "Developing other pieces has more value",
		.gamePart = GamePart::OPENING,
		.focus = PrincipleFocus::COORDINATION,
		.calculateScore = principles::dontMoveSamePieceTwice,
	},

	PrincipleInfo{
		.id = PrincipleId::NO_EARLY_QUEEN_MOVES,
		.title = "Don't bring the queen out too early",
		.imagePath = "",
		.spotElo = 625,
		.description = "Developing pieces gains space more sustainably",
		.gamePart = GamePart::OPENING,
		.focus = PrincipleFocus::QUEEN,
		.calculateScore = principles::noEarlyQueenMoves,
	},

	PrincipleInfo{
		.id = PrincipleId::CASTLE_OFTEN_EARLY,
		.title = "Castle often, castle early",
		.imagePath = "",
		.spotElo = 400,
		.description = "Castling gets your king to safety",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME,
		.focus = PrincipleFocus::KING,
		.calculateScore = principles::castleOftenEarly,
	},

	PrincipleInfo{
		.id = PrincipleId::CENTRALIZE_ROOKS,
		.title = "Centralize your rooks",
		.imagePath = "",
		.spotElo = 1100,
		.description = "Centralizing your rooks supports your position",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME,
		.focus = PrincipleFocus::ROOK | PrincipleFocus::COORDINATION,
		.calculateScore = principles::centralizeRooks,
	},

	PrincipleInfo{
		.id = PrincipleId::UNPIN_KNIGHTS_WITH_BISHOPS,
		.title = "Use bishops to unpin knights",
		.imagePath = "",
		.spotElo = 875,
		.description = "Remove pressure from your knights",
		.gamePart = GamePart::OPENING,
		.focus = PrincipleFocus::KNIGHT | PrincipleFocus::BISHOP | PrincipleFocus::STRATEGY,
		.calculateScore = principles::unpinKnightsWithBishops,
	},

	PrincipleInfo{
		.id = PrincipleId::FIANCHETTO_SQUARES,
		.title = "Fianchetto Bishops",
		.imagePath = "",
		.spotElo = 925,
		.description = "Creating fianchettos can be a good alternative for developing bishops to a strong diagonal",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::BISHOP,
		.calculateScore = principles::fianchettoSquares,
	},


	//Center & Space
	PrincipleInfo{
		.id = PrincipleId::CONTEST_THE_CENTER,
		.title = "Contest the Center",
		.imagePath = "",
		.spotElo = 300,
		.description = "Controlling central space eases development",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME,
		.focus = PrincipleFocus::SPACE,
		.calculateScore = principles::contestTheCenter,
	},

	PrincipleInfo{
		.id = PrincipleId::KICK_OUT_PIECES,
		.title = "Kick out enemy pieces",
		.imagePath = "",
		.spotElo = 575,
		.description = "Forcing back opponent's pieces gains space",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::SPACE,
		.calculateScore = principles::kickOutPieces,
	},

	PrincipleInfo{
		.id = PrincipleId::TAKE_PAWN_SPACE,
		.title = "Take space from the opponent's pieces",
		.imagePath = "",
		.spotElo = 1275,
		.description = "Restricting your opponent's movement gains you space",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::SPACE,
		.calculateScore = principles::takePawnSpace,
	},

	PrincipleInfo{
		.id = PrincipleId::C_AND_D_PAWNS_TOGETHER,
		.title = "C and D pawns go together",
		.imagePath = "",
		.spotElo = 1375,
		.description = "Pushing C and D pawns together allows for a strong center while your king stays safe",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::COORDINATION,
		.calculateScore = principles::cAndDPawnsTogether,
	},


	//Pawn Structure
	PrincipleInfo{
		.id = PrincipleId::STRONG_PAWN_CHAINS,
		.title = "Don't weaken pawn chains",
		.imagePath = "",
		.spotElo = 1075,
		.description = "Strong pawn chains restrict your opponent's possibilities",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::SPACE | PrincipleFocus::COORDINATION,
		.calculateScore = principles::strongPawnChains,
	},

	PrincipleInfo{
		.id = PrincipleId::AVOID_UNINFLUENTIAL_DOUBLED_PAWNS,
		.title = "Avoid doubled pawns",
		.imagePath = "",
		.spotElo = 675,
		.description = "Doubled pawns are easy targets since they defend themselves badly",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::COORDINATION,
		.calculateScore = principles::avoidUninfluentialDoubledPawns,
	},

	PrincipleInfo{
		.id = PrincipleId::ALLOW_INFLUENTIAL_DOUBLED_PAWNS,
		.title = "Allow influential doubled pawns",
		.imagePath = "",
		.spotElo = 1825,
		.description = "Sometimes, doubled pawns can create momentary positional strengths",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::COORDINATION,
		.calculateScore = principles::allowInfluentialDoubledPawns,
	},

	PrincipleInfo{
		.id = PrincipleId::PAWN_BREAKS,
		.title = "Create pawn breaks",
		.imagePath = "",
		.spotElo = 1575,
		.description = "Use pawn breaks to strategically weaken your opponent's footing",
		.gamePart = GamePart::MIDGAME,
		.focus = PrincipleFocus::PAWN,
		.calculateScore = principles::pawnBreaks,
	},

	PrincipleInfo{
		.id = PrincipleId::ATTACK_BACKWARDS_PAWNS,
		.title = "Attack backwards pawns",
		.imagePath = "",
		.spotElo = 1675,
		.description = "Weak backwards pawns can become targets for your attack",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::STRATEGY,
		.calculateScore = principles::attackBackwardsPawns,
	},

	PrincipleInfo{
		.id = PrincipleId::BLOCK_ISOLATED_PAWNS,
		.title = "Block isolated pawns",
		.imagePath = "",
		.spotElo = 1750,
		.description = "Disallow counterplay around the positional weakness",
		.gamePart = GamePart::MIDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::STRATEGY,
		.calculateScore = principles::blockIsolatedPawns,
	},

	PrincipleInfo{
		.id = PrincipleId::KEEP_KING_FILES_CLOSED,
		.title = "Keep files to your king closed",
		.imagePath = "",
		.spotElo = 900,
		.description = "The king is usually safer with these pawns",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::SPACE,
		.calculateScore = principles::keepKingFilesClosed,
	},

	PrincipleInfo{
		.id = PrincipleId::CREATE_KING_LUFT,
		.title = "Create space for your king",
		.imagePath = "",
		.spotElo = 725,
		.description = "Making an escape square stops back rank attacks",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::SPACE,
		.calculateScore = principles::createKingLuft,
	},


	//Piece Activity
	PrincipleInfo{
		.id = PrincipleId::OUTPOST_SQUARES,
		.title = "Create outposts",
		.imagePath = "",
		.spotElo = 1350,
		.description = "Outposts are dominant positions for your knights",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::KNIGHT | PrincipleFocus::COORDINATION,
		.calculateScore = principles::outpostSquares,
	},

	PrincipleInfo{
		.id = PrincipleId::KNIGHT_REROUTES,
		.title = "Reroute knights that are stuck",
		.imagePath = "",
		.spotElo = 1525,
		.description = "Counterintuitive backwards moves may allow you to place knights on more active squares",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::KNIGHT | PrincipleFocus::COORDINATION,
		.calculateScore = principles::knightReroutes,
	},

	PrincipleInfo{
		.id = PrincipleId::BISHOP_QUEEN_BATTERIES,
		.title = "Create Batteries with Queen and Bishop",
		.imagePath = "",
		.spotElo = 500,
		.description = "Having a queen and bishop on the same diagonal can apply lots of pressure",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::BISHOP | PrincipleFocus::QUEEN | PrincipleFocus::COORDINATION,
		.calculateScore = principles::bishopQueenBatteries,
	},

	PrincipleInfo{
		.id = PrincipleId::CREATE_PINS,
		.title = "Create pins",
		.imagePath = "",
		.spotElo = 725,
		.description = "Pinned pieces can't move freely and become easy targets",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::BISHOP | PrincipleFocus::STRATEGY,
		.calculateScore = principles::createPins,
	},

	PrincipleInfo{
		.id = PrincipleId::ROOKS_ON_OPEN_FILES,
		.title = "Place rooks on open files",
		.imagePath = "",
		.spotElo = 950,
		.description = "Open files give your rooks full reach into the enemy position",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::ROOK | PrincipleFocus::SPACE,
		.calculateScore = principles::rooksOnOpenFiles,
	},

	PrincipleInfo{
		.id = PrincipleId::ROOKS_ON_SEMI_OPEN_FILES,
		.title = "Place rooks on semi-open files",
		.imagePath = "",
		.spotElo = 1325,
		.description = "A single pawn in the way still lets your rook pressure valuable targets behind it",
		.gamePart = GamePart::MIDGAME | GamePart::ATTACKING_MIDGAME,
		.focus = PrincipleFocus::ROOK | PrincipleFocus::STRATEGY,
		.calculateScore = principles::rooksOnSemiOpenFiles,
	},


	//Attack & Initiative
	PrincipleInfo{
		.id = PrincipleId::CREATE_THREATS,
		.title = "Create threats to the enemy",
		.imagePath = "",
		.spotElo = 200,
		.description = "Creating threats to the enemy king, pieces or position is the only way to win",
		.gamePart = GamePart::OPENING | GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::STRATEGY,
		.calculateScore = principles::createThreats,
	},

	PrincipleInfo{
		.id = PrincipleId::TEMPO_WINNING_MOVES,
		.title = "Play tempo-winning moves",
		.imagePath = "",
		.spotElo = 1175,
		.description = "Momentum allows you to take control over the game",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::STRATEGY,
		.calculateScore = principles::tempoWinningMoves,
	},

	PrincipleInfo{
		.id = PrincipleId::RULE_OF_PLUS_TWO,
		.title = "Know when to attack (Rule of +2)",
		.imagePath = "",
		.spotElo = 1650,
		.description = "A successful attack usually requires at least 2 more attackers than defenders",
		.gamePart = GamePart::MIDGAME,
		.focus = PrincipleFocus::COORDINATION,
		.calculateScore = principles::ruleOfPlusTwo,
	},

	PrincipleInfo{
		.id = PrincipleId::KEEP_ATTACKERS,
		.title = "Keep vital attackers on the board",
		.imagePath = "",
		.spotElo = 1525,
		.description = "Without sufficient material presence, your attack can be stopped easily",
		.gamePart = GamePart::ATTACKING_MIDGAME,
		.focus = PrincipleFocus::STRATEGY,
		.calculateScore = principles::keepAttackers,
	},

	PrincipleInfo{
		.id = PrincipleId::COORDINATE_ATTACKERS,
		.title = "Coordinate your attack",
		.imagePath = "",
		.spotElo = 1400,
		.description = "Bring and coordinate your attackers to overwhelm the enemy position",
		.gamePart = GamePart::ATTACKING_MIDGAME,
		.focus = PrincipleFocus::COORDINATION,
		.calculateScore = principles::coordinateAttackers,
	},

	PrincipleInfo{
		.id = PrincipleId::LOCK_DOWN_CENTER,
		.title = "Attack the same-side castled king",
		.imagePath = "",
		.spotElo = 1725,
		.description = "Lock down the center to avoid enemy counterplay",
		.gamePart = GamePart::ATTACKING_MIDGAME,
		.focus = PrincipleFocus::COORDINATION,
		.calculateScore = principles::lockDownCenter,
	},

	PrincipleInfo{
		.id = PrincipleId::OPEN_UP_CENTER,
		.title = "Attack the opposite-side castled king",
		.imagePath = "",
		.spotElo = 1475,
		.description = "Open up the center to create a big attack",
		.gamePart = GamePart::ATTACKING_MIDGAME,
		.focus = PrincipleFocus::COORDINATION,
		.calculateScore = principles::openUpCenter,
	},

	PrincipleInfo{
		.id = PrincipleId::ROOK_LIFTS,
		.title = "Create rook lifts",
		.imagePath = "",
		.spotElo = 1625,
		.description = "Bring rooks to the attack using C- and F- file",
		.gamePart = GamePart::ATTACKING_MIDGAME,
		.focus = PrincipleFocus::ROOK | PrincipleFocus::COORDINATION,
		.calculateScore = principles::rookLifts,
	},

	PrincipleInfo{
		.id = PrincipleId::F_AND_H_PAWN_STORMS,
		.title = "Create Pawn storms with F4 & H4, H5",
		.imagePath = "",
		.spotElo = 1500,
		.description = "F and H pawn pushes apply pressure and take space from the opponent's kingside",
		.gamePart = GamePart::ATTACKING_MIDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::SPACE | PrincipleFocus::STRATEGY,
		.calculateScore = principles::fAndHPawnStorms,
	},


	//Endgame & Conversion
	PrincipleInfo{
		.id = PrincipleId::TRADE_WHEN_UP_MATERIAL,
		.title = "Trade if you're up material",
		.imagePath = "",
		.spotElo = 825,
		.description = "It's the simplest way to convert a winning position",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::STRATEGY,
		.calculateScore = principles::tradeWhenUpMaterial,
	},

	PrincipleInfo{
		.id = PrincipleId::ACTIVATE_KING_WITHOUT_QUEENS,
		.title = "Bring your king in once the queens are off",
		.imagePath = "",
		.spotElo = 1025,
		.description = "Without queens, the king is safe enough to become a strong attacking piece",
		.gamePart = GamePart::ENDGAME,
		.focus = PrincipleFocus::KING | PrincipleFocus::STRATEGY,
		.calculateScore = principles::activateKingWithoutQueens,
	},

	PrincipleInfo{
		.id = PrincipleId::KING_OFF_BACK_RANK,
		.title = "Get your king off the back rank",
		.imagePath = "",
		.spotElo = 825,
		.description = "A king on the back rank is passive and vulnerable to back rank mates",
		.gamePart = GamePart::ENDGAME,
		.focus = PrincipleFocus::KING | PrincipleFocus::SPACE,
		.calculateScore = principles::kingOffBackRank,
	},

	PrincipleInfo{
		.id = PrincipleId::CREATE_PASSED_PAWNS,
		.title = "Create passed pawns",
		.imagePath = "",
		.spotElo = 1050,
		.description = "Passed pawns tie down enemy pieces and threaten to promote",
		.gamePart = GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::STRATEGY,
		.calculateScore = principles::createPassedPawns,
	},

	PrincipleInfo{
		.id = PrincipleId::PUSH_PASSED_PAWNS,
		.title = "Push passed pawns",
		.imagePath = "",
		.spotElo = 650,
		.description = "Every step closer to promotion forces your opponent to react",
		.gamePart = GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN,
		.calculateScore = principles::pushPassedPawns,
	},

	PrincipleInfo{
		.id = PrincipleId::ROOKS_BEHIND_PASSED_PAWNS,
		.title = "Put rooks behind passed pawns",
		.imagePath = "",
		.spotElo = 1450,
		.description = "A rook behind a passed pawn gains activity with every push",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::ROOK | PrincipleFocus::COORDINATION,
		.calculateScore = principles::rooksBehindPassedPawns,
	},

	PrincipleInfo{
		.id = PrincipleId::BLOCKADE_SQUARES,
		.title = "Blockade passed pawns",
		.imagePath = "",
		.spotElo = 1475,
		.description = "Placing a piece in front of a passed pawn restricts your opponent's play",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::KNIGHT | PrincipleFocus::BISHOP | PrincipleFocus::SPACE,
		.calculateScore = principles::blockadeSquares,
	},

	PrincipleInfo{
		.id = PrincipleId::BISHOP_BLOCKADE_SQUARES,
		.title = "Blockade with bishops out of knight reach",
		.imagePath = "",
		.spotElo = 1775,
		.description = "A blockading bishop that no knight attacks is hard to remove",
		.gamePart = GamePart::MIDGAME | GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::BISHOP | PrincipleFocus::SPACE,
		.calculateScore = principles::bishopBlockadeSquares,
	},

	PrincipleInfo{
		.id = PrincipleId::PUSH_BAD_BISHOP_PAWNS,
		.title = "Push pawns on the bad bishop's color",
		.imagePath = "",
		.spotElo = 1800,
		.description = "Push to promotion squares unavailable to that bishop",
		.gamePart = GamePart::ENDGAME,
		.focus = PrincipleFocus::PAWN | PrincipleFocus::BISHOP | PrincipleFocus::STRATEGY,
		.calculateScore = principles::pushBadBishopPawns,
	},
};

//make wrong entries a compile error
namespace principle_registry_checks {
	consteval bool idsMatchIndices() {
		for (std::size_t i = 0; i < PRINCIPLES.size(); ++i) {
			if (static_cast<std::size_t>(PRINCIPLES[i].id) != i) return false;
		}
		return true;
	}

	consteval bool allHaveGamePartAndFocus() {
		for (const PrincipleInfo& principle : PRINCIPLES) {
			if (principle.gamePart.empty() || principle.focus.empty()) return false;
		}
		return true;
	}

	consteval bool allHaveScoreFunction() {
		for (const PrincipleInfo& principle : PRINCIPLES) {
			if (principle.calculateScore == nullptr) return false;
		}
		return true;
	}
}

static_assert(PRINCIPLES.size() == static_cast<std::size_t>(PrincipleId::COUNT), 
	"engine/eval/principles/PrincipleRegistry.hpp:every PrincipleId needs exactly one entry in PRINCIPLES");
static_assert(principle_registry_checks::idsMatchIndices(), 
	"engine/eval/principles/PrincipleRegistry.hpp: PRINCIPLES must be in the same order as PrincipleId");
static_assert(principle_registry_checks::allHaveScoreFunction(), 
	"engine/eval/principles/PrincipleRegistry.hpp: every principle needs a calculateScore function");
static_assert(principle_registry_checks::allHaveGamePartAndFocus(), 
	"engine/eval/principles/PrincipleRegistry.hpp: every principle needs at least one gamePart and focus");


#endif //!INCLUDED__CHESS_ENGINE__PRINCIPLE_REGISTRY_HPP
