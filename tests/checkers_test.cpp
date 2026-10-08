#include "main.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <set>
#include <tuple>

using Entry = std::tuple<int, int, std::uint64_t, std::uint64_t>;
using Position = std::array<std::uint8_t, 64>;

// Independent coordinate-based reference for the bitboard generator.
void captures(Position position, int color, int square, Move sequence, std::set<Entry>& result)
{
    const int piece = position[square];
    const bool king = (piece & 7) == KING;
    bool continued = false;
    if (king || !(color == WHITEPIECE ? square < 8 : square >= 56))
    {
        for (int dr : {-1, 1})
        {
            if (!king && dr != (color == WHITEPIECE ? -1 : 1)) continue;
            for (int dc : {-1, 1})
            {
                int r = square / 8 + dr * 2, c = square % 8 + dc * 2;
                if (r < 0 || r >= 8 || c < 0 || c >= 8) continue;
                int to = r * 8 + c;
                int mid = (square / 8 + dr) * 8 + square % 8 + dc;
                if (position[to] || !position[mid] || (position[mid] >> 3) == color) continue;
                Position next = position;
                next[square] = next[mid] = 0;
                next[to] = piece;
                Move move = sequence;
                move.to = to;
                move.capturedPieces |= 1ULL << mid;
                move.landingSquares |= 1ULL << to;
                captures(next, color, to, move, result);
                continued = true;
            }
        }
    }
    if (!continued && sequence.capturedPieces)
        result.emplace(sequence.from, sequence.to, sequence.capturedPieces, sequence.landingSquares);
}

void verify(Board board, int color)
{
    Position position;
    std::copy(std::begin(board.squares), std::end(board.squares), position.begin());
    std::set<Entry> expected, actual;
    for (int from = 0; from < 64; ++from)
        if (position[from] && (position[from] >> 3) == color)
            captures(position, color, from, Move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(from)}, expected);
    if (expected.empty())
    {
        for (int from = 0; from < 64; ++from)
        {
            if (!position[from] || (position[from] >> 3) != color) continue;
            for (int dr : {-1, 1})
            {
                if ((position[from] & 7) == MAN && dr != (color == WHITEPIECE ? -1 : 1)) continue;
                for (int dc : {-1, 1})
                {
                    int r = from / 8 + dr, c = from % 8 + dc;
                    if (r >= 0 && r < 8 && c >= 0 && c < 8 && !position[r * 8 + c])
                        expected.emplace(from, r * 8 + c, 0, 0);
                }
            }
        }
    }
    MoveList moves = generateMoves(board, color);
    for (std::size_t i = 0; i < moves.count; ++i)
    {
        const Move move = moves.moves[i];
        actual.emplace(move.from, move.to, move.capturedPieces, move.landingSquares);
        Board after = board;
        makeMove(after, move);
        Board rebuilt{};
        for (int square = 0; square < 64; ++square)
        {
            int piece = position[square];
            if (square == move.from || (move.capturedPieces & (1ULL << square))) piece = 0;
            if (square == move.to)
            {
                piece = position[move.from];
                if ((piece & 7) == MAN && (color == WHITEPIECE ? square < 8 : square >= 56))
                    piece = (color << 3) | KING;
            }
            assert(after.squares[square] == piece);
            setSquare(rebuilt, square, piece);
        }
        assert(after.turn == (color ^ 1));
        for (int i = 0; i < 3; ++i) assert(after.byType[i] == rebuilt.byType[i]);
        for (int i = 0; i < 2; ++i) assert(after.byColor[i] == rebuilt.byColor[i]);
    }
    assert(actual == expected);
}

int main()
{
    Board cycle{};
    setSquare(cycle, 42, KING);
    for (int square : {35, 19, 17, 33}) setSquare(cycle, square, (BLACKPIECE << 3) | MAN);
    verify(cycle, WHITEPIECE);
    auto cycles = generateMoves(cycle, WHITEPIECE);
    assert(cycles.count == 2);
    for (std::size_t i = 0; i < cycles.count; ++i) assert(cycles.moves[i].to == 42);

    Board promotion{};
    setSquare(promotion, 17, MAN);
    setSquare(promotion, 10, (BLACKPIECE << 3) | MAN);
    setSquare(promotion, 12, (BLACKPIECE << 3) | MAN);
    verify(promotion, WHITEPIECE);
    auto promotions = generateMoves(promotion, WHITEPIECE);
    assert(promotions.count == 1 && promotions.moves[0].to == 3);
    assert(promotions.moves[0].capturedPieces == (1ULL << 10));
    makeMove(promotion, promotions.moves[0]);
    assert(promotion.squares[3] == KING && promotion.squares[12] != 0);

    std::mt19937 rng(42);
    for (int trial = 0; trial < 1000; ++trial)
    {
        Board board{};
        for (int square = 0; square < 64; ++square)
        {
            if ((square / 8 + square % 8) % 2 == 0 || rng() % 3 != 0) continue;
            int color = rng() % 2;
            int type = rng() % 2 ? KING : MAN;
            if (color == WHITEPIECE ? square < 8 : square >= 56) type = KING;
            setSquare(board, square, (color << 3) | type);
        }
        verify(board, WHITEPIECE);
        verify(board, BLACKPIECE);
    }
    std::cout << "Checkers tests passed\n";
}
