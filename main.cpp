#include <raylib.h>
#include <algorithm>
#include <array>
#include "main.h"
#include <lunasvg.h>

int main()
{
    constexpr int boardSize = 800;
    constexpr int boardSides = 8;
    constexpr int tileSize = boardSize / boardSides;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(boardSize, boardSize, "Checkers");
    SetWindowMinSize(320, 320);
    SetTargetFPS(30);

    RenderTexture2D boardCanvas = LoadRenderTexture(boardSize, boardSize);
    SetTextureFilter(boardCanvas.texture, TEXTURE_FILTER_BILINEAR);



    const char* pieceFiles[] = {
        "king_black.svg", "man_black.svg",
        "king_white.svg", "man_white.svg"
    };
    Texture2D pieceTextures[4]{};
    for (int i = 0; i < 4; ++i)
    {
        auto svg = lunasvg::Document::loadFromFile(TextFormat("vectors/%s", pieceFiles[i]));
        if (!svg) continue;

        auto bitmap = svg->renderToBitmap(tileSize, tileSize);
        if (bitmap.isNull()) continue;

        bitmap.convertToRGBA();
        Image image{bitmap.data(), static_cast<int>(bitmap.width()),
                    static_cast<int>(bitmap.height()), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        pieceTextures[i] = LoadTextureFromImage(image);
    }

    int turn = WHITEPIECE;
    int selectedSquare = -1;
    int winner = -1;
    std::array<int, boardSides * boardSides> pieceAtSquare;
    pieceAtSquare.fill(-1);

    Board board{};

    auto put = [&](int color, int type, int square)
    {
        const std::uint64_t mask = 1ULL << square;

        board.squares[square] = (color << 3) | type;
        board.byColor[color] |= mask;
        board.byType[OCCUPIED] |= mask;
        board.byType[type] |= mask;
    };

    // Restore the starting position and clear the previous result.
    auto resetGame = [&]()
    {
        board = Board{};
        turn = WHITEPIECE;
        board.turn = turn;
        selectedSquare = -1;
        winner = -1;
        for (int row = 0; row < boardSides; ++row)
        {
            for (int col = 0; col < boardSides; ++col)
            {
                if ((row + col) % 2 == 1)
                {
                    const int square = row * boardSides + col;
                    if (row < 3)
                        put(BLACKPIECE, MAN, square);
                    else if (row >= 5)
                        put(WHITEPIECE, MAN, square);
                }
            }
        }
    };
    resetGame();

    auto drawSquare = [&](int square)
    {
        const int col = square % boardSides;
        const int row = square / boardSides;
        const Color boardColor = (row + col) % 2 == 0
            ? Color{240, 217, 181, 255}
            : Color{181, 136, 99, 255};

        DrawRectangle(col * tileSize, row * tileSize, tileSize, tileSize, boardColor);
        DrawText(TextFormat("%d", square), col * tileSize + 4,
                 row * tileSize + 4, 16, BLACK);

        const int piece = board.squares[square];
        const int textureIndex = piece == 0 ? -1
            : ((piece >> 3) == BLACKPIECE ? 0 : 2) + (piece & 7) - 1;
        pieceAtSquare[square] = textureIndex;
        if (textureIndex >= 0 && pieceTextures[textureIndex].id != 0)
            DrawTexture(pieceTextures[textureIndex], col * tileSize, row * tileSize, WHITE);
    };

    std::array<std::uint8_t, boardSides * boardSides> drawnSquares;
    BeginTextureMode(boardCanvas);
    for (int square = 0; square < boardSides * boardSides; ++square)
    {
        drawSquare(square);
        drawnSquares[square] = board.squares[square];
    }
    EndTextureMode();
    MoveList availableMoves = generateMoves(board, board.turn);

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F11))
            ToggleBorderlessWindowed();

        const float scale = std::min(
            GetScreenWidth() / static_cast<float>(boardSize),
            GetScreenHeight() / static_cast<float>(boardSize)
        );
        const Rectangle boardArea = {
            (GetScreenWidth() - boardSize * scale) / 2.0f,
            (GetScreenHeight() - boardSize * scale) / 2.0f,
            boardSize * scale,
            boardSize * scale
        };

        const float dialogWidth = std::min(400.0f, GetScreenWidth() - 32.0f);
        const Rectangle victoryDialog = {
            (GetScreenWidth() - dialogWidth) / 2.0f,
            (GetScreenHeight() - 220.0f) / 2.0f,
            dialogWidth, 220.0f
        };
        const Rectangle restartButton = {
            victoryDialog.x + 24, victoryDialog.y + 138,
            victoryDialog.width - 48, 56
        };

        // Once the game ends, only the restart button accepts clicks.
        if (winner != -1)
        {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(GetMousePosition(), restartButton))
            {
                resetGame();
                availableMoves = generateMoves(board, turn);
            }
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && scale > 0)
        {
            const Vector2 mouse = GetMousePosition();
            const float x = (mouse.x - boardArea.x) / scale;
            const float y = (mouse.y - boardArea.y) / scale;
            if (x >= 0 && x < boardSize && y >= 0 && y < boardSize)
            {
                const int col = static_cast<int>(x) / tileSize;
                const int row = static_cast<int>(y) / tileSize;
                const int square = row * boardSides + col;
                const int textureIndex = pieceAtSquare[square];
                bool moved = false;
                if (selectedSquare != -1)
                {
                    for (std::size_t i = 0; i < availableMoves.count; ++i)
                    {
                        if (availableMoves.moves[i].from == selectedSquare && availableMoves.moves[i].to == square)
                        {
                            makeMove(board, availableMoves.moves[i]);
                            selectedSquare = -1;
                            turn = board.turn;
                            availableMoves = generateMoves(board, turn);
                            // No pieces or no legal moves means the opponent loses.
                            if (board.byColor[turn] == 0 || availableMoves.count == 0)
                                winner = turn ^ 1;
                            moved = true;
                            break;
                        }
                    }
                }
                if (!moved && textureIndex >= 0)
                {
                    const int color = textureIndex < 2 ? BLACKPIECE : WHITEPIECE;
                    if (color == turn)
                    {

                        selectedSquare = square;
                    }
                }
            }
        }

        // Redraw only squares that changed since the last frame.
        BeginTextureMode(boardCanvas);
        for (int square = 0; square < boardSides * boardSides; ++square)
        {
            if (drawnSquares[square] != board.squares[square])
            {
                drawSquare(square);
                drawnSquares[square] = board.squares[square];
            }
        }
        EndTextureMode();

        BeginDrawing();
            ClearBackground(Color{22, 25, 32, 255});

            auto drawPanel = [](Rectangle area, const char* title,
                                const char* subtitle, const char* text)
            {
                if (area.width < 120 || area.height < 100)
                    return;

                DrawRectangleRounded(area, 0.08f, 8, Color{32, 37, 47, 255});

                // Keep text inside the panel on smaller windows.
                BeginScissorMode(
                    static_cast<int>(area.x),
                    static_cast<int>(area.y),
                    static_cast<int>(area.width),
                    static_cast<int>(area.height)
                );

                const int left = static_cast<int>(area.x) + 16;
                const int top = static_cast<int>(area.y) + 20;

                DrawText(title, left, top, 24, RAYWHITE);
                DrawText(subtitle, left, top + 38, 18, Color{130, 190, 160, 255});
                DrawText(text, left, top + 78, 16, Color{175, 182, 195, 255});

                EndScissorMode();
            };

            const float padding = 16.0f;

            // Place panels beside the board in wide windows.
            if (boardArea.x >= 152)
            {
                drawPanel(
                    Rectangle{
                        padding, padding,
                        boardArea.x - 2 * padding,
                        GetScreenHeight() - 2 * padding
                    },
                    "PLAYERS",
                    turn == WHITEPIECE ? "Your turn" : "Black's turn",
                    "You - White\n\nComputer - Black\n\n"
                    "Time\n--:--\n\nDifficulty\nComing soon"
                );

                drawPanel(
                    Rectangle{
                        boardArea.x + boardArea.width + padding,
                        padding,
                        boardArea.x - 2 * padding,
                        GetScreenHeight() - 2 * padding
                    },
                    "GAME",
                    "History",
                    "No moves recorded\n\n"
                    "Captured pieces\n--\n\n"
                    "F11 - Fullscreen"
                );
            }
            // Place the panel below the board in tall windows.
            else if (boardArea.y >= 132)
            {
                drawPanel(
                    Rectangle{
                        padding,
                        boardArea.y + boardArea.height + padding,
                        GetScreenWidth() - 2 * padding,
                        boardArea.y - 2 * padding
                    },
                    "GAME",
                    turn == WHITEPIECE ? "Your turn - White" : "Black's turn",
                    "History and captured pieces coming soon"
                );
            }
            DrawTexturePro(boardCanvas.texture,
                           Rectangle{0, 0, static_cast<float>(boardSize), -static_cast<float>(boardSize)},
                           boardArea, Vector2{0, 0}, 0, WHITE);
            if (selectedSquare >= 0)
            {
                const int col = selectedSquare % boardSides;
                const int row = selectedSquare / boardSides;

                DrawRectangleRec(
                    Rectangle{
                        boardArea.x + col * tileSize * scale,
                        boardArea.y + row * tileSize * scale,
                        tileSize * scale,
                        tileSize * scale
                    },
                    Color{0, 0, 255, 100}
                );

                // Show final destinations in green and intermediate landings in amber.
                std::uint64_t destinations = 0;
                std::uint64_t intermediateSquares = 0;
                for (std::size_t i = 0; i < availableMoves.count; ++i)
                {
                    const Move& move = availableMoves.moves[i];
                    if (move.from != selectedSquare)
                        continue;

                    destinations |= 1ULL << move.to;
                    if (move.capturedPieces != 0)
                        intermediateSquares |= move.landingSquares & ~(1ULL << move.to);
                }
                // Final destinations take priority when different routes share a square.
                intermediateSquares &= ~destinations;
                std::uint64_t highlighted = destinations | intermediateSquares;
                while (highlighted)
                {
                    const int square = __builtin_ctzll(highlighted);
                    const int destCol = square % boardSides;
                    const int destRow = square / boardSides;
                    const bool isDestination = (destinations & (1ULL << square)) != 0;
                    DrawRectangleRec(
                        Rectangle{
                            boardArea.x + destCol * tileSize * scale,
                            boardArea.y + destRow * tileSize * scale,
                            tileSize * scale,
                            tileSize * scale
                        },
                        isDestination ? Color{0, 255, 0, 100} : Color{255, 190, 60, 70}
                    );
                    highlighted &= highlighted - 1;
                }
            }

            if (winner != -1)
            {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 180});
                DrawRectangleRounded(victoryDialog, 0.08f, 8, Color{32, 37, 47, 255});
                const char* title = winner == WHITEPIECE ? "White wins!" : "Black wins!";
                const int titleSize = 24;
                DrawText(title,
                         static_cast<int>(victoryDialog.x + (victoryDialog.width - MeasureText(title, titleSize)) / 2),
                         static_cast<int>(victoryDialog.y + 32), titleSize, RAYWHITE);
                const char* message = board.byColor[turn] == 0
                    ? "Your opponent has no pieces."
                    : "Your opponent has no moves.";
                DrawText(message,
                         static_cast<int>(victoryDialog.x + (victoryDialog.width - MeasureText(message, 16)) / 2),
                         static_cast<int>(victoryDialog.y + 82), 16, Color{175, 182, 195, 255});
                const bool hovered = CheckCollisionPointRec(GetMousePosition(), restartButton);
                DrawRectangleRounded(restartButton, 0.15f, 8,
                                     hovered ? Color{105, 200, 145, 255} : Color{75, 170, 115, 255});
                DrawText("Restart",
                         static_cast<int>(restartButton.x + (restartButton.width - MeasureText("Restart", 22)) / 2),
                         static_cast<int>(restartButton.y + (restartButton.height - 22) / 2),
                         22, Color{22, 25, 32, 255});
            }
        EndDrawing();
    }

    for (const Texture2D texture : pieceTextures)
    {
        if (texture.id != 0)
            UnloadTexture(texture);
    }
    UnloadRenderTexture(boardCanvas);
    CloseWindow();
    return 0;
}
