#pragma once

#include "core/LogLevel.h"

enum class ThemeId { TokyoNight, TTY };

class Theme {
public:
    // (Re)load CSS for the current theme into the default GDK display.
    static void Apply();
    // Switch the active theme. Caller must trigger UI refresh after this
    // (table, sidebar, detail) so that their dynamic Pango markup is rebuilt
    // against the new palette.
    static void SetCurrent(ThemeId id);
    static ThemeId Current();
};

// Hex string ("#rrggbb") for the current theme's message-color of a level.
const char* LevelHexColor(LogLevel level);
// Hex string ("#rrggbb") for the current theme's thread color (cycle of 12).
const char* ThreadHexColor(int threadIdx);