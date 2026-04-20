#include "ui/ThemeColors.h"

// --- Tokyo Night theme ---
static const Theme kTokyoNight = {
    "TokyoNight", "Tokyo Night",
    // background, foreground, selection, secondary
    MakeColor(0.10f, 0.11f, 0.17f),   // #1a1b2c
    MakeColor(0.66f, 0.69f, 0.84f),   // #a9b1d6
    MakeColorA(0.47f, 0.64f, 0.97f, 0.25f), // selection
    MakeColor(0.34f, 0.37f, 0.54f),   // #565f89

    // threadColors (12)
    {{
        MakeColor(0.97f, 0.33f, 0.33f), // systemRed
        MakeColor(0.20f, 0.48f, 1.00f), // systemBlue
        MakeColor(0.20f, 0.78f, 0.35f), // systemGreen
        MakeColor(1.00f, 0.62f, 0.04f), // systemOrange
        MakeColor(0.69f, 0.32f, 0.87f), // systemPurple
        MakeColor(0.35f, 0.78f, 0.80f), // systemTeal
        MakeColor(1.00f, 0.18f, 0.33f), // systemPink
        MakeColor(0.00f, 0.78f, 0.75f), // systemMint
        MakeColor(0.35f, 0.34f, 0.84f), // systemIndigo
        MakeColor(0.64f, 0.52f, 0.37f), // systemBrown
        MakeColor(0.39f, 0.82f, 1.00f), // systemCyan
        MakeColor(1.00f, 0.84f, 0.04f), // systemYellow
    }},

    // levelBadge[32]
    {{
        MakeColor(0.66f, 0.69f, 0.84f), // Unknown = default
        MakeColor(0.66f, 0.69f, 0.84f), // Info
        MakeColor(0.34f, 0.37f, 0.54f), // Debug
        MakeColor(0.34f, 0.37f, 0.54f), // Trace
        MakeColor(0.88f, 0.69f, 0.41f), // Warn
        MakeColor(0.97f, 0.46f, 0.56f), // Error  #f7768e
        MakeColor(0.62f, 0.81f, 0.42f), // Enter  #9ece6a
        MakeColor(0.62f, 0.81f, 0.42f), // Leave
        MakeColor(0.86f, 0.44f, 0.54f), // OsErr  #db7093
        MakeColor(0.97f, 0.46f, 0.56f), // Exc    #f7768e
        MakeColor(0.86f, 0.44f, 0.54f), // ExcOs
        MakeColor(0.66f, 0.69f, 0.84f), // Mem
        MakeColor(0.66f, 0.69f, 0.84f), // Stack
        MakeColor(0.86f, 0.44f, 0.54f), // Fail
        MakeColor(0.48f, 0.64f, 0.97f), // Sql    #7aa2f7
        MakeColor(0.66f, 0.69f, 0.84f), // Cache
        MakeColor(0.66f, 0.69f, 0.84f), // Res
        MakeColor(0.45f, 0.84f, 0.61f), // Db     #73daca
        MakeColor(0.49f, 0.81f, 1.00f), // Http   #7dcfff
        MakeColor(0.70f, 0.89f, 0.93f), // Clnt   #b4f9ec
        MakeColor(0.70f, 0.89f, 0.93f), // Srvr
        MakeColor(0.66f, 0.69f, 0.84f), // Call
        MakeColor(0.66f, 0.69f, 0.84f), // Ret
        MakeColor(0.73f, 0.60f, 0.97f), // Auth   #bb9af7
        MakeColor(0.48f, 0.64f, 0.97f), // Cust1 (same as SQL)
        MakeColor(0.88f, 0.69f, 0.41f), // Cust2 (same as Warn)
        MakeColor(0.66f, 0.69f, 0.84f), // Cust3
        MakeColor(0.66f, 0.69f, 0.84f), // Cust4
        MakeColor(0.66f, 0.69f, 0.84f), // Rotat
        MakeColor(0.86f, 0.44f, 0.54f), // DddER
        MakeColor(0.66f, 0.69f, 0.84f), // DddIN
        MakeColor(0.66f, 0.69f, 0.84f), // Mon
    }},

    // messageColor[32]
    {{
        MakeColor(0.66f, 0.69f, 0.84f), // Unknown
        MakeColor(0.66f, 0.69f, 0.84f), // Info    #a9b1d6
        MakeColor(0.34f, 0.37f, 0.54f), // Debug   #565f89
        MakeColor(0.34f, 0.37f, 0.54f), // Trace
        MakeColor(0.88f, 0.69f, 0.41f), // Warn    #e0af68
        MakeColor(0.97f, 0.46f, 0.56f), // Error   #f7768e
        MakeColor(0.62f, 0.81f, 0.42f), // Enter   #9ece6a
        MakeColor(0.62f, 0.81f, 0.42f), // Leave
        MakeColor(0.86f, 0.44f, 0.54f), // OsErr   #db7093
        MakeColor(0.97f, 0.46f, 0.56f), // Exc
        MakeColor(0.86f, 0.44f, 0.54f), // ExcOs
        MakeColor(0.66f, 0.69f, 0.84f), // Mem
        MakeColor(0.66f, 0.69f, 0.84f), // Stack
        MakeColor(0.86f, 0.44f, 0.54f), // Fail
        MakeColor(0.48f, 0.64f, 0.97f), // Sql     #7aa2f7
        MakeColor(0.66f, 0.69f, 0.84f), // Cache
        MakeColor(0.66f, 0.69f, 0.84f), // Res
        MakeColor(0.45f, 0.84f, 0.61f), // Db      #73daca
        MakeColor(0.49f, 0.81f, 1.00f), // Http    #7dcfff
        MakeColor(0.70f, 0.89f, 0.93f), // Clnt    #b4f9ec
        MakeColor(0.70f, 0.89f, 0.93f), // Srvr
        MakeColor(0.66f, 0.69f, 0.84f), // Call
        MakeColor(0.66f, 0.69f, 0.84f), // Ret
        MakeColor(0.73f, 0.60f, 0.97f), // Auth    #bb9af7
        MakeColor(0.48f, 0.64f, 0.97f), // Cust1
        MakeColor(0.88f, 0.69f, 0.41f), // Cust2
        MakeColor(0.66f, 0.69f, 0.84f), // Cust3
        MakeColor(0.66f, 0.69f, 0.84f), // Cust4
        MakeColor(0.66f, 0.69f, 0.84f), // Rotat
        MakeColor(0.86f, 0.44f, 0.54f), // DddER
        MakeColor(0.66f, 0.69f, 0.84f), // DddIN
        MakeColor(0.66f, 0.69f, 0.84f), // Mon
    }},

    // rowBackground[32] — alpha 0 = no bg
    {{
        {0, 0, 0, 0},                             // Unknown
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, // Info, Debug, Trace
        MakeColorA(0.88f, 0.69f, 0.41f, 0.05f),         // Warn
        MakeColorA(0.97f, 0.47f, 0.56f, 0.07f),         // Error
        {0, 0, 0, 0}, {0, 0, 0, 0},               // Enter, Leave
        MakeColorA(0.90f, 0.52f, 0.62f, 0.06f),         // OsErr
        MakeColorA(0.97f, 0.47f, 0.56f, 0.07f),         // Exc
        MakeColorA(0.90f, 0.52f, 0.62f, 0.06f),         // ExcOs
        {0, 0, 0, 0}, {0, 0, 0, 0},               // Mem, Stack
        MakeColorA(0.90f, 0.52f, 0.62f, 0.06f),         // Fail
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, // Sql..Db
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, // Http, Clnt, Srvr
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, // Call, Ret, Auth
        {0, 0, 0, 0},                              // Cust1
        MakeColorA(0.88f, 0.69f, 0.41f, 0.05f),         // Cust2
        {0, 0, 0, 0}, {0, 0, 0, 0},               // Cust3, Cust4
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, // Rotat..Mon
    }},

    // durationTiers: <100ms, <1s, <10s, >=10s
    {{
        MakeColor(0.34f, 0.37f, 0.54f), // secondary
        MakeColor(0.73f, 0.67f, 0.44f), // >=100ms
        MakeColor(0.88f, 0.69f, 0.41f), // >=1s
        MakeColor(0.97f, 0.47f, 0.56f), // >=10s
    }},

    // SQL highlighter
    MakeColor(0.20f, 0.48f, 1.00f), // keyword (systemBlue)
    MakeColor(0.20f, 0.78f, 0.35f), // string (systemGreen)
    MakeColor(1.00f, 0.62f, 0.04f), // number (systemOrange)
    MakeColor(0.50f, 0.50f, 0.50f), // comment (systemGray)
    MakeColor(0.69f, 0.32f, 0.87f), // operator (systemPurple)
    MakeColor(1.00f, 0.18f, 0.33f), // bind (systemPink)
    MakeColor(0.39f, 0.82f, 1.00f), // type (systemCyan)

    // JSON highlighter
    MakeColor(0.51f, 0.63f, 0.78f), // key (steel blue)
    MakeColor(0.51f, 0.67f, 0.51f), // string (sage green)
    MakeColor(0.82f, 0.68f, 0.39f), // number (warm amber)
    MakeColor(0.63f, 0.57f, 0.75f), // bool (muted lavender)
    MakeColor(0.50f, 0.50f, 0.50f), // bracket (secondaryLabel)
};

// --- TTY theme ---
static const Theme kTTY = {
    "TTY", "TTY",
    MakeColor(0.0f, 0.0f, 0.0f),      // background
    MakeColor(0.75f, 0.75f, 0.75f),   // foreground
    MakeColorA(0.30f, 0.30f, 0.80f, 0.35f), // selection
    MakeColor(0.50f, 0.50f, 0.50f),   // secondary

    // threadColors
    {{
        MakeColor(0.90f, 0.20f, 0.20f), MakeColor(0.40f, 0.50f, 1.00f),
        MakeColor(0.20f, 0.80f, 0.20f), MakeColor(0.80f, 0.80f, 0.00f),
        MakeColor(0.80f, 0.20f, 0.80f), MakeColor(0.20f, 0.80f, 0.80f),
        MakeColor(1.00f, 0.40f, 0.60f), MakeColor(0.20f, 0.90f, 0.60f),
        MakeColor(0.50f, 0.30f, 1.00f), MakeColor(0.70f, 0.50f, 0.30f),
        MakeColor(0.30f, 0.70f, 1.00f), MakeColor(1.00f, 1.00f, 0.30f),
    }},

    // levelBadge
    {{
        MakeColor(0.50f, 0.50f, 0.50f), // Unknown
        MakeColor(0.50f, 0.50f, 0.50f), // Info
        MakeColor(0.40f, 0.40f, 0.40f), // Debug
        MakeColor(0.40f, 0.40f, 0.40f), // Trace
        MakeColor(0.80f, 0.80f, 0.00f), // Warn
        MakeColor(0.80f, 0.00f, 0.00f), // Error
        MakeColor(0.00f, 0.60f, 0.00f), // Enter
        MakeColor(0.00f, 0.60f, 0.00f), // Leave
        MakeColor(0.80f, 0.00f, 0.00f), // OsErr
        MakeColor(0.80f, 0.00f, 0.00f), // Exc
        MakeColor(0.80f, 0.00f, 0.00f), // ExcOs
        MakeColor(0.50f, 0.50f, 0.50f), // Mem
        MakeColor(0.50f, 0.50f, 0.50f), // Stack
        MakeColor(0.80f, 0.00f, 0.00f), // Fail
        MakeColor(0.70f, 0.70f, 0.00f), // Sql
        MakeColor(0.50f, 0.50f, 0.50f), // Cache
        MakeColor(0.50f, 0.50f, 0.50f), // Res
        MakeColor(0.00f, 0.60f, 0.00f), // Db
        MakeColor(0.00f, 0.60f, 0.60f), // Http
        MakeColor(0.00f, 0.60f, 0.60f), // Clnt
        MakeColor(0.00f, 0.60f, 0.60f), // Srvr
        MakeColor(0.50f, 0.50f, 0.50f), // Call
        MakeColor(0.50f, 0.50f, 0.50f), // Ret
        MakeColor(0.60f, 0.00f, 0.60f), // Auth
        MakeColor(0.70f, 0.00f, 0.70f), // Cust1
        MakeColor(0.80f, 0.80f, 0.00f), // Cust2
        MakeColor(0.50f, 0.50f, 0.50f), // Cust3
        MakeColor(0.50f, 0.50f, 0.50f), // Cust4
        MakeColor(0.50f, 0.50f, 0.50f), // Rotat
        MakeColor(0.80f, 0.00f, 0.00f), // DddER
        MakeColor(0.50f, 0.50f, 0.50f), // DddIN
        MakeColor(0.50f, 0.50f, 0.50f), // Mon
    }},

    // messageColor
    {{
        MakeColor(0.75f, 0.75f, 0.75f), // Unknown
        MakeColor(0.85f, 0.85f, 0.85f), // Info
        MakeColor(0.50f, 0.50f, 0.50f), // Debug
        MakeColor(0.50f, 0.50f, 0.50f), // Trace
        MakeColor(1.00f, 1.00f, 0.30f), // Warn
        MakeColor(0.90f, 0.20f, 0.20f), // Error
        MakeColor(0.20f, 0.80f, 0.20f), // Enter
        MakeColor(0.20f, 0.80f, 0.20f), // Leave
        MakeColor(0.90f, 0.20f, 0.20f), // OsErr
        MakeColor(0.90f, 0.20f, 0.20f), // Exc
        MakeColor(0.90f, 0.20f, 0.20f), // ExcOs
        MakeColor(0.75f, 0.75f, 0.75f), // Mem
        MakeColor(0.75f, 0.75f, 0.75f), // Stack
        MakeColor(0.90f, 0.20f, 0.20f), // Fail
        MakeColor(0.80f, 0.80f, 0.00f), // Sql
        MakeColor(0.75f, 0.75f, 0.75f), // Cache
        MakeColor(0.75f, 0.75f, 0.75f), // Res
        MakeColor(0.60f, 0.80f, 0.20f), // Db
        MakeColor(0.20f, 0.80f, 0.80f), // Http
        MakeColor(0.20f, 0.80f, 0.80f), // Clnt
        MakeColor(0.20f, 0.80f, 0.80f), // Srvr
        MakeColor(0.75f, 0.75f, 0.75f), // Call
        MakeColor(0.75f, 0.75f, 0.75f), // Ret
        MakeColor(0.70f, 0.40f, 0.90f), // Auth
        MakeColor(0.80f, 0.20f, 0.80f), // Cust1
        MakeColor(1.00f, 1.00f, 0.30f), // Cust2
        MakeColor(0.75f, 0.75f, 0.75f), // Cust3
        MakeColor(0.75f, 0.75f, 0.75f), // Cust4
        MakeColor(0.75f, 0.75f, 0.75f), // Rotat
        MakeColor(0.90f, 0.20f, 0.20f), // DddER
        MakeColor(0.75f, 0.75f, 0.75f), // DddIN
        MakeColor(0.75f, 0.75f, 0.75f), // Mon
    }},

    // rowBackground
    {{
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        MakeColorA(0.80f, 0.80f, 0.00f, 0.06f), // Warn
        MakeColorA(0.90f, 0.00f, 0.00f, 0.10f), // Error
        {0, 0, 0, 0}, {0, 0, 0, 0},
        MakeColorA(0.90f, 0.00f, 0.00f, 0.10f), // OsErr
        MakeColorA(0.90f, 0.00f, 0.00f, 0.10f), // Exc
        MakeColorA(0.90f, 0.00f, 0.00f, 0.10f), // ExcOs
        {0, 0, 0, 0}, {0, 0, 0, 0},
        MakeColorA(0.90f, 0.00f, 0.00f, 0.10f), // Fail
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0},
        MakeColorA(0.80f, 0.80f, 0.00f, 0.06f), // Cust2
        {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
    }},

    // durationTiers
    {{
        MakeColor(0.20f, 0.80f, 0.20f), // <100ms
        MakeColor(0.80f, 0.80f, 0.00f), // <1s
        MakeColor(1.00f, 1.00f, 0.30f), // <10s
        MakeColor(0.90f, 0.20f, 0.20f), // >=10s
    }},

    // SQL
    MakeColor(0.40f, 0.50f, 1.00f), MakeColor(0.20f, 0.80f, 0.20f), MakeColor(0.80f, 0.80f, 0.00f),
    MakeColor(0.50f, 0.50f, 0.50f), MakeColor(0.80f, 0.20f, 0.80f), MakeColor(1.00f, 0.40f, 0.60f),
    MakeColor(0.20f, 0.80f, 0.80f),
    // JSON
    MakeColor(0.51f, 0.63f, 0.78f), MakeColor(0.51f, 0.67f, 0.51f), MakeColor(0.82f, 0.68f, 0.39f),
    MakeColor(0.63f, 0.57f, 0.75f), MakeColor(0.50f, 0.50f, 0.50f),
};

static ThemeId g_currentTheme = ThemeId::TokyoNight;

const Theme& GetTheme(ThemeId id) {
    return (id == ThemeId::TTY) ? kTTY : kTokyoNight;
}

const Theme& CurrentTheme() { return GetTheme(g_currentTheme); }
ThemeId CurrentThemeId() { return g_currentTheme; }
void SetCurrentTheme(ThemeId id) { g_currentTheme = id; }

ColorRGBA ThreadColor(int threadIdx) {
    auto& colors = CurrentTheme().threadColors;
    return colors[threadIdx % colors.size()];
}

ColorRGBA DurationColor(int64_t durationUS) {
    auto& tiers = CurrentTheme().durationTiers;
    if (durationUS >= 10'000'000) return tiers[3];
    if (durationUS >= 1'000'000)  return tiers[2];
    if (durationUS >= 100'000)    return tiers[1];
    return tiers[0];
}

#ifdef _WIN32
#include <windows.h>

void DrawThemedButton(const DRAWITEMSTRUCT* dis) {
    auto& theme = CurrentTheme();
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    // Background: slightly lighter than theme bg; pressed = even lighter
    float lift = 0.08f;
    if (dis->itemState & ODS_SELECTED) lift = 0.15f;

    ColorRGBA bg;
    bg.r = std::min(1.0f, theme.background.r + lift);
    bg.g = std::min(1.0f, theme.background.g + lift);
    bg.b = std::min(1.0f, theme.background.b + lift);
    bg.a = 1.0f;

    HBRUSH hBrush = CreateSolidBrush(ToCOLORREF(bg));
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);

    // Border: secondary color (subtle)
    HPEN hPen = CreatePen(PS_SOLID, 1, ToCOLORREF(theme.secondary));
    HPEN hOld = static_cast<HPEN>(SelectObject(hdc, hPen));
    HBRUSH hNull = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH hOldBr = static_cast<HBRUSH>(SelectObject(hdc, hNull));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 4, 4);
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOld);
    DeleteObject(hPen);

    // Text
    wchar_t text[64];
    int len = GetWindowTextW(dis->hwndItem, text, 64);
    SetBkMode(hdc, TRANSPARENT);

    // Checked state (for Params toggle) — use accent color
    bool checked = (dis->itemState & ODS_SELECTED) ||
                   (SendMessageW(dis->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED);
    ColorRGBA textColor = checked ? theme.levelBadge[static_cast<int>(LogLevel::Sql)]
                                  : theme.foreground;
    SetTextColor(hdc, ToCOLORREF(textColor));

    // Focus rect — subtle dotted inside
    if (dis->itemState & ODS_FOCUS) {
        RECT focus = rc;
        InflateRect(&focus, -2, -2);
        DrawFocusRect(hdc, &focus);
    }

    DrawTextW(hdc, text, len, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawThemedIconButton(const DRAWITEMSTRUCT* dis, HBITMAP icon, int iconSize) {
    auto& theme = CurrentTheme();
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    float lift = 0.08f;
    if (dis->itemState & ODS_SELECTED) lift = 0.15f;
    ColorRGBA bg;
    bg.r = std::min(1.0f, theme.background.r + lift);
    bg.g = std::min(1.0f, theme.background.g + lift);
    bg.b = std::min(1.0f, theme.background.b + lift);
    bg.a = 1.0f;
    HBRUSH hBrush = CreateSolidBrush(ToCOLORREF(bg));
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 1, ToCOLORREF(theme.secondary));
    HPEN hOld = static_cast<HPEN>(SelectObject(hdc, hPen));
    HBRUSH hNull = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HBRUSH hOldBr = static_cast<HBRUSH>(SelectObject(hdc, hNull));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 4, 4);
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOld);
    DeleteObject(hPen);

    if (icon && iconSize > 0) {
        HDC memDC = CreateCompatibleDC(hdc);
        HGDIOBJ oldBmp = SelectObject(memDC, icon);
        int rw = rc.right - rc.left;
        int rh = rc.bottom - rc.top;
        int x = rc.left + (rw - iconSize) / 2;
        int y = rc.top + (rh - iconSize) / 2;
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        AlphaBlend(hdc, x, y, iconSize, iconSize,
                   memDC, 0, 0, iconSize, iconSize, bf);
        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
    }

    if (dis->itemState & ODS_FOCUS) {
        RECT focus = rc;
        InflateRect(&focus, -2, -2);
        DrawFocusRect(hdc, &focus);
    }
}
#endif
