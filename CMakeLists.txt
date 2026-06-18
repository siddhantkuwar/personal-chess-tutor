cmake_minimum_required(VERSION 3.25)

project(personal_chess_tutor VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PCT_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(PCT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

add_library(pct_core
    src/common/json.cpp
    src/app/repository.cpp
    src/app/job_manager.cpp
    src/chess/board.cpp
    src/chess/fen.cpp
    src/chess/san.cpp
    src/chess/pgn.cpp
    src/import/import_service.cpp
    src/engine/stockfish.cpp
    src/analysis/analyzer.cpp
    src/storage/event_log.cpp
    src/service/http_server.cpp
)

target_include_directories(pct_core PUBLIC include)
target_link_libraries(pct_core PUBLIC Threads::Threads CURL::libcurl)

if(MSVC)
    target_compile_options(pct_core PRIVATE /W4)
else()
    target_compile_options(pct_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(PCT_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(pct_core PRIVATE /WX)
    else()
        target_compile_options(pct_core PRIVATE -Werror)
    endif()
endif()

if(PCT_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(pct_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(pct_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(personal-chess-tutor src/main.cpp)
target_link_libraries(personal-chess-tutor PRIVATE pct_core)

add_executable(pct-cli src/cli.cpp)
target_link_libraries(pct-cli PRIVATE pct_core)

if(BUILD_TESTING)
    add_executable(pct-tests
        tests/test_main.cpp
        tests/chess_tests.cpp
        tests/pgn_tests.cpp
        tests/import_tests.cpp
        tests/engine_tests.cpp
        tests/analysis_tests.cpp
        tests/storage_tests.cpp
        tests/json_tests.cpp
        tests/repository_tests.cpp
        tests/job_tests.cpp
        tests/api_tests.cpp
    )
    target_link_libraries(pct-tests PRIVATE pct_core)
    add_executable(pct-fake-stockfish tests/fake_stockfish.cpp)
    add_executable(pct-crash-stockfish tests/crash_stockfish.cpp)
    add_executable(pct-slow-stockfish tests/slow_stockfish.cpp)
    add_dependencies(pct-tests pct-fake-stockfish pct-crash-stockfish pct-slow-stockfish)
    target_compile_definitions(pct-tests PRIVATE
        PCT_FAKE_STOCKFISH_PATH="$<TARGET_FILE:pct-fake-stockfish>"
        PCT_CRASH_STOCKFISH_PATH="$<TARGET_FILE:pct-crash-stockfish>"
        PCT_SLOW_STOCKFISH_PATH="$<TARGET_FILE:pct-slow-stockfish>")
    add_test(NAME pct-tests COMMAND pct-tests)
endif()
