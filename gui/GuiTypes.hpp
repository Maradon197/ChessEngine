#ifndef GUITYPES_HPP
#define GUITYPES_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "DataTypes.hpp"
#include "MoveBits.hpp"
#include "Principle.hpp"

using NodeId = std::uint32_t;
constexpr NodeId ROOT_NODE = 0;	//starting position, always exists

enum class EvalCriterion {WEIGHTED_MATERIAL, KING_SAFETY, PIECE_ACTIVITY, SPACE, COUNT};

constexpr int CRITERION_MIN = -3;
constexpr int CRITERION_MAX = 3;

//one value per square, indexed by Square (a1 = 0); positive is good for white, negative for black
using SquareValues = std::array<int, 64>;

enum class ThemeMode {SYSTEM, LIGHT, DARK};

enum class ColorRole {
	LIGHT_SQUARE, DARK_SQUARE, LAST_MOVE, SELECTION, LEGAL_MOVE,
	USER_HIGHLIGHT, USER_ARROW, ENGINE_HIGHLIGHT, ENGINE_ARROW,
	EVAL_PANEL, MOVES_PANEL, PRINCIPLES_PANEL,
	COUNT
};
constexpr std::size_t COLOR_ROLE_COUNT = static_cast<std::size_t>(ColorRole::COUNT);

using ColorValue = std::uint32_t;

inline constexpr const char* SETTINGS_ORGANIZATION = "ChessEngineGUI";
inline constexpr const char* SETTINGS_APPLICATION = "ChessEngineGUI";

enum class KeyAction {
	TOGGLE_SIDEBAR, MOVE_TREE, PRINCIPLE_DRAWER, CLOSE_DRAWER, TREE_BACK, TREE_FORWARD,
	COUNT
};
constexpr std::size_t KEY_ACTION_COUNT = static_cast<std::size_t>(KeyAction::COUNT);

enum class KeyMode {TOGGLE, HOLD};

//own types so they can't be mixed up with Square and Move
class Highlight {
public:
	constexpr Highlight() = default;
	constexpr explicit Highlight(Square square) : square(square) {}

	constexpr Square toSquare() const { return square; }

	constexpr bool operator==(const Highlight& other) const = default;

private:
	Square square = NO_SQUARE;
};

//keeps only source and destination
class Arrow {
public:
	constexpr Arrow() = default;
	constexpr explicit Arrow(Move move) : move(moveBits::getFromTo(move)) {}
	constexpr Arrow(Square source, Square destination) : move(moveBits::makeMove(source, destination)) {}

	constexpr Move toMove() const { return move; }
	constexpr Square getSource() const { return moveBits::getSource(move); }
	constexpr Square getDestination() const { return moveBits::getDestination(move); }

	constexpr bool operator==(const Arrow& other) const = default;

private:
	Move move = 0;
};

#endif
