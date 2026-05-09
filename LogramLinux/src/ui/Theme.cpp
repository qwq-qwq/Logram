#include "ui/Theme.h"

#include <gtk/gtk.h>

namespace {

// Tokyo Night palette (matches LogramWin/src/ui/ThemeColors.cpp):
//   bg          #1a1b26
//   bg-elev     #1f2335   (slightly lighter — sidebar / alt rows)
//   bg-deep     #16161e   (darker — header / column headers)
//   fg          #a9b1d6
//   fg-dim      #565f89
//   accent      #7aa2f7
//   border      #2a2e44
//   selection   #283457
//
// First iteration covers chrome only (window, headerbar, sidebar, table,
// scrollbars, paned, entries, textview, status). Per-level cell colors come
// next via Pango markup in the column factories.
const char kCss[] = R"CSS(
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

/* Sidebar */
.sidebar, scrolledwindow > viewport {
    background-color: #16161e;
    color: #a9b1d6;
}
checkbutton {
    color: #a9b1d6;
    padding: 2px 4px;
}
checkbutton check {
    background-color: #1f2335;
    border: 1px solid #2a2e44;
    border-radius: 3px;
    min-width: 14px;
    min-height: 14px;
}
checkbutton:checked check {
    background-color: #7aa2f7;
    border-color: #7aa2f7;
    color: #16161e;
}

/* Column view (the log table) */
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

/* Detail panel (read-only TextView) */
textview, textview text {
    background-color: #1a1b26;
    color: #c0caf5;
    caret-color: transparent;
}
textview text selection {
    background-color: #283457;
    color: #c0caf5;
}

/* Status bar at the bottom */
window > box > label {
    background-color: #16161e;
    color: #565f89;
    border-top: 1px solid #2a2e44;
    padding: 4px 8px;
}
)CSS";

} // namespace

void Theme::Apply() {
    // Force GTK to its dark variant so widgets we don't override stay coherent.
    g_object_set(gtk_settings_get_default(),
                 "gtk-application-prefer-dark-theme", TRUE, nullptr);

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, kCss);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}