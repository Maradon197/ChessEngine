#ifndef PGNWRITER
#define PGNWRITER

#include <string>
#include <vector>

#include "DataTypes.hpp"

//builds a PGN text (headers + move text) of a game played from the starting position
//the moves are replayed on a fresh board, each one has to be legal in the position it is played in
namespace PgnWriter {
	struct Headers {
		std::string event = "Casual game";
		std::string site = "ChessEngineGUI";
		std::string date = "????.??.??";	//PGN format: YYYY.MM.DD
		std::string white = "?";
		std::string black = "?";
	};

	std::string buildPgn(const std::vector<Move>& moves, GameStatus status, const Headers& headers);
}

#endif
