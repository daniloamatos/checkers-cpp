# Checkers (C++17)

A desktop American checkers (English draughts) game with a graphical interface and a computer opponent. The rules engine uses 64-bit bitboards, the AI is a time-limited iterative-deepening negamax search with alpha-beta pruning, and the engine is checked against an independent reference implementation with randomized differential tests.

- **Language / standard:** C++17
- **Build system:** CMake (3.16+), cross-platform (Linux GCC/Clang, Windows MSVC via vcpkg)
- **Graphics:** [raylib](https://www.raylib.com/) (windowing, input, rendering)
- **Asset rendering:** [LunaSVG](https://github.com/sammycage/lunasvg) (SVG rasterized at startup, included as a git submodule)
- **Concurrency:** `std::async` / `std::future`, `std::atomic` cancellation
- **Size:** ~1,200 lines of C++ (engine ~400, UI/game loop ~500, tests ~230)

---

## Technical Overview

### 1. Board representation (bitboards)

The position is stored in a compact `Board` struct (`main.h`):

| Field | Type | Purpose |
|---|---|---|
| `byType[3]` | `uint64_t[3]` | Bitboards for occupied squares, kings and men |
| `byColor[2]` | `uint64_t[2]` | Bitboards for white and black pieces |
| `squares[64]` | `uint8_t[64]` | Mailbox array for O(1) piece lookup (color in bit 3, type in the low bits) |
| `turn` | `uint8_t` | Side to move |

`setSquare()` is the single mutation point that keeps the mailbox and all five bitboards consistent. The board uses a full 8x8 / 64-bit layout, so diagonal moves are plain shifts by 7 and 9, with edge-file masks (`0x0101…`, `0x8080…`, `0x0303…`, `0xC0C0…`) preventing wrap-around.

### 2. Move generation

- **Bitwise parallel move detection:** simple steps and capture origins for all pieces of a side are found in a few shift/mask operations (e.g. `pieces & ~border & (enemy << 7) & (~occupied << 14)`), then iterated with `__builtin_ctzll` and `x &= x - 1`.
- **Full multi-jump capture sequences:** a recursive generator (`generateCaptureSequences`) explores every branch of a multi-jump, and only complete sequences are emitted, because a capture cannot stop halfway. Each `Move` records `from`, `to`, a `capturedPieces` bitmask and a `landingSquares` bitmask.
- **Rules implemented:**
  - Men move and capture forward; kings move and capture in all four diagonal directions.
  - Mandatory capture: if any capture exists, all non-capturing moves are filtered out.
  - A man that reaches the far row is crowned, and crowning ends the capture sequence (American rules).
  - A king can finish a capture sequence on its own starting square (closed-loop captures).
  - Win condition: the side to move has no pieces or no legal moves.
- **Ambiguous captures:** two different capture routes can end on the same square. The move structure keeps the captured and landing masks, so both routes stay distinct and playable.

### 3. AI opponent

- **Algorithm:** negamax with alpha-beta pruning (`negamax()`).
- **Iterative deepening:** the bot searches depth 1, 2, 3, … until a 100 ms budget expires, and keeps the result of the last fully completed depth. A timed-out partial search is discarded.
- **Move ordering:** moves are sorted (stable sort) by captured material and promotion bonus so alpha-beta cuts earlier.
- **Evaluation:** material (man = 100, king = 175) plus a small advancement bonus for men approaching promotion, computed with `popcount` over the bitboards.
- **Mate-distance scoring:** terminal losses score `-100000 + ply`, so the engine prefers faster wins and delays unavoidable losses.
- **Cooperative cancellation:** the search checks a `SearchControl` (deadline, `interrupted` flag and an optional `std::atomic<bool>` cancel token) and unwinds cleanly. Timeouts propagate back up the tree instead of being treated as scores.

### 4. Application and UI (`main.cpp`)

- **Non-blocking AI:** the search runs on a background thread via `std::async` on a private copy of the board, so the render loop stays responsive. The result is polled from the main loop with `wait_for(0ms)`. On exit the search is cancelled through the atomic token and joined before resources are released.
- **Rendering:**
  - The board is drawn into a `RenderTexture2D` and only squares that changed since the last frame are redrawn.
  - The texture is scaled to the window, preserving aspect ratio.
  - Window is resizable, supports F11 borderless fullscreen, and has a responsive panel layout (side panels in wide windows, bottom panel in tall ones).
- **SVG assets:** piece graphics (`vectors/*.svg`) are rasterized once at startup with LunaSVG to tile size and uploaded as GPU textures. If an asset is missing, the game falls back to drawn circles so pieces stay visible.
- **Input and interaction:**
  - Click-to-select / click-to-move with highlighting of legal destinations (green) and intermediate landing squares of multi-jumps (amber).
  - A capture-choice dialog (Prev / Next / Play / Cancel) lets the player pick between different capture routes, with captured pieces outlined in red.
  - A game-over dialog with restart.
  - Window coordinates are converted to board coordinates through the current scale and offset.

### 5. Testing (`tests/checkers_test.cpp`, registered with CTest)

- **Differential testing:** an independent, coordinate-based (row/column) reference move generator is compared against the bitboard generator. Results are compared as sets of `(from, to, captured, landing)` tuples.
- **Randomized coverage:** 1,000 seeded random positions (`std::mt19937`, fixed seed for reproducibility), each checked for both colors, with random men and kings and forced kings on promotion rows.
- **State-consistency checks:** after every generated move, `makeMove()` is verified against a position rebuilt from scratch, covering the mailbox, all bitboards, promotion and turn switching.
- **Targeted scenario tests:**
  - Capture cycles (closed loops that end on the starting square).
  - Promotion ending a capture sequence.
  - Alternative capture routes that share an endpoint.
  - Evaluation symmetry and advancement scoring.
  - Mate-distance preference (faster win chosen; slower loss chosen).
  - Time-limit behavior: a completed timed search matches an unlimited one, an expired deadline leaves the previous best move unchanged, and a timeout deep in the tree propagates.
  - Cross-thread cancellation of a running search, with the board left unchanged.
- Assertions stay enabled in Release builds (`-UNDEBUG`).

---

## Project Structure

```
.
├── main.cpp / main.h     UI, rendering, game loop (main.h also holds the engine API and data types)
├── checkers.cpp          Rules engine, move generation, evaluation, search
├── tests/
│   └── checkers_test.cpp Rules, search and concurrency tests (CTest)
├── vectors/              SVG piece graphics (man/king, black/white)
├── lunasvg/              LunaSVG library (git submodule)
└── CMakeLists.txt        Build configuration (game + test targets, asset copy, install rules)
```

## Build and Run

**Dependencies:** CMake 3.16+, a C++17 compiler, raylib, LunaSVG.

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake pkg-config libraylib-dev

# Build and install LunaSVG locally
cmake -S lunasvg -B lunasvg/build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build lunasvg/build
cmake --install lunasvg/build

# Build the game
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

./build/main
```

### Windows (Visual Studio + vcpkg)

```powershell
vcpkg install raylib lunasvg --triplet x64-windows

cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release

.\build\Release\main.exe
```

### Run the tests

```bash
ctest --test-dir build --output-on-failure
```

CMake copies `vectors/` next to the executable after each build, and the game locates it with `GetApplicationDirectory()`, so it works regardless of the working directory.

## Controls

- **Left click:** select a piece, then click a destination square.
- **Capture dialog:** when several capture routes exist, use Prev / Next to preview each one and Play to confirm.
- **F11:** toggle borderless fullscreen.
- **Restart:** start a new game from the game-over dialog.

---
