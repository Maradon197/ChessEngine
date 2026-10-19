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
}

#endif
