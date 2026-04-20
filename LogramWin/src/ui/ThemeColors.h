#pragma once
#include "core/LogLevel.h"
#include <cstdint>
#include <array>
#include <string>

#ifdef _WIN32
#include <d2d1.h>
#endif

struct ColorRGBA {
    float r, g, b, a;
};

// Named MakeColor / MakeColorA instead of RGB/RGBA to avoid clashing with
// the Windows RGB macro from wingdi.h, which is textually substituted.
inline constexpr ColorRGBA MakeColor(float r, float g, float b) { return {r, g, b, 1.0f}; }
inline constexpr ColorRGBA MakeColorA(float r, float g, float b, float a) { return {r, g, b, a}; }

#ifdef _WIN32
inline D2D1_COLOR_F ToD2D(ColorRGBA c) { return {c.r, c.g, c.b, c.a}; }
inline COLORREF ToCOLORREF(ColorRGBA c) {
    return RGB(static_cast<BYTE>(c.r * 255),
               static_cast<BYTE>(c.g * 255),
               static_cast<BYTE>(c.b * 255));  // Windows macro
}
#endif

enum class ThemeId { TokyoNight, TTY };

struct Theme {
    const char* id;
    const char* displayName;

    ColorRGBA background;
    ColorRGBA foreground;
    ColorRGBA selection;
    ColorRGBA secondary;

    std::array<ColorRGBA, 12> threadColors;
    std::array<ColorRGBA, kLogLevelCount> levelBadge;
    std::array<ColorRGBA, kLogLevelCount> messageColor;
    std::array<ColorRGBA, kLogLevelCount> rowBackground; // .a == 0 means no bg

    // Duration tiers: <100ms, <1s, <10s, >=10s
    std::array<ColorRGBA, 4> durationTiers;

    // SQL highlighter colors
    ColorRGBA sqlKeyword, sqlString, sqlNumber, sqlComment, sqlOperator, sqlBind, sqlType;
    // JSON highlighter colors
    ColorRGBA jsonKey, jsonString, jsonNumber, jsonBool, jsonBracket;
};

const Theme& GetTheme(ThemeId id);
const Theme& CurrentTheme();
void SetCurrentTheme(ThemeId id);
ThemeId CurrentThemeId();

ColorRGBA ThreadColor(int threadIdx);
ColorRGBA DurationColor(int64_t durationUS);

#ifdef _WIN32
// Owner-drawn dark-themed button helper. Call from WM_DRAWITEM handler.
void DrawThemedButton(const DRAWITEMSTRUCT* dis);

// Like DrawThemedButton but centres `icon` (pre-multiplied 32bpp BGRA HBITMAP)
// in place of button text. Background/border/state rendering matches.
void DrawThemedIconButton(const DRAWITEMSTRUCT* dis, HBITMAP icon, int iconSize);
#endif
