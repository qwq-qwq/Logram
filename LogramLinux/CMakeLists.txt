cmake_minimum_required(VERSION 3.24)
project(Logram VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Shared source tree lives in ../LogramWin/src — single source of truth.
set(LOGRAM_SHARED_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../LogramWin/src")
set(LOGRAM_SHARED_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/../LogramWin/tests")

# --- Core library (parser, document model, SQL helpers; no UI) ---
add_library(logram_core STATIC
    ${LOGRAM_SHARED_SRC}/core/LogLevel.cpp
    ${LOGRAM_SHARED_SRC}/core/LogParser.cpp
    ${LOGRAM_SHARED_SRC}/core/LogDocument.cpp
    ${LOGRAM_SHARED_SRC}/core/MappedFile.cpp
    ${LOGRAM_SHARED_SRC}/sql/SqlStats.cpp
    ${LOGRAM_SHARED_SRC}/sql/SqlFormatter.cpp
    ${LOGRAM_SHARED_SRC}/sql/SqlParamSubst.cpp
    ${LOGRAM_SHARED_SRC}/sql/JsonPretty.cpp
)
target_include_directories(logram_core PUBLIC ${LOGRAM_SHARED_SRC})
target_precompile_headers(logram_core PUBLIC ${LOGRAM_SHARED_SRC}/pch.h)

# pthread is needed for std::jthread / std::thread on Linux.
find_package(Threads REQUIRED)
target_link_libraries(logram_core PUBLIC Threads::Threads)

# --- GTK4 (UI) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

# --- Main executable ---
add_executable(Logram
    src/main.cpp
    src/ui/App.cpp
    src/ui/MainWindow.cpp
    src/ui/LogTableView.cpp
)
target_include_directories(Logram PRIVATE src)
target_link_libraries(Logram PRIVATE
    logram_core
    PkgConfig::GTK4
)

# --- Tests ---
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_executable(parser_tests ${LOGRAM_SHARED_TESTS}/parser_tests.cpp)
    target_link_libraries(parser_tests PRIVATE logram_core)
    add_test(NAME ParserTests COMMAND parser_tests)
endif()