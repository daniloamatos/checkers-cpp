#pragma once

#include <cstdint>
#include <array>
#include <cstddef> 

struct Board {
    std::uint64_t byType[3]; 
    std::uint64_t byColor[2];
    std::uint8_t squares[64];
    std::uint8_t turn;
};

enum PieceType : std::uint8_t 
{
    OCCUPIED, KING, MAN
};

enum PieceColor : std::uint8_t 
{
    WHITEPIECE, BLACKPIECE
};

struct Move {
    std::uint8_t from;
    std::uint8_t to;
    std::uint64_t capturedPieces;
    std::uint64_t landingSquares;
};

struct MoveList {
    static constexpr std::size_t capacity = 12 * 13;

    std::array<Move, capacity> moves;
    std::size_t count = 0;

    void add(Move move)
    {
        moves.at(count) = move;
        ++count;
    }

    void clear()
    {
        count = 0;
    }
};

void setSquare(Board& board, std::uint8_t square, std::uint8_t piece);
MoveList generateMoves(Board& board, std::uint8_t color);
//MoveResult makeMove(Board& board, Move& move);

