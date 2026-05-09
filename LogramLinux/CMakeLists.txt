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
# Minimum target: Ubuntu 24.04 LTS, which ships GTK 4.14 / GLib 2.80.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.14)

# --- GResource (app icons) ---
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
set(LOGRAM_GRESOURCE_XML
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/logram.gresource.xml)
set(LOGRAM_GRESOURCE_C
    ${CMAKE_CURRENT_BINARY_DIR}/logram_resources.c)

set(LOGRAM_GRESOURCE_DEPS
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/16x16/apps/com.unitybase.Logram.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/32x32/apps/com.unitybase.Logram.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/64x64/apps/com.unitybase.Logram.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/128x128/apps/com.unitybase.Logram.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/256x256/apps/com.unitybase.Logram.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/512x512/apps/com.unitybase.Logram.png)

add_custom_command(
    OUTPUT ${LOGRAM_GRESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES}
            --target=${LOGRAM_GRESOURCE_C}
            --generate-source
            --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/resources
            ${LOGRAM_GRESOURCE_XML}
    DEPENDS ${LOGRAM_GRESOURCE_XML} ${LOGRAM_GRESOURCE_DEPS}
    VERBATIM)

# --- Main executable ---
add_executable(Logram
    src/main.cpp
    src/ui/App.cpp
    src/ui/MainWindow.cpp
    src/ui/LogTableView.cpp
    src/ui/FilterSidebar.cpp
    src/ui/DetailPanel.cpp
    src/ui/Theme.cpp
    ${LOGRAM_GRESOURCE_C}
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