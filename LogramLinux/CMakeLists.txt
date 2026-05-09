cmake_minimum_required(VERSION 3.24)
project(Logram VERSION 1.2.0 LANGUAGES CXX C)

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
            --c-name=logram
            --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/resources
            ${LOGRAM_GRESOURCE_XML}
    DEPENDS ${LOGRAM_GRESOURCE_XML} ${LOGRAM_GRESOURCE_DEPS}
    VERBATIM)
set_source_files_properties(${LOGRAM_GRESOURCE_C} PROPERTIES GENERATED TRUE)

# Wrap the generated C in its own static library — this guarantees CMake
# emits the compile step regardless of how it folds GENERATED sources into
# the main executable.
add_library(logram_resources STATIC ${LOGRAM_GRESOURCE_C})
# The source is GENERATED so CMake can't autodetect the language at
# configure time; tell it.
set_target_properties(logram_resources PROPERTIES
    LINKER_LANGUAGE C
    C_STANDARD 11)
target_link_libraries(logram_resources PUBLIC PkgConfig::GTK4)

# --- Main executable ---
add_executable(Logram
    src/main.cpp
    src/ui/App.cpp
    src/ui/MainWindow.cpp
    src/ui/LogTableView.cpp
    src/ui/FilterSidebar.cpp
    src/ui/DetailPanel.cpp
    src/ui/Theme.cpp
    src/ui/TimingDialog.cpp
    src/ui/StatsDialog.cpp
)
target_include_directories(Logram PRIVATE src)
# logram_resources is wrapped in WHOLE_ARCHIVE so the linker keeps the
# generated GResource constructor symbol — without this, --gc-sections /
# no-as-needed can drop it because nothing references it from main code.
target_link_libraries(Logram PRIVATE
    logram_core
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,logram_resources>"
    PkgConfig::GTK4
)

# --- Install rules (icons + .desktop for GNOME Shell / taskbar) ---
include(GNUInstallDirs)

install(TARGETS Logram
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES resources/com.unitybase.Logram.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

foreach(SIZE 16 32 64 128 256 512)
    install(FILES resources/icons/hicolor/${SIZE}x${SIZE}/apps/com.unitybase.Logram.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/apps)
endforeach()

# --- Tests ---
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_executable(parser_tests ${LOGRAM_SHARED_TESTS}/parser_tests.cpp)
    target_link_libraries(parser_tests PRIVATE logram_core)
    add_test(NAME ParserTests COMMAND parser_tests)
endif()

# --- CPack: .deb package ---
# `cpack -G DEB` from the build dir emits logram_<ver>_<arch>.deb that wraps
# the install rules above. Dependencies are auto-resolved via SHLIBDEPS, so
# `apt install ./logram_*.deb` pulls in the right libgtk-4-1 / libglib etc.
set(CPACK_PACKAGE_NAME              "logram")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR            "Sergey Perekrestov")
set(CPACK_PACKAGE_CONTACT           "Sergey Perekrestov <qwq.z.qwq@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Native UnityBase log analyzer")
set(CPACK_PACKAGE_HOMEPAGE_URL      "https://logram.perek.rest")

set(CPACK_GENERATOR                 "DEB")
set(CPACK_DEBIAN_PACKAGE_NAME       "logram")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sergey Perekrestov <qwq.z.qwq@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION    "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY   "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE   "https://logram.perek.rest")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Native UnityBase log analyzer for Linux.\n Opens multi-gigabyte UB server logs and turns them into a readable\n stream with filters, method timing, SQL highlighting and cust1\n parameter substitution. GTK4 native UI, no Electron.")
# Auto-detect dependencies from the linked .so files. Falls back to a
# manual list if shlibdeps fails (e.g. dpkg-shlibdeps not on PATH).
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS  ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS    "libgtk-4-1 (>= 4.14), libglib2.0-0 (>= 2.80)")
# logram_1.2.0_amd64.deb naming.
set(CPACK_DEBIAN_FILE_NAME          "DEB-DEFAULT")
include(CPack)