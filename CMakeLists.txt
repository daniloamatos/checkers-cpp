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

include(CTest)
if(BUILD_TESTING)
    add_executable(checkers_tests tests/checkers_test.cpp checkers.cpp)
    target_include_directories(checkers_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    # Keep assertions enabled in Release builds as well.
    target_compile_options(checkers_tests PRIVATE -UNDEBUG)
    add_test(NAME checkers_rules COMMAND checkers_tests)
endif()
