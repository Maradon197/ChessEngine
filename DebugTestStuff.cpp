#include "DataTypes.hpp"
#include "Bitboard.hpp"
#include "DebugTestStuff.hpp"
#include <iostream>

Bitboard getBishopAttacksSlow(Square sq, Bitboard own_occupancy, Bitboard enemy_occupancy) {
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
            Bitboard targetBB = BB::oneSquareBB(target);

            //own piece on current square? stop!
            if(own_occupancy & targetBB) break;
            //else: add and potentially stop if opponents piece
            attacks |= targetBB;

            if(enemy_occupancy & targetBB) break;

            f += dir[0];
            r += dir[1];
        }
    }

    return attacks;
}

Bitboard getRookAttacksSlow(Square sq, Bitboard own_occupancy, Bitboard enemy_occupancy) {
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
            Bitboard targetBB = BB::oneSquareBB(target);

            //own piece on current square? stop!
            if(own_occupancy & targetBB) break;

            //else: add and potentially stop if opponents piece
            attacks |= targetBB;
            if (enemy_occupancy & targetBB)
                break;

            f += dir[0];
            r += dir[1];
        }
    }
    return attacks;
}

Bitboard getQueenAttacksSlow(Square sq, Bitboard occupancy) {
    return getBishopAttacksSlow(sq, occupancy) & getRookAttacksSlow(sq, occupancy);
}