#include "PgnWriter.hpp"

#include <cstddef>

#include "BoardState.hpp"
#include "MoveNotation.hpp"

namespace {

constexpr std::size_t MAX_LINE_LENGTH = 79;	//PGN export format: lines below 80 characters

const char* getResultString(GameResult result) {
	switch (result) {
	case GameResult::WHITE_WIN: return "1-0";
	case GameResult::BLACK_WIN: return "0-1";
	case GameResult::DRAW: return "1/2-1/2";
	default: return "*";
	}
}

const char* getReasonString(GameEndingReason reason) {
	switch (reason) {
	case GameEndingReason::CHECKMATE: return "Checkmate";
	case GameEndingReason::STALEMATE: return "Stalemate";
	case GameEndingReason::REPETITION: return "Threefold repetition";
	case GameEndingReason::FIFTY_MOVE_RULE: return "Fifty move rule";
	case GameEndingReason::INSUFFICIENT_MATERIAL: return "Insufficient material";
	case GameEndingReason::RESIGNATION: return "Resignation";
	case GameEndingReason::TIME: return "Time forfeit";
	case GameEndingReason::AGREEANCE: return "Draw by agreement";
	default: return nullptr;
	}
}

//MoveNotation writes for humans, PGN wants castling with the letter O and no "e.p." suffix
std::string toPgnSan(std::string san) {
	if (san.rfind("0-0", 0) == 0) {
		for (char& character : san) {
			if (character == '0') character = 'O';
		}
	}
	const std::size_t ep = san.find(" e.p.");
	if (ep != std::string::npos) {
		san.erase(ep, 5);
	}
	return san;
}

void addHeader(std::string& pgn, const char* name, const std::string& value) {
	pgn += '[';
	pgn += name;
	pgn += " \"";
	for (char character : value) {	//quotes and backslashes are escaped inside tag values
		if (character == '"' || character == '\\') pgn += '\\';
		pgn += character;
	}
	pgn += "\"]\n";
}

} // !namespace

std::string PgnWriter::buildPgn(const std::vector<Move>& moves, GameStatus status, const Headers& headers) {
	const char* result = getResultString(status.result);

	std::string pgn;
	addHeader(pgn, "Event", headers.event);
	addHeader(pgn, "Site", headers.site);
	addHeader(pgn, "Date", headers.date);
	addHeader(pgn, "Round", "-");
	addHeader(pgn, "White", headers.white);
	addHeader(pgn, "Black", headers.black);
	addHeader(pgn, "Result", result);
	pgn += '\n';

	//move text as tokens, wrapped into lines afterwards
	std::vector<std::string> tokens;
	BoardState board;
	board.setStartingPosition();
	for (std::size_t index = 0; index < moves.size(); ++index) {
		const Position& position = board.getPosition();
		if (position.side_to_move == WHITE) {
			tokens.push_back(std::to_string(position.fullmove_counter) + ".");
		}
		else if (index == 0) {
			tokens.push_back(std::to_string(position.fullmove_counter) + "...");
		}
		tokens.push_back(toPgnSan(MoveNotation::buildMoveString(moves[index], position)));
		board.makeMove(moves[index]);
	}
	//the core doesn't mark mate in the notation, the game result knows it
	if (status.reason == GameEndingReason::CHECKMATE && !tokens.empty() && tokens.back().back() == '+') {
		tokens.back().back() = '#';
	}
	if (const char* reason = getReasonString(status.reason)) {
		tokens.push_back(std::string("{") + reason + "}");
	}
	tokens.push_back(result);

	std::size_t lineLength = 0;
	for (const std::string& token : tokens) {
		if (lineLength > 0 && lineLength + 1 + token.size() > MAX_LINE_LENGTH) {
			pgn += '\n';
			lineLength = 0;
		}
		if (lineLength > 0) {
			pgn += ' ';
			++lineLength;
		}
		pgn += token;
		lineLength += token.size();
	}
	pgn += "\n\n";
	return pgn;
}
