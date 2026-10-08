cmake_minimum_required(VERSION 3.16)

project(main LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED IMPORTED_TARGET raylib)
find_package(lunasvg REQUIRED)
find_package(Threads REQUIRED)

add_executable(main main.cpp checkers.cpp)
target_link_libraries(main PRIVATE PkgConfig::RAYLIB)
target_link_libraries(main PRIVATE lunasvg::lunasvg)
target_link_libraries(main PRIVATE Threads::Threads)

# Keep piece images beside the executable, regardless of the working directory.
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/vectors" "$<TARGET_FILE_DIR:main>/vectors")
install(TARGETS main RUNTIME DESTINATION bin)
install(DIRECTORY vectors DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    add_executable(checkers_tests tests/checkers_test.cpp checkers.cpp)
    target_include_directories(checkers_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(checkers_tests PRIVATE Threads::Threads)
    # Keep assertions enabled in Release builds as well.
    target_compile_options(checkers_tests PRIVATE -UNDEBUG)
    add_test(NAME checkers_rules COMMAND checkers_tests)
endif()
