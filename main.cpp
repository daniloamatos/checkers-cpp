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

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && scale > 0)
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
                if (selectedSquare != -1)
                {
                    for (std::size_t i = 0; i < availableMoves.count; ++i) {
                        if (availableMoves.moves[i].from == square) {
                            selectedSquare = square;
                            break;
                        }

                        if (availableMoves.moves[i].from == selectedSquare && availableMoves.moves[i].to == square) {
                            setSquare(board, square, board.squares[selectedSquare]);
                            setSquare(board, selectedSquare, 0);
                            std::uint64_t captured = availableMoves.moves[i].capturedPieces;
                            while (captured)
                            {
                                setSquare(board, __builtin_ctzll(captured), 0);
                                captured &= captured - 1;
                            }
                            selectedSquare = -1;
                            turn ^= 1;
                            availableMoves = generateMoves(board, turn);
                            break;
                        }
                    }
                }
                if (textureIndex >= 0)
                {
                    const int color = textureIndex < 2 ? BLACKPIECE : WHITEPIECE;
                    if (color == turn)
                    {

                        selectedSquare = square;
                    }
                }
            }
        }

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

                // Impede que o texto ultrapasse o painel em janelas menores.
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

            // Janela larga: painéis nas laterais.
            if (boardArea.x >= 152)
            {
                drawPanel(
                    Rectangle{
                        padding, padding,
                        boardArea.x - 2 * padding,
                        GetScreenHeight() - 2 * padding
                    },
                    "JOGADORES",
                    turn == WHITEPIECE ? "Sua vez" : "Vez das pretas",
                    "Voce - Brancas\n\nComputador - Pretas\n\n"
                    "Tempo\n--:--\n\nDificuldade\nEm breve"
                );

                drawPanel(
                    Rectangle{
                        boardArea.x + boardArea.width + padding,
                        padding,
                        boardArea.x - 2 * padding,
                        GetScreenHeight() - 2 * padding
                    },
                    "PARTIDA",
                    "Historico",
                    "Nenhum lance registrado\n\n"
                    "Pecas capturadas\n--\n\n"
                    "F11 - Tela cheia"
                );
            }
            // Janela alta: painel abaixo do tabuleiro.
            else if (boardArea.y >= 132)
            {
                drawPanel(
                    Rectangle{
                        padding,
                        boardArea.y + boardArea.height + padding,
                        GetScreenWidth() - 2 * padding,
                        boardArea.y - 2 * padding
                    },
                    "PARTIDA",
                    turn == WHITEPIECE ? "Sua vez - Brancas" : "Vez das pretas",
                    "Historico e pecas capturadas em breve"
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

                for (std::size_t i = 0; i < availableMoves.count; ++i)
                {
                    const Move& move = availableMoves.moves[i];
                    if (move.from != selectedSquare)
                        continue;

                    const int destCol = move.to % boardSides;
                    const int destRow = move.to / boardSides;

                    DrawRectangleRec(
                        Rectangle{
                            boardArea.x + destCol * tileSize * scale,
                            boardArea.y + destRow * tileSize * scale,
                            tileSize * scale,
                            tileSize * scale
                        },
                        Color{0, 255, 0, 100}
                    );
                }
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
