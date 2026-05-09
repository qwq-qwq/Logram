#pragma once

#include "core/LogLevel.h"

// Loads the Tokyo Night CSS provider into the default GDK display.
// Call once after gtk_application is up (e.g. from the activate handler).
class Theme {
public:
    static void Apply();
};

// Hex string ("#rrggbb") for the Tokyo Night message-color of a log level.
// Matches LogramWin/src/ui/ThemeColors.cpp messageColor[].
const char* LevelHexColor(LogLevel level);