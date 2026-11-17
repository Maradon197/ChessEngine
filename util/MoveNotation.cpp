#include "MoveNotation.hpp"

#include <string>

#include "DataTypes.hpp"
#include "Bitboard.hpp"

#include "AttackDetector.hpp"
#include "IOHandler.hpp"
#include "PseudoLegalMoveGenerator.hpp"

namespace {

	constexpr char PIECE_LETTERS[] = { '\0', 'N', 'B', 'R', 'Q', 'K' };

	PieceType getPieceTypeAt(const Position& pos, Color color, Square sq) {
		for (int type = PAWN; type <= KING; ++type)
			if (pos.pieces[color][type] & BB::oneSquareBB(sq))
				return static_cast<PieceType>(type);
		return NONE;
	}

	//same promotion fallback as BoardState::makeMove
	PieceType getPromotionType(Move move) {
		const PieceType type = static_cast<PieceType>((move >> 15) & 0x7);
		return (type < KNIGHT || type > QUEEN) ? QUEEN : type;
	}

	//rough copy of BoardState::makeMove, only the piece bitboards and occupancies are kept up to date
	//(enough for check detection, clocks / castling rights / ep square stay untouched)
	Position applyMoveToCopy(const Position& pos, Move move) {
		Position after{ pos };

		const Color us = pos.side_to_move;
		const Color them = !us;
		const Square source = move & 0x3f;
		const Square destination = (move >> 6) & 0x3f;
		const PieceType moving_type = getPieceTypeAt(pos, us, source);

		BB::remSquare(after.pieces[us][moving_type], source);

		const PieceType captured_type = getPieceTypeAt(pos, them, destination);
		if (captured_type != NONE)
			BB::remSquare(after.pieces[them][captured_type], destination);

		if (moving_type == PAWN && destination == pos.ep_square) {
			const Square captured_square = us == WHITE ? destination - 8 : destination + 8;
			BB::remSquare(after.pieces[them][PAWN], captured_square);
		}

		if (moving_type == KING && (destination == source + 2 || source == destination + 2)) {
			const bool is_castling_king_side = destination > source;
			const Square home_rank = us == WHITE ? 0 : 56;
			BB::remSquare(after.pieces[us][ROOK], home_rank + (is_castling_king_side ? 7 : 0));
			BB::addSquare(after.pieces[us][ROOK], home_rank + (is_castling_king_side ? 5 : 3));
		}

		const bool is_promo = moving_type == PAWN && (destination >= 56 || destination < 8);
		BB::addSquare(after.pieces[us][is_promo ? getPromotionType(move) : moving_type], destination);

		after.color_occupancy[WHITE] = after.color_occupancy[BLACK] = 0;
		for (int type = PAWN; type <= KING; ++type) {
			after.color_occupancy[WHITE] |= after.pieces[WHITE][type];
			after.color_occupancy[BLACK] |= after.pieces[BLACK][type];
		}
		after.total_occupancy = after.color_occupancy[WHITE] | after.color_occupancy[BLACK];
		after.side_to_move = them;

		return after;
	}

	bool isLegal(const Position& pos, Move move) {
		return !isInCheck(pos.side_to_move, applyMoveToCopy(pos, move));
	}

	//file, rank or full square of the source, whatever is needed to tell it apart
	//from other pieces of the same type that can legally reach the same destination
	std::string buildDisambiguation(const Position& pos, Square source, Square destination, PieceType moving_type) {
		bool is_ambiguous = false;
		bool shares_file = false;
		bool shares_rank = false;

		const MoveList candidates = getPseudoLegalMovesInPosition(pos);
		for (std::size_t i = 0; i < candidates.count; ++i) {
			const Move candidate = candidates.moves[i];
			const Square other_source = candidate & 0x3f;
			const Square other_destination = (candidate >> 6) & 0x3f;

			if (other_source == source || other_destination != destination)
				continue;
			if (getPieceTypeAt(pos, pos.side_to_move, other_source) != moving_type)
				continue;
			if (!isLegal(pos, candidate))
				continue;

			is_ambiguous = true;
			shares_file |= BB::getFile(other_source) == BB::getFile(source);
			shares_rank |= BB::getRank(other_source) == BB::getRank(source);
		}

		if (!is_ambiguous)
			return "";

		const std::string source_string = IOHandler::buildStringFromSquare(source);
		if (!shares_file)
			return source_string.substr(0, 1);
		if (!shares_rank)
			return source_string.substr(1, 1);
		return source_string;
	}

}

std::string MoveNotation::buildMoveString(Move move, const Position& pos) {
	const Color us = pos.side_to_move;
	const Square source = move & 0x3f;
	const Square destination = (move >> 6) & 0x3f;

	const PieceType moving_type = getPieceTypeAt(pos, us, source);
	if (moving_type == NONE)
		return IOHandler::buildStringFromSquare(source) + IOHandler::buildStringFromSquare(destination);

	//derived from the position like in BoardState::makeMove, the move flags are not always set
	const bool is_castle = moving_type == KING && (destination == source + 2 || source == destination + 2);
	const bool is_ep = moving_type == PAWN && destination == pos.ep_square;
	const bool is_capture = is_ep || getPieceTypeAt(pos, !us, destination) != NONE;
	const bool is_promo = moving_type == PAWN && (destination >= 56 || destination < 8);

	std::string result;

	if (is_castle) {
		result = destination > source ? "0-0" : "0-0-0";
	}
	else if (moving_type == PAWN) {
		if (is_capture) {
			result += IOHandler::buildStringFromSquare(source)[0];
			result += 'x';
		}
		result += IOHandler::buildStringFromSquare(destination);
		if (is_promo) {
			result += '=';
			result += PIECE_LETTERS[getPromotionType(move)];
		}
	}
	else {
		result += PIECE_LETTERS[moving_type];
		if (moving_type != KING)
			result += buildDisambiguation(pos, source, destination, moving_type);
		if (is_capture)
			result += 'x';
		result += IOHandler::buildStringFromSquare(destination);
	}

	//no '#' yet, mate is not detected by the core
	if (isInCheck(!us, applyMoveToCopy(pos, move)))
		result += '+';

	if (is_ep)
		result += " e.p.";

	return result;
}
