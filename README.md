# Checkers

A desktop checkers (draughts) game built with C++17 and raylib. The game features a graphical interface, SVG pieces, and a computer-controlled opponent.

## Requirements

- CMake 3.16 or newer
- C++17 compiler (GCC/Clang on Linux or Visual Studio on Windows)
- [raylib](https://www.raylib.com/)
- [LunaSVG](https://github.com/sammycage/lunasvg)

## Building on Linux

On Debian or Ubuntu, install the build tools and raylib:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libraylib-dev
```

Install LunaSVG locally:

```bash
cmake -S lunasvg -B lunasvg/build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build lunasvg/build
cmake --install lunasvg/build
```

Configure and build the game:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run it:

```bash
./build/main
```

## Building on Windows

Install Visual Studio with the **Desktop development with C++** workload, CMake, and [vcpkg](https://github.com/microsoft/vcpkg). In PowerShell, install the dependencies using the triplet that matches your target architecture. Example for 64-bit Windows:

```powershell
vcpkg install raylib lunasvg --triplet x64-windows
```

Configure and build from the repository root. Replace the path below with the directory where vcpkg is installed:

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

With the default Visual Studio generator, the executable will be located at `build\Release\main.exe`. To launch it from PowerShell:

```powershell
.\build\Release\main.exe
```

## Features and Controls

- Click on the squares to select and move pieces.
- When there is more than one possible capture, choose one of the options shown on screen.
- Press **F11** to toggle fullscreen mode.
- Use **Restart** on the game over screen to start a new match.

CMake automatically copies the `vectors` folder to the executable's directory. Keep this folder alongside the program when distributing or moving the executable.

## Structure

- `main.cpp`, `main.h`: interface, rendering, and game flow.
- `checkers.cpp`: rules and move search.
- `tests/checkers_test.cpp`: rules tests.
- `vectors/`: SVG images of the pieces.
- `lunasvg/`: vendored source code of the LunaSVG library.
