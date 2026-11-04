#include "DataTypes.hpp"
#include "IOHandler.hpp"
#include "Bitboard.hpp"

#include <iostream>
#include <limits>
#include <string>

std::string buildStringFromSquare(Square sq) {
	int rank = sq / 8;
	int file = sq % 8;

	std::string squareString;
	squareString += static_cast<char>('a' + file);
	squareString += std::to_string(rank + 1);

	return squareString;
}

Square buildSquareFromString(const std::string& squareString) {
	if (squareString.length() != 2) {
		return NO_SQUARE; 
	}
	char fileChar = squareString[0];
	char rankChar = squareString[1];
	if (fileChar < 'a' || fileChar > 'h' || rankChar < '1' || rankChar > '8') {
		return NO_SQUARE;
	}
	int file = fileChar - 'a';
	int rank = rankChar - '1';
	return static_cast<Square>(rank * 8 + file);
}

void IOHandler::printBitboard(Bitboard bb) {
	//print a 1 for occupied squares and a 0 for empty squares
	for (int rank = 7; rank >= 0; --rank) {
		for (int file = 0; file < 8; ++file) {
			Square sq = rank * 8 + file;
			if (bb & (1ULL << sq)) {
				std::cout << "1 ";
			} else {
				std::cout << "0 ";
			}
		}
		std::cout << std::endl;
	}
}

void IOHandler::printPosition(Position pos) {
	//populate castling rights to become readable
	std::string castling_rights;
	pos.castling_rights & 1 ? castling_rights += "WL, " : castling_rights += "-";
	pos.castling_rights & 2 ? castling_rights += "WS, " : castling_rights += "-";

	pos.castling_rights & 4 ? castling_rights += "BL, " : castling_rights += "-";
	pos.castling_rights & 8 ? castling_rights += "BS" : castling_rights += "-";

	std::cout << "Position:" << std::endl;
	printBitboard(pos.total_occupancy);
	std::cout << "Side to move: " << (pos.side_to_move == WHITE ? "White" : "Black") << std::endl;
	std::cout << "Castling rights: " << castling_rights << std::endl;
	std::cout << "En passant square: " << ((int)pos.ep_square == NO_SQUARE ? "None" : buildStringFromSquare(static_cast<Square>(pos.ep_square))) << std::endl;
	std::cout << "Reversible halfmove clock: " << pos.reversible_halfmove_clock << std::endl;
	std::cout << "Move number: " << pos.fullmove_counter << std::endl;
}

Move IOHandler::attemptMove() {
	Square source;
	Square destination;

	std::cout << "\n\nIOHandler::attemptMove: Input tile coordinates from square: \n";

	//fancy lambda sponsored by ChatGPT
	source = buildSquareFromString([]() {
		std::string input;
		std::getline(std::cin, input);
		return input;
		}());

	std::cout << "IOHandler::attemptMove: To square: \n";
	destination = buildSquareFromString([]() {
		std::string input;
		std::getline(std::cin, input);
		return input;
		}());
	

	Move move{};


	move = static_cast<Move>(source | (destination << 6));
	return move;
}