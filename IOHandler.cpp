#include "DataTypes.hpp"
#include "IOHandler.hpp"
#include "Bitboard.hpp"

#include <iostream>
#include <limits>
#include <string>

std::string buildStringFromSquare(Square sq) {
	if (sq == NO_SQUARE || sq < 0 || sq > 64) {
		return "invalid";
	}

	int rank = sq / 8;
	int file = sq % 8;

	std::string squareString;
	squareString += static_cast<char>('a' + file);
	squareString += static_cast<char>('1' + rank);

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

	std::cout << "\n\nIOHandler::attemptMove: Input tile coordinates from square (lowercase letters): \n";

	//fancy lambdas sponsored by ChatGPT
	source = buildSquareFromString([]() {
		std::string input;
		std::getline(std::cin, input);
		return input;
		}());

	/*	std::cout << "Iohandler::attemptMove: understood location " << static_cast<int>(source) << "\n";	*/
	//^ interestingly, this returns complete garbage for the contents of source. By some miracle I am not
	//able to break anything with this, though. So it may stay for now and we pray it stays as compliant as it is of now.
	//if you're still reading this in the final version, hopefully in an inactive preprocessor block, I am a happy man.

	//... and please put it in such a block if it isn't. I wrote this at 2am. My compiler is no use if it gets PTSD


	std::cout << "IOHandler::attemptMove: To square: \n";
	destination = buildSquareFromString([]() {
		std::string input;
		std::getline(std::cin, input);
		return input;
		}());

	std::cout << "Iohandler::attemptMove: understood location " << source << "\n";

	return static_cast<Move>(source | (destination << 6));
}