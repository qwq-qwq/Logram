#pragma once

// Loads the Tokyo Night CSS provider into the default GDK display.
// Call once after gtk_application is up (e.g. from the activate handler).
class Theme {
public:
    static void Apply();
};