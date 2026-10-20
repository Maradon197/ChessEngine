#include "DataTypes.h"
#include "Bitboard.h"
#include <iostream>

Bitboard getBishopAttacksSlow(Square sq, Bitboard occupancy) {
    Bitboard attacks = 0;

    const int file = sq % 8;
    const int rank = sq / 8;

    constexpr int directions[4][2] = {
        { 1,  1},  //up-right
        {-1,  1},  //up-left
        { 1, -1},  //down-right
        {-1, -1}   //down-left
    };

    for (const auto& dir : directions) {
        int f = file + dir[0];
        int r = rank + dir[1];

        while (f >= 0 && f < 8 && r >= 0 && r < 8) {
            Square target = r * 8 + f;
            Bitboard targetBB = Bitboard{ 1 } << target;

            attacks |= targetBB;

            if (occupancy & targetBB)
                break;

            f += dir[0];
            r += dir[1];
        }
    }

    return attacks;
}

Bitboard getRookAttacksSlow(Square sq, Bitboard occupancy) {
    Bitboard attacks = 0;
    const int file = sq % 8;
    const int rank = sq / 8;
    constexpr int directions[4][2] = {
        { 1,  0},  //right
        {-1,  0},  //left
        { 0,  1},  //up
        { 0, -1}   //down
    };
    for (const auto& dir : directions) {
        int f = file + dir[0];
        int r = rank + dir[1];
        while (f >= 0 && f < 8 && r >= 0 && r < 8) {
            Square target = r * 8 + f;
            Bitboard targetBB = Bitboard{ 1 } << target;
            attacks |= targetBB;
            if (occupancy & targetBB)
                break;
            f += dir[0];
            r += dir[1];
        }
    }
    return attacks;
}