#include "main.h"
#include <iostream>
#include <immintrin.h>

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
struct CaptureOrigins 
{
    std::uint64_t from7;
    std::uint64_t from9;
};

CaptureOrigins findCaptures(const Board& board, std::uint8_t color, std::uint64_t pieces) 
{
    const std::uint64_t enemyPieces = board.byColor[color ^ 1];
    const std::uint64_t occupied = board.byType[OCCUPIED];

    constexpr std::uint64_t borderAB = 0x0303030303030303ULL;
    constexpr std::uint64_t borderGH = 0xC0C0C0C0C0C0C0C0ULL;

    if (color == WHITEPIECE)
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

    CaptureOrigins attacks = findCaptures(nextBoard, color, 1ULL << sequence.to);
    if ((attacks.from7 | attacks.from9) == 0)
    {
        moves.add(sequence);
        return;
    }

    const int direction = color == WHITEPIECE ? -1 : 1;

    for (int offset : {7, 9})
    {
        if ((offset == 7 ? attacks.from7 : attacks.from9) == 0)
            continue;

        const std::uint8_t capturedSquare = sequence.to + direction * offset;
        const std::uint8_t nextTo = sequence.to + direction * offset * 2;

        Move nextSequence = sequence;
        nextSequence.to = nextTo;
        nextSequence.capturedPieces |= 1ULL << capturedSquare;
        nextSequence.landingSquares |= 1ULL << nextTo;

        generateCaptureSequences(board, color, nextSequence, moves);
    }
}

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

    std::cout << "moves.count = " << moves.count << '\n';
}


MoveList generateMoves(Board& board, std::uint8_t color)
{
    std::uint64_t ownPieces =  board.byColor[color] ;
    std::uint64_t enemyPieces =  board.byColor[color ^ 1]; 
    std::uint64_t occupied = board.byType[OCCUPIED];

    MoveList moves;

    generateManMoves(board, color, occupied, moves);
    return moves;
};

