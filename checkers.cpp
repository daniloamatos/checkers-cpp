#include "main.h"
#include <algorithm>
#include <iostream>
#include <limits>


// Score the position from the current player's perspective.
int evaluate(const Board& board, std::uint8_t color)
{
    const std::uint64_t own = board.byColor[color];
    const std::uint64_t enemy = board.byColor[color ^ 1];

    const int men =
        __builtin_popcountll(own & board.byType[MAN]) -
        __builtin_popcountll(enemy & board.byType[MAN]);

    const int kings =
        __builtin_popcountll(own & board.byType[KING]) -
        __builtin_popcountll(enemy & board.byType[KING]);

    // Give men a small bonus for getting closer to promotion.
    auto advancement = [&](std::uint8_t side)
    {
        int bonus = 0;
        std::uint64_t pieces = board.byColor[side] & board.byType[MAN];
        while (pieces)
        {
            const int row = __builtin_ctzll(pieces) / 8;
            bonus += (side == WHITEPIECE ? 7 - row : row) * 3;
            pieces &= pieces - 1;
        }
        return bonus;
    };

    return men * 100 + kings * 175 + advancement(color) - advancement(color ^ 1);
}

// Try valuable captures and promotions first to help alpha-beta cut sooner.
int movePriority(const Board& board, const Move& move)
{
    const int men = __builtin_popcountll(move.capturedPieces & board.byType[MAN]);
    const int kings = __builtin_popcountll(move.capturedPieces & board.byType[KING]);
    int priority = men * 100 + kings * 175;

    const std::uint8_t piece = board.squares[move.from];
    const std::uint8_t color = piece >> 3;
    if ((piece & 7) == MAN && (color == WHITEPIECE ? move.to < 8 : move.to >= 56))
        priority += 75;

    return priority;
}

int negamax(Board board, int depth, std::uint8_t color, int alpha, int beta, Move* bestMove, int ply, SearchControl* control)
{
    // Discard unfinished searches when the time runs out.
    if (control && (control->interrupted ||
        (control->cancelled && control->cancelled->load()) ||
        std::chrono::steady_clock::now() >= control->deadline))
    {
        control->interrupted = true;
        return 0;
    }
    MoveList moves = generateMoves(board, color);
    int maxEval = -100000;
    if (moves.count == 0)
    {
        // Prefer faster wins and postpone unavoidable losses.
        return -100000 + ply;
    }
    if (depth <= 0)
    {
        return evaluate(board, color);
    }

    // Keep the original order when priorities are equal.
    std::stable_sort(moves.moves.begin(), moves.moves.begin() + moves.count,
        [&board](const Move& left, const Move& right)
        {
            return movePriority(board, left) > movePriority(board, right);
        });

    for (std::size_t i = 0; i < moves.count; ++i)
    {
        Board nextBoard = board;
        makeMove(nextBoard, moves.moves[i]);

        // Switch perspective and reverse the search limits.
        int score = -negamax(nextBoard, depth - 1, color ^ 1, -beta, -alpha, nullptr, ply + 1, control);
        if (control && control->interrupted)
            return 0;
        if (score > maxEval)
        {
            maxEval = score;
            if (bestMove != nullptr)
                *bestMove = moves.moves[i];
        }
        // Stop when the opponent would avoid this branch.
        alpha = std::max(alpha, score);
        if (alpha >= beta)
            break;
    }
    return maxEval;
};
// Keep the square and all piece masks in sync.
void setSquare(Board& board, std::uint8_t square, std::uint8_t piece)
{
    const std::uint64_t source = 1ULL << square;
    const std::uint8_t old = board.squares[square];

    if (old != 0)
    {
        const std::uint8_t oldColor = old >> 3;
        const std::uint8_t oldType = old & 7;
        board.byColor[oldColor] &= ~source;
        board.byType[oldType] &= ~source;
        board.byType[OCCUPIED] &= ~source; 
    }
    board.squares[square] = piece;
    const std::uint64_t &destination = source;
    if (piece != 0)
    {
        const std::uint8_t newColor = piece >> 3;
        const std::uint8_t newType = piece & 7;
        board.byColor[newColor] |= destination;
        board.byType[newType] |= destination;
        board.byType[OCCUPIED] |= destination; 
    }
};
// Each set bit marks a piece that can start a capture.
struct CaptureOrigins 
{
    std::uint64_t from7;
    std::uint64_t from9;
};

CaptureOrigins findCaptures(const Board& board, std::uint8_t color, std::uint64_t pieces, bool backwards = false) 
{
    const std::uint64_t enemyPieces = board.byColor[color ^ 1];
    const std::uint64_t occupied = board.byType[OCCUPIED];

    // A jump needs room for two squares before reaching an edge.
    constexpr std::uint64_t borderAB = 0x0303030303030303ULL;
    constexpr std::uint64_t borderGH = 0xC0C0C0C0C0C0C0C0ULL;

    // Kings also search in the opposite direction.
    if ((color == WHITEPIECE) != backwards)
    {
        return 
        {
            pieces & ~borderGH & (enemyPieces << 7) & (~occupied << 7 * 2),
            pieces & ~borderAB & (enemyPieces << 9) & (~occupied << 9 * 2)
        };
    }

    return 
    {
        pieces & ~borderAB & (enemyPieces >> 7) & (~occupied >> 7 * 2),
        pieces & ~borderGH & (enemyPieces >> 9) & (~occupied >> 9 * 2)
    };
}

void generateCaptureSequences(Board board, std::uint8_t color, Move sequence, MoveList& moves)
{
    // Rebuild the accumulated sequence from the original board.
    Board nextBoard = board;
    std::uint8_t piece = board.squares[sequence.from];

    setSquare(nextBoard, sequence.from, 0);
    setSquare(nextBoard, sequence.to, piece);

    std::uint64_t captured = sequence.capturedPieces;
    while (captured)
    {
        setSquare(nextBoard, __builtin_ctzll(captured), 0);
        captured &= captured - 1;
    }

    // In American checkers, crowning ends the capture sequence.
    if ((piece & 7) == MAN &&
        (color == WHITEPIECE ? sequence.to < 8 : sequence.to >= 56))
    {
        moves.add(sequence);
        return;
    }

    bool hasContinuation = false;
    // Men capture forward; kings can capture both ways.
    const int directions = (piece & 7) == KING ? 2 : 1;
    for (int backwards = 0; backwards < directions; ++backwards)
    {
        CaptureOrigins attacks = findCaptures(nextBoard, color, 1ULL << sequence.to, backwards);
        const int direction = ((color == WHITEPIECE) != bool(backwards)) ? -1 : 1;
        for (int offset : {7, 9})
        {
            if ((offset == 7 ? attacks.from7 : attacks.from9) == 0)
                continue;

            hasContinuation = true;
            const std::uint8_t capturedSquare = sequence.to + direction * offset;
            const std::uint8_t nextTo = sequence.to + direction * offset * 2;
            // Explore each possible next jump with its own sequence.
            Move nextSequence = sequence;
            nextSequence.to = nextTo;
            nextSequence.capturedPieces |= 1ULL << capturedSquare;
            nextSequence.landingSquares |= 1ULL << nextTo;
            // Keep the original board so each branch starts from the same position.
            generateCaptureSequences(board, color, nextSequence, moves);
        }
    }
    // Save only complete sequences: a capture cannot stop halfway.
    if (!hasContinuation)
        moves.add(sequence);
}

// Generate forward steps and full capture sequences for men.
void generateManMoves(const Board& board, std::uint8_t color, std::uint64_t occupied, MoveList& moves)
{
    std::uint64_t ownPieces = board.byColor[color] & board.byType[MAN];

    constexpr std::uint64_t borderA = 0x0101010101010101ULL;
    constexpr std::uint64_t borderH = 0x8080808080808080ULL;


    if (color == WHITEPIECE)
    {
        std::uint64_t from7 = ownPieces & ~borderH & (~occupied << 7);
        std::uint64_t from9 = ownPieces & ~borderA & (~occupied << 9);
        
        CaptureOrigins attacks = findCaptures(board, color, ownPieces);
        
        
        bool hasAttack = (attacks.from7 | attacks.from9) != 0;
        while (from7 && !hasAttack)
        {
            const std::uint8_t from = __builtin_ctzll(from7);
            const std::uint8_t to = from - 7;

            moves.add(Move{from, to});
            from7 &= from7 - 1;
        }

        while (from9 && !hasAttack)
        {
            const std::uint8_t from = __builtin_ctzll(from9);
            const std::uint8_t to = from - 9;

            moves.add(Move{from, to});
            from9 &= from9 - 1;
        }
        while (attacks.from7)
        {
            const std::uint8_t from = __builtin_ctzll(attacks.from7);
            const std::uint8_t to = from - 7 * 2;

            generateCaptureSequences(board, color, Move{from, to, 1ULL << (from - 7), 1ULL << to}, moves);
            attacks.from7 &= attacks.from7 - 1;
        }
        while (attacks.from9)
        {
            const std::uint8_t from = __builtin_ctzll(attacks.from9);
            const std::uint8_t to = from - 9 * 2;

            generateCaptureSequences(board, color, Move{from, to, 1ULL << (from - 9), 1ULL << to}, moves);
            attacks.from9 &= attacks.from9 - 1;
        }
           
    }
    else
    {
        std::uint64_t from7 = ownPieces & ~borderA & (~occupied >> 7);

        std::uint64_t from9 = ownPieces & ~borderH & (~occupied >> 9);

        CaptureOrigins attacks = findCaptures(board, color, ownPieces);
        const bool hasAttack = (attacks.from7 | attacks.from9) != 0;
        while (from7 && !hasAttack)
        {
            const std::uint8_t from = __builtin_ctzll(from7);
            const std::uint8_t to = from + 7;

            moves.add(Move{from, to});
            from7 &= from7 - 1;
        }

        while (from9 && !hasAttack)
        {
            const std::uint8_t from = __builtin_ctzll(from9);
            const std::uint8_t to = from + 9;
            
            moves.add(Move{from, to});
            from9 &= from9 - 1;
        }
        while (attacks.from7)
        {
            const std::uint8_t from = __builtin_ctzll(attacks.from7);
            const std::uint8_t to = from + 7 * 2;
            
            generateCaptureSequences(board, color, Move{from, to, 1ULL << (from + 7), 1ULL << to}, moves);
            attacks.from7 &= attacks.from7 - 1;
        }
        while (attacks.from9)
        {
            const std::uint8_t from = __builtin_ctzll(attacks.from9);
            const std::uint8_t to = from + 9 * 2;

            generateCaptureSequences(board, color, Move{from, to, 1ULL << (from + 9), 1ULL << to}, moves);
            attacks.from9 &= attacks.from9 - 1;
        }
    }


}


// Kings take single steps or jumps in all four diagonal directions.
void generateKingMoves(const Board& board, std::uint8_t color, MoveList& moves)
{
    const std::uint64_t ownPieces = board.byColor[color] & board.byType[KING];
    const std::uint64_t occupied = board.byType[OCCUPIED];
    constexpr std::uint64_t borderA = 0x0101010101010101ULL;
    constexpr std::uint64_t borderH = 0x8080808080808080ULL;

    for (int backwards = 0; backwards < 2; ++backwards)
    {
        const bool upwards = (color == WHITEPIECE) != bool(backwards);
        const int direction = upwards ? -1 : 1;
        std::uint64_t from7 = upwards
            ? ownPieces & ~borderH & (~occupied << 7)
            : ownPieces & ~borderA & (~occupied >> 7);
        std::uint64_t from9 = upwards
            ? ownPieces & ~borderA & (~occupied << 9)
            : ownPieces & ~borderH & (~occupied >> 9);
        CaptureOrigins attacks = findCaptures(board, color, ownPieces, backwards);

        while (from7)
        {
            const std::uint8_t from = __builtin_ctzll(from7);
            const std::uint8_t to = from + direction * 7;
            moves.add(Move{from, to});
            from7 &= from7 - 1;
        }
        while (from9)
        {
            const std::uint8_t from = __builtin_ctzll(from9);
            const std::uint8_t to = from + direction * 9;
            moves.add(Move{from, to});
            from9 &= from9 - 1;
        }
        while (attacks.from7)
        {
            const std::uint8_t from = __builtin_ctzll(attacks.from7);
            const std::uint8_t to = from + direction * 7 * 2;
            generateCaptureSequences(board, color,
                Move{from, to, 1ULL << (from + direction * 7), 1ULL << to}, moves);
            attacks.from7 &= attacks.from7 - 1;
        }
        while (attacks.from9)
        {
            const std::uint8_t from = __builtin_ctzll(attacks.from9);
            const std::uint8_t to = from + direction * 9 * 2;
            generateCaptureSequences(board, color,
                Move{from, to, 1ULL << (from + direction * 9), 1ULL << to}, moves);
            attacks.from9 &= attacks.from9 - 1;
        }
    }
}

MoveList generateMoves(Board& board, std::uint8_t color)
{
    MoveList moves;
    generateManMoves(board, color, board.byType[OCCUPIED], moves);
    generateKingMoves(board, color, moves);

    // If any piece can capture, remove all non-capturing moves.
    const auto end = moves.moves.begin() + moves.count;
    if (std::any_of(moves.moves.begin(), end,
                    [](const Move& move) { return move.capturedPieces != 0; }))
    {
        moves.count = std::remove_if(moves.moves.begin(), end,
            [](const Move& move) { return move.capturedPieces == 0; }) - moves.moves.begin();
    }
    return moves;
}

// Apply the chosen sequence, then pass the turn to the opponent.
void makeMove(Board& board, const Move& move)
{
    std::uint8_t piece = board.squares[move.from];
    const std::uint8_t color = piece >> 3;
    // Clear the origin first: a king can finish a capture on its starting square.
    setSquare(board, move.from, 0);
    std::uint64_t captured = move.capturedPieces;
    while (captured)
    {
        setSquare(board, __builtin_ctzll(captured), 0);
        captured &= captured - 1;
    }
    // A man reaching the far row becomes a king.
    if ((piece & 7) == MAN && (color == WHITEPIECE ? move.to < 8 : move.to >= 56))
        piece = (color << 3) | KING;
    setSquare(board, move.to, piece);
    board.turn = color ^ 1;
}
