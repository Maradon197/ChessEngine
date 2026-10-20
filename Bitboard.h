#ifndef BITBOARD
#define BITBOARD

#include "DataTypes.h"
#include <cstdint>
#if defined(_MSC_VER)
  #include <intrin.h>
  #pragma intrinsic(__popcnt64)
#endif

namespace BB {

	constexpr Bitboard oneSquareBB(Square sq) {
		return Bitboard{ 1 } << sq;
	}

	inline void addSquare(Bitboard& bb, Square sq) {
		bb |= (Bitboard{ 1 } << sq);
	}

	inline void remSquare(Bitboard& bb, Square sq) {
		bb = bb & ~(Bitboard{ 1 } << sq);
	}

	inline void toggleSquare(Bitboard& bb, Square sq) {
		bb ^=  (Bitboard{ 1 } << sq);	//this is a logical XOR
	}

	inline std::uint8_t popCount(Bitboard bb) {
		//use compiler-owned population counting function
	#if defined(_MSC_VER)
		return static_cast<std::uint8_t>(__popcnt64(bb));

	#elif defined(__GNUC__) || defined(__clang__)
		return static_cast<std::uint8_t>(__builtin_popcountll(bb));

	#else
		//this is ugly but it works for some Robin with a compiler from 1961
		std::uint8_t count = 0;
		while (bb) {
			count += bb & 1;
			bb >>= 1;
		}
		return count;
	#endif
	}

	constexpr Bitboard getDiagonalMask(Square sq) {
		Bitboard mask = 0;

		const int file = sq % 8;
		const int rank = sq / 8;

		//crappy runtime, but making this constexpr so the compiler
		//can optimize away
		for (int i = 1; i < 8; ++i) {
			//up-right
			if (file + i < 8 && rank + i < 8)
				mask |= Bitboard{ 1 } << (sq + 9 * i);

			//up-left
			if (file - i >= 0 && rank + i < 8)
				mask |= Bitboard{ 1 } << (sq + 7 * i);

			//down-right
			if (file + i < 8 && rank - i >= 0)
				mask |= Bitboard{ 1 } << (sq - 7 * i);

			//down-left
			if (file - i >= 0 && rank - i >= 0)
				mask |= Bitboard{ 1 } << (sq - 9 * i);
		}

		return mask;
	}

	constexpr Bitboard getFileMask(Square sq) {
		return	(0x0101010101010101ULL << (sq % 8) ^ oneSquareBB(sq));
	}

	constexpr Bitboard getRankMask(Square sq) {
		return (0xFFULL << (8 * (sq / 8)) ^ oneSquareBB(sq));	//0 is LSB so (int) /8 and then *8 is correct
	}

	constexpr Bitboard getFileRankMask(Square sq) {
		return getFileMask(sq) | getRankMask(sq);
	}
}
	
#endif
