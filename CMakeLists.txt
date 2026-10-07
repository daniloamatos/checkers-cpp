cmake_minimum_required(VERSION 3.16)

project(main LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED IMPORTED_TARGET raylib)
find_package(lunasvg REQUIRED)

add_executable(main main.cpp checkers.cpp)
target_link_libraries(main PRIVATE PkgConfig::RAYLIB)
target_link_libraries(main PRIVATE lunasvg::lunasvg)
