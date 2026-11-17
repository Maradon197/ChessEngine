#ifndef PRINCIPLE_HPP
#define PRINCIPLE_HPP

#include <string>

//rough chess.com rapid rating bands, derived from Principle::spotElo
enum class PrincipleDifficulty {BEGINNER, INTERMEDIATE, ADVANCED, EXPERT, COUNT};
enum class GamePart {OPENING, MIDGAME, ATTACKING_MIDGAME, DEFENDING_MIDGAME, ENDGAME, COUNT};
enum class PrincipleFocus {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, SPACE, STRATEGY, COORDINATION, COUNT};

constexpr PrincipleDifficulty getPrincipleDifficulty(int spotElo) {
	return spotElo <= 600 ? PrincipleDifficulty::BEGINNER
		: spotElo <= 1200 ? PrincipleDifficulty::INTERMEDIATE
		: spotElo <= 1800 ? PrincipleDifficulty::ADVANCED
		: PrincipleDifficulty::EXPERT;
}

struct Principle {
	std::string title;	//unique, identifies the principle
	std::string imagePath;	//UTF-8, empty: a placeholder is drawn
	int spotElo = 0;	//rating from which players usually spot it, also decides the difficulty tag
	std::string description;	//one or two sentences
	std::string explanation;	//optional, Markdown
	GamePart gamePart = GamePart::MIDGAME;
	PrincipleFocus focus = PrincipleFocus::STRATEGY;
	bool enabled = true;	//chosen by the user: the engine should consider it
	bool useful = false;	//decided by the engine: it can actually do something with it
};

#endif
