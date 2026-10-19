#include "DataTypes.h"
#include "IOHandler.h"
#include "Bitboard.h"
#include <iostream>
#include "string"

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
	std::cout << "En passant square: " << ((int)pos.ep_square == NO_SQUARE ? "None" : std::to_string((int)pos.ep_square)) << std::endl;
	std::cout << "Reversible halfmove clock: " << pos.reversible_halfmove_clock << std::endl;
	std::cout << "Move number: " << pos.fullmove_counter << std::endl;
}