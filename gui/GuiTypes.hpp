#ifndef GUITYPES_HPP
#define GUITYPES_HPP

#include <cstddef>
#include <cstdint>

#include "DataTypes.hpp"
#include "MoveBits.hpp"

//Types the GUI adds on top of DataTypes.hpp. Everything that crosses the Doorman
//is either a core type (Position, Move, MoveList, Square, ...) or one of these.

//Id of a node in the move tree. Assigned by the Doorman.
//ROOT_NODE is the starting position and always exists.
using NodeId = std::uint32_t;
constexpr NodeId ROOT_NODE = 0;

//Id of an entry in the review panel. Assigned by the chess core.
using ReviewEntryId = std::uint32_t;

enum class EvalCriterion {WEIGHTED_MATERIAL, KING_SAFETY, PIECE_ACTIVITY, SPACE, COUNT};

//criteria range from "3 minus" to "3 plus", positive = good for White
constexpr int CRITERION_MIN = -3;
constexpr int CRITERION_MAX = 3;

enum class ThemeMode {SYSTEM, LIGHT, DARK};

//Interface elements whose color the user (or the core) can change.
enum class ColorRole {
	LIGHT_SQUARE, DARK_SQUARE, LAST_MOVE, SELECTION, LEGAL_MOVE,
	USER_HIGHLIGHT, USER_ARROW, ENGINE_HIGHLIGHT, ENGINE_ARROW,
	EVAL_PANEL, REVIEW_PANEL, MOVES_PANEL,
	COUNT
};
constexpr std::size_t COLOR_ROLE_COUNT = static_cast<std::size_t>(ColorRole::COUNT);

//0xAARRGGBB, identical to Qt's QRgb
using ColorValue = std::uint32_t;

//A highlighted square. Its own type so it can never be mistaken for a selection (Square).
class Highlight {
public:
	constexpr Highlight() = default;
	constexpr explicit Highlight(Square square) : square(square) {}

	constexpr Square toSquare() const { return square; }

	constexpr bool operator==(const Highlight& other) const = default;

private:
	Square square = NO_SQUARE;
};

//An arrow between two squares. Its own type so it can never be mistaken for a Move.
//Only the source (bits 0-5) and destination (bits 6-11) of the wrapped move are kept.
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
