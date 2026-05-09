#include "ui/Theme.h"

#include <array>
#include <gtk/gtk.h>

namespace {

ThemeId g_currentTheme = ThemeId::TokyoNight;
GtkCssProvider* g_currentProvider = nullptr;

// ---------------- Tokyo Night CSS ----------------

const char kCssTokyoNight[] = R"CSS(
@define-color accent_color #7aa2f7;
@define-color accent_bg_color #7aa2f7;
@define-color accent_fg_color #16161e;
@define-color theme_selected_bg_color #283457;
@define-color theme_selected_fg_color #c0caf5;

window, .background {
    background-color: #1a1b26;
    color: #a9b1d6;
}

headerbar {
    background-color: #16161e;
    color: #a9b1d6;
    border-bottom: 1px solid #2a2e44;
    min-height: 36px;
    padding: 0 6px;
}

headerbar button {
    background: transparent;
    color: #a9b1d6;
    border: 1px solid transparent;
    padding: 4px 10px;
    border-radius: 4px;
}
headerbar button:hover {
    background-color: #1f2335;
    border-color: #2a2e44;
}
headerbar button:active {
    background-color: #283457;
}

headerbar button.error-nav image {
    color: #f7768e;
}
headerbar button.error-nav:hover image {
    color: #ff8fa3;
}

entry, searchentry {
    background-color: #1f2335;
    color: #c0caf5;
    border: 1px solid #2a2e44;
    border-radius: 4px;
    padding: 4px 6px;
    caret-color: #7aa2f7;
}
entry:focus-within, searchentry:focus-within {
    border-color: #7aa2f7;
    box-shadow: none;
}
entry placeholder, searchentry placeholder {
    color: #565f89;
}

paned > separator {
    background-color: #2a2e44;
    min-width: 1px;
    min-height: 1px;
}

scrolledwindow, scrollbar {
    background-color: transparent;
}
scrollbar slider {
    background-color: #2a2e44;
    min-width: 6px;
    min-height: 6px;
    border-radius: 3px;
    border: 2px solid transparent;
    background-clip: padding-box;
}
scrollbar slider:hover {
    background-color: #565f89;
}
scrollbar trough {
    background-color: transparent;
}

.sidebar, scrolledwindow > viewport {
    background-color: #16161e;
    color: #a9b1d6;
}
checkbutton {
    color: #a9b1d6;
    padding: 2px 4px;
}
checkbutton > check {
    background-color: #1f2335;
    border: 1px solid #2a2e44;
    border-radius: 3px;
    min-width: 14px;
    min-height: 14px;
}
checkbutton > check:checked,
checkbutton check:checked,
checkbutton:checked > check,
checkbutton:checked check {
    background-color: #3d4a72;
    border-color: #3d4a72;
    color: #a9b1d6;
}
checkbutton > check:checked:hover,
checkbutton:checked > check:hover {
    background-color: #4a5a8a;
    border-color: #4a5a8a;
}

columnview {
    background-color: #1a1b26;
    color: #a9b1d6;
}
columnview > listview > row {
    background-color: #1a1b26;
    color: #a9b1d6;
    padding: 0 4px;
    min-height: 22px;
}
columnview > listview > row:nth-child(even) {
    background-color: #1c1d2a;
}
columnview > listview > row:selected,
columnview > listview > row:focus:focus-visible {
    background-color: #283457;
    color: #c0caf5;
    outline: none;
}
columnview > header {
    background-color: #16161e;
    color: #565f89;
    border-bottom: 1px solid #2a2e44;
}
columnview > header > button {
    background: transparent;
    color: #565f89;
    border: none;
    border-right: 1px solid #2a2e44;
    border-radius: 0;
    padding: 4px 8px;
    font-weight: 500;
}
columnview > header > button:hover {
    background-color: #1f2335;
    color: #a9b1d6;
}

columnview > listview > row:not(:selected) label.row-warn {
    background-color: rgba(224, 175, 104, 0.05);
}
columnview > listview > row:not(:selected) label.row-error {
    background-color: rgba(247, 118, 142, 0.07);
}
columnview > listview > row:not(:selected) label.row-osErr {
    background-color: rgba(229, 132, 152, 0.06);
}
columnview > listview > row:not(:selected) label.row-exc {
    background-color: rgba(247, 118, 142, 0.07);
}
columnview > listview > row:not(:selected) label.row-excOs {
    background-color: rgba(229, 132, 152, 0.06);
}

textview, textview text {
    background-color: #1a1b26;
    color: #c0caf5;
    caret-color: transparent;
}
textview text selection {
    background-color: #283457;
    color: #c0caf5;
}

window > box > label {
    background-color: #16161e;
    color: #565f89;
    border-top: 1px solid #2a2e44;
    padding: 4px 8px;
}
)CSS";

// ---------------- TTY CSS ----------------
//
// Classic terminal look — black bg, light gray fg, neon accents. Saturation
// is left high here on purpose: TTY *is* the neon look.
const char kCssTty[] = R"CSS(
@define-color accent_color #00d7d7;
@define-color accent_bg_color #00d7d7;
@define-color accent_fg_color #000000;
@define-color theme_selected_bg_color #2a4a7a;
@define-color theme_selected_fg_color #ffffff;

window, .background {
    background-color: #000000;
    color: #c0c0c0;
}

headerbar {
    background-color: #050505;
    color: #c0c0c0;
    border-bottom: 1px solid #303030;
    min-height: 36px;
    padding: 0 6px;
}

headerbar button {
    background: transparent;
    color: #c0c0c0;
    border: 1px solid transparent;
    padding: 4px 10px;
    border-radius: 4px;
}
headerbar button:hover {
    background-color: #1a1a1a;
    border-color: #303030;
}
headerbar button:active {
    background-color: #2a4a7a;
}
headerbar button.error-nav image {
    color: #e63333;
}
headerbar button.error-nav:hover image {
    color: #ff5555;
}

entry, searchentry {
    background-color: #0a0a0a;
    color: #ffffff;
    border: 1px solid #303030;
    border-radius: 4px;
    padding: 4px 6px;
    caret-color: #00d7d7;
}
entry:focus-within, searchentry:focus-within {
    border-color: #00d7d7;
    box-shadow: none;
}
entry placeholder, searchentry placeholder {
    color: #808080;
}

paned > separator {
    background-color: #303030;
    min-width: 1px;
    min-height: 1px;
}

scrolledwindow, scrollbar {
    background-color: transparent;
}
scrollbar slider {
    background-color: #303030;
    min-width: 6px;
    min-height: 6px;
    border-radius: 3px;
    border: 2px solid transparent;
    background-clip: padding-box;
}
scrollbar slider:hover {
    background-color: #808080;
}
scrollbar trough {
    background-color: transparent;
}

.sidebar, scrolledwindow > viewport {
    background-color: #050505;
    color: #c0c0c0;
}
checkbutton {
    color: #c0c0c0;
    padding: 2px 4px;
}
checkbutton > check {
    background-color: #0a0a0a;
    border: 1px solid #303030;
    border-radius: 3px;
    min-width: 14px;
    min-height: 14px;
}
checkbutton > check:checked,
checkbutton check:checked,
checkbutton:checked > check,
checkbutton:checked check {
    background-color: #404040;
    border-color: #404040;
    color: #d0d0d0;
}
checkbutton > check:checked:hover,
checkbutton:checked > check:hover {
    background-color: #555555;
    border-color: #555555;
}

columnview {
    background-color: #000000;
    color: #c0c0c0;
}
columnview > listview > row {
    background-color: #000000;
    color: #c0c0c0;
    padding: 0 4px;
    min-height: 22px;
}
columnview > listview > row:nth-child(even) {
    background-color: #060606;
}
columnview > listview > row:selected,
columnview > listview > row:focus:focus-visible {
    background-color: #2a4a7a;
    color: #ffffff;
    outline: none;
}
columnview > header {
    background-color: #050505;
    color: #808080;
    border-bottom: 1px solid #303030;
}
columnview > header > button {
    background: transparent;
    color: #808080;
    border: none;
    border-right: 1px solid #303030;
    border-radius: 0;
    padding: 4px 8px;
    font-weight: 500;
}
columnview > header > button:hover {
    background-color: #1a1a1a;
    color: #c0c0c0;
}

columnview > listview > row:not(:selected) label.row-warn {
    background-color: rgba(255, 255, 64, 0.07);
}
columnview > listview > row:not(:selected) label.row-error {
    background-color: rgba(230, 51, 51, 0.10);
}
columnview > listview > row:not(:selected) label.row-osErr {
    background-color: rgba(230, 51, 51, 0.08);
}
columnview > listview > row:not(:selected) label.row-exc {
    background-color: rgba(230, 51, 51, 0.10);
}
columnview > listview > row:not(:selected) label.row-excOs {
    background-color: rgba(230, 51, 51, 0.08);
}

textview, textview text {
    background-color: #000000;
    color: #ffffff;
    caret-color: transparent;
}
textview text selection {
    background-color: #2a4a7a;
    color: #ffffff;
}

window > box > label {
    background-color: #050505;
    color: #808080;
    border-top: 1px solid #303030;
    padding: 4px 8px;
}
)CSS";

// ---------------- Per-theme palettes for text content ----------------

// Tokyo Night — matches LogramWin/src/ui/ThemeColors.cpp messageColor[].
constexpr std::array<const char*, kLogLevelCount> kLevelTokyoNight = {{
    "#a9b1d6", // Unknown
    "#a9b1d6", // Info
    "#565f89", // Debug
    "#565f89", // Trace
    "#e0af68", // Warn
    "#f7768e", // Error
    "#9ece6a", // Enter
    "#9ece6a", // Leave
    "#db7093", // OsErr
    "#f7768e", // Exc
    "#db7093", // ExcOs
    "#a9b1d6", // Mem
    "#a9b1d6", // Stack
    "#db7093", // Fail
    "#7aa2f7", // Sql
    "#a9b1d6", // Cache
    "#a9b1d6", // Res
    "#73daca", // Db
    "#7dcfff", // Http
    "#b4f9ec", // Clnt
    "#b4f9ec", // Srvr
    "#a9b1d6", // Call
    "#a9b1d6", // Ret
    "#bb9af7", // Auth
    "#7aa2f7", // Cust1
    "#e0af68", // Cust2
    "#a9b1d6", // Cust3
    "#a9b1d6", // Cust4
    "#a9b1d6", // Rotat
    "#db7093", // DddER
    "#a9b1d6", // DddIN
    "#a9b1d6", // Mon
}};

constexpr std::array<const char*, 12> kThreadTokyoNight = {{
    "#f75454", "#337aff", "#33c759", "#ff9d0a",
    "#b052de", "#59c7cc", "#ff2e54", "#00c7bf",
    "#5957d6", "#a3855e", "#63d2ff", "#ffd60a",
}};

// TTY — mirrors LogramWin kTTY messageColor[]. Saturated, bright on black.
constexpr std::array<const char*, kLogLevelCount> kLevelTty = {{
    "#c0c0c0", // Unknown
    "#d8d8d8", // Info
    "#808080", // Debug
    "#808080", // Trace
    "#ffff4d", // Warn
    "#e63333", // Error
    "#33cc33", // Enter
    "#33cc33", // Leave
    "#e63333", // OsErr
    "#e63333", // Exc
    "#e63333", // ExcOs
    "#c0c0c0", // Mem
    "#c0c0c0", // Stack
    "#e63333", // Fail
    "#cccc00", // Sql
    "#c0c0c0", // Cache
    "#c0c0c0", // Res
    "#99cc33", // Db
    "#33cccc", // Http
    "#33cccc", // Clnt
    "#33cccc", // Srvr
    "#c0c0c0", // Call
    "#c0c0c0", // Ret
    "#b366e6", // Auth
    "#cc33cc", // Cust1
    "#ffff4d", // Cust2
    "#c0c0c0", // Cust3
    "#c0c0c0", // Cust4
    "#c0c0c0", // Rotat
    "#e63333", // DddER
    "#c0c0c0", // DddIN
    "#c0c0c0", // Mon
}};

constexpr std::array<const char*, 12> kThreadTty = {{
    "#e63333", "#6680ff", "#33cc33", "#cccc00",
    "#cc33cc", "#33cccc", "#ff6699", "#33e699",
    "#804cff", "#b3804c", "#4cb3ff", "#ffff4d",
}};

const char* CurrentCss() {
    return g_currentTheme == ThemeId::TTY ? kCssTty : kCssTokyoNight;
}

} // namespace

void Theme::Apply() {
    // Force base theme to plain Adwaita so distro accent overrides
    // (Ubuntu Yaru, etc.) don't leak through into our checkbox/accent
    // styling. Our CSS then layers cleanly on top of Adwaita defaults.
    g_object_set(gtk_settings_get_default(),
                 "gtk-theme-name", "Adwaita",
                 "gtk-application-prefer-dark-theme", TRUE, nullptr);

    // Replace any previously applied provider so the new CSS overrides
    // (rather than layers on top of) the old one.
    GdkDisplay* display = gdk_display_get_default();
    if (g_currentProvider) {
        gtk_style_context_remove_provider_for_display(
            display, GTK_STYLE_PROVIDER(g_currentProvider));
        g_object_unref(g_currentProvider);
        g_currentProvider = nullptr;
    }
    g_currentProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(g_currentProvider, CurrentCss());
    // PRIORITY_USER+1 beats Yaru's accent override on Ubuntu, which itself
    // installs at USER priority via the system style settings.
    gtk_style_context_add_provider_for_display(
        display,
        GTK_STYLE_PROVIDER(g_currentProvider),
        GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
}

void Theme::SetCurrent(ThemeId id) { g_currentTheme = id; }
ThemeId Theme::Current() { return g_currentTheme; }

const char* LevelHexColor(LogLevel level) {
    const auto& table = (g_currentTheme == ThemeId::TTY)
                            ? kLevelTty : kLevelTokyoNight;
    const int idx = static_cast<int>(level);
    if (idx < 0 || idx >= kLogLevelCount) return table[0];
    return table[idx];
}

const char* ThreadHexColor(int threadIdx) {
    const auto& table = (g_currentTheme == ThemeId::TTY)
                            ? kThreadTty : kThreadTokyoNight;
    if (threadIdx < 0) return table[0];
    return table[threadIdx % table.size()];
}