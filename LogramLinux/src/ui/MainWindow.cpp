#include "ui/MainWindow.h"
#include "ui/LogTableView.h"
#include "ui/FilterSidebar.h"
#include "ui/DetailPanel.h"
#include "ui/TimingDialog.h"
#include "ui/Theme.h"
#include "core/LogDocument.h"
#include "core/LogLine.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace {

// Naive UTF-8 → wstring: each byte becomes a wchar_t. The POSIX MappedFile
// implementation casts wchar_t → char back the same way, so ASCII paths
// round-trip correctly. Non-ASCII paths will fail to open until we add a
// proper char* path API across LogDocument/MappedFile.
std::wstring Utf8ToWide(const char* s) {
    std::wstring out;
    if (!s) return out;
    out.reserve(std::strlen(s));
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
        out.push_back(static_cast<wchar_t>(*p));
    return out;
}

void OnOpenButtonClicked(GtkButton* /*btn*/, gpointer self) {
    static_cast<MainWindow*>(self)->OnOpenClicked();
}

void OnFileDialogOpenFinished(GObject* source, GAsyncResult* result,
                              gpointer user_data) {
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source),
                                              result, &error);
    if (file) {
        char* path = g_file_get_path(file);
        if (path) {
            static_cast<MainWindow*>(user_data)->LoadFile(path);
            g_free(path);
        }
        g_object_unref(file);
    } else if (error) {
        // Dismissed by the user or other error — nothing to do.
        g_clear_error(&error);
    }
}

const char* BaseName(const char* path) {
    const char* slash = std::strrchr(path, '/');
    return slash ? slash + 1 : path;
}

void OnSearchActivate(GtkSearchEntry* /*entry*/, gpointer self) {
    static_cast<MainWindow*>(self)->SearchNext();
}

void OnSearchChanged(GtkSearchEntry* /*entry*/, gpointer self) {
    static_cast<MainWindow*>(self)->OnSearchChanged();
}

void OnToggleParamsAction(GSimpleAction* action, GVariant* /*param*/,
                          gpointer self) {
    GVariant* state = g_action_get_state(G_ACTION(action));
    const gboolean was = g_variant_get_boolean(state);
    g_variant_unref(state);
    const gboolean now = !was;
    g_simple_action_set_state(action, g_variant_new_boolean(now));
    static_cast<MainWindow*>(self)->SetParamsEnabled(now);
}

void OnFindAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->FocusSearch();
}
void OnFindNextAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->SearchNext();
}
void OnFindPrevAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->SearchPrev();
}
void OnCopyAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->CopySelectedLine();
}
void OnJumpPairAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->JumpToPair();
}
void OnFocusCallAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->ToggleFocusOnCall();
}
void OnCancelFocusAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->CancelFocus();
}
void OnNextErrorAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->GotoError(true);
}
void OnPrevErrorAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->GotoError(false);
}
void OnToggleDurationAction(GSimpleAction* action, GVariant* /*param*/,
                            gpointer self) {
    GVariant* state = g_action_get_state(G_ACTION(action));
    const gboolean was = g_variant_get_boolean(state);
    g_variant_unref(state);
    const gboolean now = !was;
    g_simple_action_set_state(action, g_variant_new_boolean(now));
    static_cast<MainWindow*>(self)->ToggleDuration(now);
}
void OnMethodTimingAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->ShowMethodTiming();
}
void OnOpenAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->OnOpenClicked();
}
void OnThemeChange(GSimpleAction* action, GVariant* value, gpointer self) {
    g_simple_action_set_state(action, value);
    const char* id = g_variant_get_string(value, nullptr);
    static_cast<MainWindow*>(self)->SwitchTheme(id);
}
void OnAboutAction(GSimpleAction*, GVariant*, gpointer self) {
    static_cast<MainWindow*>(self)->ShowAbout();
}

} // namespace

MainWindow::MainWindow(GtkApplication* app) : app_(app) {
    window_ = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_), "Logram");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1280, 800);
    // Resolved through the icon-theme search path that GtkApplication
    // automatically registers from /com/unitybase/Logram/icons in our
    // bundled GResource (logram.gresource.xml).
    gtk_window_set_icon_name(GTK_WINDOW(window_), "com.unitybase.Logram");

    GtkWidget* header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window_), header);

    GtkWidget* openBtn = gtk_button_new_with_label("Open…");
    g_signal_connect(openBtn, "clicked", G_CALLBACK(OnOpenButtonClicked), this);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), openBtn);

    // Hamburger menu — sits next to the Open button.
    GMenu* mainMenu = g_menu_new();
    GMenu* fileSec = g_menu_new();
    g_menu_append(fileSec, "Open File…", "win.open");
    g_menu_append_section(mainMenu, nullptr, G_MENU_MODEL(fileSec));
    g_object_unref(fileSec);
    GMenu* viewSec = g_menu_new();
    g_menu_append(viewSec, "Show Duration",            "win.toggle-duration");
    g_menu_append(viewSec, "Substitute SQL Parameters", "win.toggle-params");
    g_menu_append(viewSec, "Method Timing…",            "win.method-timing");
    g_menu_append_section(mainMenu, nullptr, G_MENU_MODEL(viewSec));
    g_object_unref(viewSec);

    // Theme submenu — radio items via stateful string action.
    GMenu* themeSub = g_menu_new();
    GMenuItem* tnItem = g_menu_item_new("Tokyo Night", nullptr);
    g_menu_item_set_action_and_target_value(tnItem, "win.theme",
        g_variant_new_string("tokyo-night"));
    g_menu_append_item(themeSub, tnItem);
    g_object_unref(tnItem);
    GMenuItem* ttyItem = g_menu_item_new("TTY", nullptr);
    g_menu_item_set_action_and_target_value(ttyItem, "win.theme",
        g_variant_new_string("tty"));
    g_menu_append_item(themeSub, ttyItem);
    g_object_unref(ttyItem);
    GMenu* themeSec = g_menu_new();
    g_menu_append_submenu(themeSec, "Theme", G_MENU_MODEL(themeSub));
    g_menu_append_section(mainMenu, nullptr, G_MENU_MODEL(themeSec));
    g_object_unref(themeSub);
    g_object_unref(themeSec);

    // About section.
    GMenu* aboutSec = g_menu_new();
    g_menu_append(aboutSec, "About Logram", "win.about");
    g_menu_append_section(mainMenu, nullptr, G_MENU_MODEL(aboutSec));
    g_object_unref(aboutSec);

    GtkWidget* menuBtn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menuBtn),
                                  "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menuBtn),
                                   G_MENU_MODEL(mainMenu));
    gtk_widget_set_tooltip_text(menuBtn, "Menu");
    g_object_unref(mainMenu);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), menuBtn);

    // pack_end accumulates right-to-left; first call → rightmost.
    // Final order (left → right):
    //   [Search field] [↑search] [↓search] [↑error] [↓error]
    auto makeNavBtn = [&](const char* icon, const char* tooltip,
                          const char* action, const char* extraClass) {
        GtkWidget* b = gtk_button_new_from_icon_name(icon);
        gtk_widget_add_css_class(b, "flat");
        if (extraClass) gtk_widget_add_css_class(b, extraClass);
        gtk_widget_set_tooltip_text(b, tooltip);
        gtk_actionable_set_action_name(GTK_ACTIONABLE(b), action);
        gtk_header_bar_pack_end(GTK_HEADER_BAR(header), b);
    };
    // Error nav (rendered red via CSS class .error-nav).
    makeNavBtn("go-down-symbolic", "Next error (Ctrl+Shift+Down)",
               "win.next-error", "error-nav");
    makeNavBtn("go-up-symbolic",   "Previous error (Ctrl+Shift+Up)",
               "win.prev-error", "error-nav");
    // Search nav.
    makeNavBtn("go-down-symbolic", "Next match (F3)",
               "win.find-next", nullptr);
    makeNavBtn("go-up-symbolic",   "Previous match (Shift+F3)",
               "win.find-prev", nullptr);

    searchEntry_ = gtk_search_entry_new();
    gtk_widget_set_size_request(searchEntry_, 280, -1);
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(searchEntry_),
                                          "Search… (Ctrl+F)");
    g_signal_connect(searchEntry_, "activate",
                     G_CALLBACK(OnSearchActivate), this);
    g_signal_connect(searchEntry_, "search-changed",
                     G_CALLBACK(::OnSearchChanged), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), searchEntry_);


    sidebar_ = std::make_unique<FilterSidebar>();
    sidebar_->SetOnChanged([this]{ OnFiltersChanged(); });

    table_ = std::make_unique<LogTableView>();
    table_->SetOnSelectionChanged([this](int lineId){ OnRowSelected(lineId); });

    detail_ = std::make_unique<DetailPanel>();

    GtkWidget* rightPaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_start_child(GTK_PANED(rightPaned), table_->Widget());
    gtk_paned_set_end_child(GTK_PANED(rightPaned), detail_->Widget());
    gtk_paned_set_position(GTK_PANED(rightPaned), 520);
    gtk_paned_set_resize_end_child(GTK_PANED(rightPaned), FALSE);

    GtkWidget* paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned), sidebar_->Widget());
    gtk_paned_set_end_child(GTK_PANED(paned), rightPaned);
    gtk_paned_set_position(GTK_PANED(paned), 240);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(vbox), paned);

    statusLabel_ = gtk_label_new("Откройте файл лога: кнопка Open…");
    gtk_widget_set_halign(statusLabel_, GTK_ALIGN_START);
    gtk_widget_set_margin_start(statusLabel_, 8);
    gtk_widget_set_margin_end(statusLabel_, 8);
    gtk_widget_set_margin_top(statusLabel_, 4);
    gtk_widget_set_margin_bottom(statusLabel_, 4);
    gtk_label_set_ellipsize(GTK_LABEL(statusLabel_), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(vbox), statusLabel_);

    gtk_window_set_child(GTK_WINDOW(window_), vbox);

    InstallActions();
}

MainWindow::~MainWindow() = default;

void MainWindow::InstallActions() {
    static const GActionEntry kActions[] = {
        {"open",          OnOpenAction,         nullptr, nullptr, nullptr, {0, 0, 0}},
        {"find",          OnFindAction,         nullptr, nullptr, nullptr, {0, 0, 0}},
        {"find-next",     OnFindNextAction,     nullptr, nullptr, nullptr, {0, 0, 0}},
        {"find-prev",     OnFindPrevAction,     nullptr, nullptr, nullptr, {0, 0, 0}},
        {"copy",          OnCopyAction,         nullptr, nullptr, nullptr, {0, 0, 0}},
        {"jump-pair",     OnJumpPairAction,     nullptr, nullptr, nullptr, {0, 0, 0}},
        {"focus-call",    OnFocusCallAction,    nullptr, nullptr, nullptr, {0, 0, 0}},
        {"cancel-focus",  OnCancelFocusAction,  nullptr, nullptr, nullptr, {0, 0, 0}},
        {"next-error",    OnNextErrorAction,    nullptr, nullptr, nullptr, {0, 0, 0}},
        {"prev-error",    OnPrevErrorAction,    nullptr, nullptr, nullptr, {0, 0, 0}},
        {"method-timing", OnMethodTimingAction, nullptr, nullptr, nullptr, {0, 0, 0}},
        {"about",         OnAboutAction,        nullptr, nullptr, nullptr, {0, 0, 0}},
    };
    g_action_map_add_action_entries(G_ACTION_MAP(window_),
                                    kActions, G_N_ELEMENTS(kActions), this);

    // Stateful boolean actions — bound to menu items via gtk's automatic
    // checkmark rendering.
    GSimpleAction* durAction = g_simple_action_new_stateful(
        "toggle-duration", nullptr, g_variant_new_boolean(FALSE));
    g_signal_connect(durAction, "activate",
                     G_CALLBACK(OnToggleDurationAction), this);
    g_action_map_add_action(G_ACTION_MAP(window_), G_ACTION(durAction));
    g_object_unref(durAction);

    GSimpleAction* paramsAction = g_simple_action_new_stateful(
        "toggle-params", nullptr, g_variant_new_boolean(TRUE));
    g_signal_connect(paramsAction, "activate",
                     G_CALLBACK(OnToggleParamsAction), this);
    g_action_map_add_action(G_ACTION_MAP(window_), G_ACTION(paramsAction));
    g_object_unref(paramsAction);

    // Theme radio: stateful string action; menu radio items pick one value.
    const char* initialTheme =
        Theme::Current() == ThemeId::TTY ? "tty" : "tokyo-night";
    GSimpleAction* themeAction = g_simple_action_new_stateful(
        "theme", G_VARIANT_TYPE_STRING, g_variant_new_string(initialTheme));
    g_signal_connect(themeAction, "change-state",
                     G_CALLBACK(OnThemeChange), this);
    g_action_map_add_action(G_ACTION_MAP(window_), G_ACTION(themeAction));
    g_object_unref(themeAction);

    const char* openAccels[]    = {"<Control>o",          nullptr};
    const char* findAccels[]    = {"<Control>f",          nullptr};
    const char* nextAccels[]    = {"F3",                  nullptr};
    const char* prevAccels[]    = {"<Shift>F3",           nullptr};
    const char* copyAccels[]    = {"<Control>c",          nullptr};
    const char* jumpAccels[]    = {"<Control>j",          nullptr};
    const char* focusAccels[]   = {"<Control><Shift>e",   nullptr};
    const char* cancelFocusAccels[] = {"Escape",          nullptr};
    const char* nextErrAccels[] = {"<Control><Shift>Down", nullptr};
    const char* prevErrAccels[] = {"<Control><Shift>Up",   nullptr};
    const char* durAccels[]     = {"<Control>d",          nullptr};
    const char* paramsAccels[]  = {"<Control>p",          nullptr};
    const char* timingAccels[]  = {"<Control>m",          nullptr};
    gtk_application_set_accels_for_action(app_, "win.open",            openAccels);
    gtk_application_set_accels_for_action(app_, "win.find",            findAccels);
    gtk_application_set_accels_for_action(app_, "win.find-next",       nextAccels);
    gtk_application_set_accels_for_action(app_, "win.find-prev",       prevAccels);
    gtk_application_set_accels_for_action(app_, "win.copy",            copyAccels);
    gtk_application_set_accels_for_action(app_, "win.jump-pair",       jumpAccels);
    gtk_application_set_accels_for_action(app_, "win.focus-call",      focusAccels);
    gtk_application_set_accels_for_action(app_, "win.cancel-focus",    cancelFocusAccels);
    gtk_application_set_accels_for_action(app_, "win.next-error",      nextErrAccels);
    gtk_application_set_accels_for_action(app_, "win.prev-error",      prevErrAccels);
    gtk_application_set_accels_for_action(app_, "win.toggle-duration", durAccels);
    gtk_application_set_accels_for_action(app_, "win.toggle-params",   paramsAccels);
    gtk_application_set_accels_for_action(app_, "win.method-timing",   timingAccels);
}

void MainWindow::OnOpenClicked() {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Log File");
    gtk_file_dialog_open(dialog, GTK_WINDOW(window_), /*cancellable=*/nullptr,
                         OnFileDialogOpenFinished, this);
    g_object_unref(dialog);
}

void MainWindow::LoadFile(const char* utf8Path) {
    auto doc = std::make_unique<LogDocument>();
    const std::wstring wpath = Utf8ToWide(utf8Path);
    if (!doc->Load(wpath.c_str())) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "Failed to open: %s", utf8Path);
        gtk_label_set_text(GTK_LABEL(statusLabel_), buf);
        return;
    }
    doc_ = std::move(doc);
    timingsBuilt_ = false;
    sidebar_->SetDocument(doc_.get());
    table_->SetDocument(doc_.get());
    detail_->Clear();
    ResetSearch();

    char title[512];
    std::snprintf(title, sizeof(title), "Logram — %s", BaseName(utf8Path));
    gtk_window_set_title(GTK_WINDOW(window_), title);

    UpdateStatus();
}

void MainWindow::OnFiltersChanged() {
    if (!doc_) return;
    table_->Refresh();
    detail_->Clear();
    ResetSearch();
    UpdateStatus();
}

void MainWindow::OnRowSelected(int lineId) {
    selectedLineId_ = lineId;
    detail_->SetLine(doc_.get(), lineId);
}

void MainWindow::CopySelectedLine() {
    if (!doc_) {
        gtk_widget_error_bell(window_);
        return;
    }
    auto ids = table_->SelectedLineIds();
    if (ids.empty() && selectedLineId_ >= 0) {
        ids.push_back(selectedLineId_);
    }
    if (ids.empty()) {
        gtk_widget_error_bell(window_);
        return;
    }
    const auto& lines = doc_->AllLines();
    std::string out;
    out.reserve(ids.size() * 128);
    for (size_t i = 0; i < ids.size(); ++i) {
        const int id = ids[i];
        if (id < 0 || static_cast<size_t>(id) >= lines.size()) continue;
        const std::string_view raw = GetRawLine(doc_->MappedBase(), lines[id]);
        out.append(raw.data(), raw.size());
        if (i + 1 < ids.size()) out.push_back('\n');
    }
    GdkClipboard* clip = gtk_widget_get_clipboard(window_);
    gdk_clipboard_set_text(clip, out.c_str());
}

void MainWindow::ToggleFocusOnCall() {
    if (!doc_) return;
    if (doc_->FocusActive()) {
        doc_->ClearFocus();
    } else {
        if (selectedLineId_ < 0 || !doc_->FocusOnCall(selectedLineId_)) {
            gtk_widget_error_bell(window_);
            return;
        }
    }
    doc_->ApplyFilters();
    sidebar_->Refresh();
    table_->Refresh();
    detail_->Clear();
    ResetSearch();
    UpdateStatus();
}

void MainWindow::CancelFocus() {
    if (!doc_ || !doc_->FocusActive()) {
        gtk_widget_error_bell(window_);
        return;
    }
    doc_->ClearFocus();
    doc_->ApplyFilters();
    sidebar_->Refresh();
    table_->Refresh();
    detail_->Clear();
    ResetSearch();
    UpdateStatus();
}

void MainWindow::GotoError(bool forward) {
    if (!doc_) return;
    const auto& filtered = doc_->FilteredIndices();
    const auto& lines = doc_->AllLines();
    if (filtered.empty()) {
        gtk_widget_error_bell(window_);
        return;
    }

    // Map current selection (lineId in AllLines) to a position in filtered.
    int curPos = -1;
    if (selectedLineId_ >= 0) {
        for (size_t i = 0; i < filtered.size(); ++i) {
            if (static_cast<int>(filtered[i]) == selectedLineId_) {
                curPos = static_cast<int>(i);
                break;
            }
        }
    }

    auto isErr = [&](size_t pos) {
        const auto& line = lines[filtered[pos]];
        return GetLogLevelInfo(static_cast<LogLevel>(line.level)).isError;
    };

    const int n = static_cast<int>(filtered.size());
    int found = -1;
    bool wrapped = false;

    if (forward) {
        for (int i = curPos + 1; i < n; ++i) {
            if (isErr(i)) { found = i; break; }
        }
        if (found < 0) {
            for (int i = 0; i <= curPos && i < n; ++i) {
                if (isErr(i)) { found = i; wrapped = true; break; }
            }
        }
    } else {
        for (int i = curPos - 1; i >= 0; --i) {
            if (isErr(i)) { found = i; break; }
        }
        if (found < 0) {
            for (int i = n - 1; i > curPos; --i) {
                if (isErr(i)) { found = i; wrapped = true; break; }
            }
        }
    }

    if (found < 0) {
        gtk_widget_error_bell(window_);
        return;
    }
    if (wrapped) gtk_widget_error_bell(window_);
    table_->ScrollToPosition(static_cast<unsigned>(found));
}

void MainWindow::JumpToPair() {
    if (!doc_ || selectedLineId_ < 0) {
        gtk_widget_error_bell(window_);
        return;
    }
    const int pairLineId = doc_->FindMatchingPair(selectedLineId_);
    if (pairLineId < 0) {
        gtk_widget_error_bell(window_);
        return;
    }
    const auto& filtered = doc_->FilteredIndices();
    for (size_t i = 0; i < filtered.size(); ++i) {
        if (static_cast<int>(filtered[i]) == pairLineId) {
            table_->ScrollToPosition(static_cast<unsigned>(i));
            return;
        }
    }
    // Pair exists but is filtered out.
    gtk_widget_error_bell(window_);
}

void MainWindow::FocusSearch() {
    gtk_widget_grab_focus(searchEntry_);
}

void MainWindow::SetParamsEnabled(bool enabled) {
    detail_->SetParamsEnabled(enabled);
}

void MainWindow::ShowAbout() {
    GtkAboutDialog* dlg = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
    gtk_about_dialog_set_program_name(dlg, "Logram");
    gtk_about_dialog_set_version(dlg, "1.2");
    gtk_about_dialog_set_comments(dlg, "UnityBase log file analyzer");
    gtk_about_dialog_set_website(dlg, "https://logram.perek.rest");
    gtk_about_dialog_set_website_label(dlg, "logram.perek.rest");
    gtk_about_dialog_set_logo_icon_name(dlg, "com.unitybase.Logram");
    gtk_about_dialog_set_license_type(dlg, GTK_LICENSE_MIT_X11);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(window_));
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_present(GTK_WINDOW(dlg));
}

void MainWindow::SwitchTheme(const char* themeId) {
    const ThemeId id =
        (themeId && std::strcmp(themeId, "tty") == 0) ? ThemeId::TTY
                                                      : ThemeId::TokyoNight;
    Theme::SetCurrent(id);
    Theme::Apply();
    // Pango markup in the table/sidebar/detail-header captures theme colors
    // at format time, so each consumer needs a refresh to pick up new hex.
    if (sidebar_) sidebar_->Refresh();
    if (table_)   table_->Refresh();
    if (doc_ && selectedLineId_ >= 0 && detail_) {
        detail_->SetLine(doc_.get(), selectedLineId_);
    }
}

void MainWindow::ToggleDuration(bool visible) {
    if (!doc_) return;
    if (visible && !timingsBuilt_) {
        // Method-timing pairs feed the per-line durations of the matching
        // `+`/`-` lines. Build once on first toggle, then reuse.
        doc_->BuildMethodTimings();
        timingsBuilt_ = true;
    }
    table_->SetDurationVisible(visible);
    table_->Refresh();
}

void MainWindow::ShowMethodTiming() {
    if (!doc_) {
        gtk_widget_error_bell(window_);
        return;
    }
    if (!timingsBuilt_) {
        doc_->BuildMethodTimings();
        timingsBuilt_ = true;
    }
    auto* dlg = new TimingDialog(GTK_WINDOW(window_), doc_.get(),
        [this](int lineId) { GoToLineId(lineId); });
    dlg->Show(); // dialog deletes itself on close-request.
}

void MainWindow::GoToLineId(int lineId) {
    if (!doc_ || lineId < 0) return;
    const auto& filtered = doc_->FilteredIndices();
    for (size_t i = 0; i < filtered.size(); ++i) {
        if (static_cast<int>(filtered[i]) == lineId) {
            table_->ScrollToPosition(static_cast<unsigned>(i));
            return;
        }
    }
    // The target line is filtered out; don't silently fail.
    gtk_widget_error_bell(window_);
}

void MainWindow::SearchNext() { DoSearch(true); }
void MainWindow::SearchPrev() { DoSearch(false); }

void MainWindow::OnSearchChanged() {
    // Restart search position so the next search starts from the top.
    currentMatchPos_ = -1;
}

void MainWindow::ResetSearch() {
    currentMatchPos_ = -1;
}

void MainWindow::DoSearch(bool forward) {
    if (!doc_) return;
    const char* text = gtk_editable_get_text(GTK_EDITABLE(searchEntry_));
    if (!text || !*text) return;
    const std::string pattern = text;
    using Dir = LogDocument::SearchDirection;
    const Dir dir = forward ? Dir::Forward : Dir::Backward;

    int result = doc_->FindNext(pattern, dir, currentMatchPos_);
    bool wrapped = false;
    if (result < 0) {
        const int from = forward
            ? -1
            : static_cast<int>(doc_->FilteredCount());
        result = doc_->FindNext(pattern, dir, from);
        if (result < 0) {
            gtk_widget_error_bell(window_);
            return;
        }
        wrapped = true;
    }
    if (wrapped) gtk_widget_error_bell(window_);
    currentMatchPos_ = result;
    table_->ScrollToPosition(static_cast<unsigned>(result));
}

void MainWindow::UpdateStatus() {
    if (!doc_) {
        gtk_label_set_text(GTK_LABEL(statusLabel_),
                           "Откройте файл лога: кнопка Open…");
        return;
    }
    char buf[1024];
    if (doc_->FocusActive()) {
        std::snprintf(buf, sizeof(buf),
                      "[Focused on thread %d, lines %d–%d]   %d / %d lines",
                      doc_->FocusThread(),
                      doc_->FocusStart() + 1,
                      doc_->FocusEnd() + 1,
                      doc_->FilteredCount(),
                      doc_->TotalEvents());
    } else {
        std::snprintf(buf, sizeof(buf),
                      "%d / %d lines · %zu threads · %d errors · %s",
                      doc_->FilteredCount(),
                      doc_->TotalEvents(),
                      doc_->ActiveThreads().size(),
                      doc_->ErrorCount(),
                      doc_->DurationFormatted().c_str());
    }
    gtk_label_set_text(GTK_LABEL(statusLabel_), buf);
}