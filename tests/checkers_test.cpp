#include "main.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <future>
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
    // Equal progress is neutral; advancing a man helps only its own side.
    Board progress{};
    setSquare(progress, 42, MAN);
    setSquare(progress, 21, (BLACKPIECE << 3) | MAN);
    assert(negamax(progress, 0, WHITEPIECE, -100001, 100001, nullptr, 0) == 0);
    setSquare(progress, 42, 0);
    setSquare(progress, 35, MAN);
    assert(negamax(progress, 0, WHITEPIECE, -100001, 100001, nullptr, 0) == 3);
    assert(negamax(progress, 0, BLACKPIECE, -100001, 100001, nullptr, 0) == -3);
    setSquare(progress, 35, KING);
    setSquare(progress, 21, (BLACKPIECE << 3) | KING);
    assert(negamax(progress, 0, WHITEPIECE, -100001, 100001, nullptr, 0) == 0);

    // Choose a win in three plies over a win in five.
    Board fastWin{};
    for (int square : {5, 44, 60}) setSquare(fastWin, square, KING);
    for (int square : {14, 37}) setSquare(fastWin, square, (BLACKPIECE << 3) | KING);
    Move bestWin = generateMoves(fastWin, WHITEPIECE).moves[0];
    assert(negamax(fastWin, 5, WHITEPIECE, -100001, 100001, &bestWin, 0) == 99997);
    assert(bestWin.from == 5 && bestWin.to == 23);

    // Completed timed searches agree with an unrestricted search.
    SearchControl completed{std::chrono::steady_clock::now() + std::chrono::seconds(30)};
    Move timedBest = generateMoves(fastWin, WHITEPIECE).moves[0];
    assert(negamax(fastWin, 5, WHITEPIECE, -100001, 100001, &timedBest, 0, &completed) == 99997);
    assert(!completed.interrupted);
    assert(timedBest.from == bestWin.from && timedBest.to == bestWin.to);

    // An expired deadline leaves the last completed choice untouched.
    SearchControl expired{std::chrono::steady_clock::now() - std::chrono::seconds(1)};
    Move candidate = timedBest;
    assert(negamax(fastWin, 6, WHITEPIECE, -100001, 100001, &candidate, 0, &expired) == 0);
    assert(expired.interrupted);
    assert(candidate.from == timedBest.from && candidate.to == timedBest.to);

    // A timeout inside the tree propagates back instead of becoming a score.
    Board longSearch{};
    setSquare(longSearch, 17, KING);
    setSquare(longSearch, 46, (BLACKPIECE << 3) | KING);
    SearchControl limited{std::chrono::steady_clock::now() + std::chrono::milliseconds(5)};
    assert(negamax(longSearch, 50, WHITEPIECE, -100001, 100001, nullptr, 0, &limited) == 0);
    assert(limited.interrupted);

    // A background search can be cancelled without changing the game board.
    std::atomic<bool> cancelled{false};
    std::promise<void> started;
    auto startedFuture = started.get_future();
    auto search = std::async(std::launch::async, [&]() {
        SearchControl control{std::chrono::steady_clock::now() + std::chrono::seconds(30), false, &cancelled};
        started.set_value();
        negamax(longSearch, 50, WHITEPIECE, -100001, 100001, nullptr, 0, &control);
        return control.interrupted;
    });
    startedFuture.wait();
    cancelled.store(true);
    assert(search.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    assert(search.get());
    assert(longSearch.squares[17] == KING);
    assert(longSearch.squares[46] == ((BLACKPIECE << 3) | KING));

    // Same endpoint, different captures: either option must remain playable.
    Board alternatives{};
    setSquare(alternatives, 21, KING);
    for (int square : {1, 7, 10, 12, 14, 26, 28, 33, 37, 44, 56, 58})
        setSquare(alternatives, square, (BLACKPIECE << 3) | KING);
    auto options = generateMoves(alternatives, WHITEPIECE);
    bool shortRoute = false, longRoute = false;
    for (std::size_t i = 0; i < options.count; ++i)
    {
        const Move move = options.moves[i];
        if (move.from != 21 || move.to != 53) continue;
        Board after = alternatives;
        makeMove(after, move);
        assert(after.squares[53] == KING && after.squares[21] == 0);
        if (move.capturedPieces == ((1ULL << 28) | (1ULL << 44)))
        {
            shortRoute = true;
            assert(after.squares[28] == 0 && after.squares[44] == 0);
            assert(after.squares[10] != 0 && after.squares[12] != 0 && after.squares[26] != 0);
        }
        if (move.capturedPieces == ((1ULL << 10) | (1ULL << 12) | (1ULL << 26) | (1ULL << 44)))
        {
            longRoute = true;
            assert(after.squares[28] != 0);
            assert(after.squares[10] == 0 && after.squares[12] == 0 && after.squares[26] == 0 && after.squares[44] == 0);
        }
    }
    assert(shortRoute && longRoute);

    // When every move loses, choose four plies over two.
    Board slowLoss{};
    for (int square : {37, 53, 60}) setSquare(slowLoss, square, KING);
    for (int square : {44, 46}) setSquare(slowLoss, square, (BLACKPIECE << 3) | KING);
    Move bestDefense = generateMoves(slowLoss, BLACKPIECE).moves[0];
    assert(negamax(slowLoss, 5, BLACKPIECE, -100001, 100001, &bestDefense, 0) == -99996);
    assert(bestDefense.from == 44 && bestDefense.to == 62);

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
