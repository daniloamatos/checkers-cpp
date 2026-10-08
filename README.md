# Checkers

Um jogo de damas para desktop, feito em C++17 com raylib. O jogo tem uma interface gráfica, peças em SVG e um oponente controlado pelo computador.

## Requisitos

- CMake 3.16 ou mais recente
- Compilador C++17 (GCC/Clang no Linux ou Visual Studio no Windows)
- [raylib](https://www.raylib.com/)
- [LunaSVG](https://github.com/sammycage/lunasvg)

## Compilar no Linux

No Debian ou Ubuntu, instale as ferramentas de compilação e raylib:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libraylib-dev
```

Instale LunaSVG localmente:

```bash
cmake -S lunasvg -B lunasvg/build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build lunasvg/build
cmake --install lunasvg/build
```

Configure e compile o jogo:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Execute:

```bash
./build/main
```

## Compilar no Windows

Instale o Visual Studio com o componente **Desenvolvimento para desktop com C++**, o CMake e o [vcpkg](https://github.com/microsoft/vcpkg). No PowerShell, instale as dependências usando o triplet correspondente à arquitetura desejada. Exemplo para Windows 64-bit:

```powershell
vcpkg install raylib lunasvg --triplet x64-windows
```

Configure e compile a partir da raiz do repositório. Substitua o caminho abaixo pelo diretório onde o vcpkg foi instalado:

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

Com o gerador padrão do Visual Studio, o executável ficará em `build\Release\main.exe`. Para iniciar pelo PowerShell:

```powershell
.\build\Release\main.exe
```

## Recursos e controles

- Clique nas casas para selecionar e mover as peças.
- Quando houver mais de uma possibilidade de captura, escolha uma das opções exibidas na tela.
- Pressione **F11** para alternar o modo de tela cheia.
- Use **Restart** na tela de fim de jogo para começar outra partida.

O CMake copia automaticamente a pasta `vectors` para o diretório do executável. Mantenha essa pasta junto do programa ao distribuir ou mover o executável.

## Estrutura

- `main.cpp`, `main.h`: interface, desenho e fluxo da partida.
- `checkers.cpp`: regras e busca de jogadas.
- `tests/checkers_test.cpp`: testes das regras.
- `vectors/`: imagens SVG das peças.
- `lunasvg/`: código-fonte vendorizado da biblioteca LunaSVG.

