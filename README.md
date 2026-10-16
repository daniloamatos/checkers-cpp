# Checkers

A desktop American checkers game written in C++17. Play as White against a computer opponent on a resizable 8×8 board. The game includes compulsory captures, multiple jumps, kings, promotion, undo, and automatic draw detection.

## Play

White moves first. Click one of your pieces, then click a highlighted destination. Final destinations are green; intermediate landing squares in a capture sequence are amber. If several capture sequences reach the same destination, use **Prev** and **Next** to inspect them, then **Play** or **Cancel**. Red outlines mark the pieces captured by the selected sequence.

| Control | Action |
| --- | --- |
| Click **New game** | Restart at any time |
| **Ctrl+N** | Restart at any time |
| **Ctrl+Z** | Undo the latest human move and, if already played, the computer's reply |
| **F11** | Toggle borderless fullscreen |
| **Restart** in the result dialog | Start another game after a win or draw |

The board labels its squares from 0 to 63, starting at the upper left. When there is room for a side or lower panel, the history shows the move count and recent moves in those square numbers. A dash marks an ordinary move (`42-33`); `x` marks a capture (`26x44`).

### Rules and results

- Men move and capture diagonally forward. Kings move one square diagonally in either direction and capture by jumping an adjacent opposing piece; they do not fly across the board.
- A capture is compulsory. A piece must complete every available jump in its chosen capture sequence, but it need not choose the sequence that captures the most pieces.
- A man becomes a king on reaching the far row. Reaching that row during a capture ends the turn.
- A player loses when they have no legal move, including when all their pieces have been captured.
- The game declares a draw when the same board position with the same player to move occurs three times, or after 40 moves by each player without a capture or an uncrowned man advancing. These thresholds follow the [WCDF rules](https://wcdf.net/rules/rules_of_checkers_english.pdf); this desktop game applies them automatically.

The clock, difficulty, and captured-piece totals shown in the side panel are currently placeholders. There is one play mode: human White versus computer Black.

## Requirements

- Linux with a graphical desktop and a C and C++ toolchain. The current source uses GCC/Clang bit-operation intrinsics; a native MSVC build is not configured.
- CMake 3.16 or newer and a C++17 compiler.
- `pkg-config` and [raylib](https://www.raylib.com/) development files. CMake discovers raylib through its `raylib.pc` module; `pkg-config --modversion raylib` should print a version before configuring this project.
- [LunaSVG](https://github.com/sammycage/lunasvg), installed with its CMake package configuration. This repository includes LunaSVG as a Git submodule, along with its Plutovg source, so it can be built locally.
- Thread support supplied by the system toolchain.

The build commands below were verified on Linux with GCC 16, CMake 4.4, raylib 6.0, and the included LunaSVG source. Other GCC/Clang and raylib versions may work, but are not tested here.

## Build and run on Linux

Install CMake, a C/C++ compiler, `pkg-config`, and raylib's development package using your distribution's package manager. Make sure `pkg-config --modversion raylib` succeeds. To get a fresh checkout:

```sh
git clone --recurse-submodules https://github.com/daniloamatos/checkers-cpp.git
cd checkers-cpp
```

From the repository root, build the included LunaSVG source and then the game. The submodule update also works for an existing checkout:

```sh
git submodule update --init --recursive

cmake -S lunasvg -B build/lunasvg \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/deps" \
  -DBUILD_SHARED_LIBS=OFF \
  -DLUNASVG_BUILD_EXAMPLES=OFF
cmake --build build/lunasvg -j
cmake --install build/lunasvg

cmake -S . -B build/app \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/build/deps"
cmake --build build/app -j
./build/app/main
```

The executable is `build/app/main`. CMake copies `vectors/` beside it after each build, and the game looks for those SVG files beside the executable rather than in the current working directory. Keep that directory with the executable when moving it. If an image cannot be loaded, the game draws a simple piece shape in its place.

If LunaSVG is already installed, skip its build and point `CMAKE_PREFIX_PATH` at its installation prefix. The application target is named `main`.

## Run the tests

After configuring and building the project:

```sh
ctest --test-dir build/app --output-on-failure
```

The `checkers_tests` executable tests move generation against an independent coordinate-based reference on fixed and random positions. It also covers promotion, capture routes, board/bitboard consistency, search scoring and cancellation, undo, repetition, and the no-progress draw limit. The tests do not require a graphical desktop.

## How it works

| Component | Details |
| --- | --- |
| Position representation | `Board` holds a 64-square array for piece lookup plus 64-bit occupancy masks by piece type and color. `setSquare` keeps the representations synchronized. |
| Move generation | Bit shifts find diagonal steps and initial jumps. Recursive exploration builds complete multi-jump captures. Each `Move` records its origin, destination, captured-piece mask, and landing-square mask. |
| Computer opponent | An asynchronous iterative-deepening negamax search uses alpha-beta pruning and orders captures and promotions first. Evaluation combines material and the advancement of men; terminal scores prefer quicker wins and later losses. Each search has an approximately 100 ms thinking budget and can be cancelled by undo or restart. |
| Game history | Complete position snapshots support undo and repetition detection. A counter tracks moves without a capture or man advancement for the 40-move-per-side draw condition. |
| Rendering | raylib draws into an 800×800 render texture, updates only changed squares, and scales the board to the window. LunaSVG rasterizes the four SVG piece assets once at startup; the resulting textures are reused. |

Draws are detected from the played game history. The computer's position search itself does not consult that history.

## Repository layout

| Path | Purpose |
| --- | --- |
| `main.cpp` | Window, input, rendering, game flow, and background computer search |
| `main.h` | Shared board, move, search-control, and history types |
| `checkers.cpp` | Move rules, position evaluation, search, and history logic |
| `tests/checkers_test.cpp` | Rule, search, and history tests |
| `vectors/` | SVG art for men and kings in both colors |
| `lunasvg/` | LunaSVG Git submodule and its Plutovg dependency |
