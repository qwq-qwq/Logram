cmake_minimum_required(VERSION 3.24)
project(Logram VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /W4 /MP)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)
endif()

# --- Core library (no Win32 UI dependencies) ---
add_library(logram_core STATIC
    src/core/LogLevel.cpp
    src/core/LogParser.cpp
    src/core/LogDocument.cpp
    src/core/MappedFile.cpp
    src/sql/SqlStats.cpp
    src/sql/SqlFormatter.cpp
    src/sql/SqlParamSubst.cpp
    src/sql/JsonPretty.cpp
)
target_include_directories(logram_core PUBLIC src)
target_precompile_headers(logram_core PUBLIC src/pch.h)

# --- Main executable ---
add_executable(Logram WIN32
    src/main.cpp
    src/ui/App.cpp
    src/ui/MainWindow.cpp
    src/ui/LogTableView.cpp
    src/ui/Splitter.cpp
    src/ui/StatusBar.cpp
    src/ui/FilterSidebar.cpp
    src/ui/DetailPanel.cpp
    src/ui/StatsDialog.cpp
    src/ui/TimingDialog.cpp
    src/ui/ThemeColors.cpp
    src/infra/Settings.cpp
    src/infra/Clipboard.cpp
    src/infra/Utf.cpp
    src/infra/Dpi.cpp
    Logram.rc
)
target_link_libraries(Logram PRIVATE
    logram_core
    d2d1 dwrite dwmapi comctl32 shlwapi uxtheme
)
target_include_directories(Logram PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR})
target_precompile_headers(Logram REUSE_FROM logram_core)

if(MSVC)
    set_target_properties(Logram PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        VS_DPI_AWARE "PerMonitor"
    )
    set_target_properties(logram_core PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

# --- Tests ---
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_executable(parser_tests tests/parser_tests.cpp)
    target_link_libraries(parser_tests PRIVATE logram_core)
    target_include_directories(parser_tests PRIVATE src)
    if(MSVC)
        set_target_properties(parser_tests PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()
    add_test(NAME ParserTests COMMAND parser_tests)
endif()
