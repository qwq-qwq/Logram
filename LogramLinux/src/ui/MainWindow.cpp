#include "ui/MainWindow.h"
#include "ui/LogTableView.h"
#include "ui/FilterSidebar.h"
#include "ui/DetailPanel.h"
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

} // namespace

MainWindow::MainWindow(GtkApplication* app) : app_(app) {
    window_ = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_), "Logram");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1280, 800);

    GtkWidget* header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window_), header);

    GtkWidget* openBtn = gtk_button_new_with_label("Open…");
    g_signal_connect(openBtn, "clicked", G_CALLBACK(OnOpenButtonClicked), this);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), openBtn);

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
        {"find",       OnFindAction,      nullptr, nullptr, nullptr, {0, 0, 0}},
        {"find-next",  OnFindNextAction,  nullptr, nullptr, nullptr, {0, 0, 0}},
        {"find-prev",  OnFindPrevAction,  nullptr, nullptr, nullptr, {0, 0, 0}},
        {"copy",       OnCopyAction,      nullptr, nullptr, nullptr, {0, 0, 0}},
        {"jump-pair",  OnJumpPairAction,  nullptr, nullptr, nullptr, {0, 0, 0}},
        {"focus-call", OnFocusCallAction, nullptr, nullptr, nullptr, {0, 0, 0}},
    };
    g_action_map_add_action_entries(G_ACTION_MAP(window_),
                                    kActions, G_N_ELEMENTS(kActions), this);

    const char* findAccels[]  = {"<Control>f",        nullptr};
    const char* nextAccels[]  = {"F3",                nullptr};
    const char* prevAccels[]  = {"<Shift>F3",         nullptr};
    const char* copyAccels[]  = {"<Control>c",        nullptr};
    const char* jumpAccels[]  = {"<Control>j",        nullptr};
    const char* focusAccels[] = {"<Control><Shift>e", nullptr};
    gtk_application_set_accels_for_action(app_, "win.find",       findAccels);
    gtk_application_set_accels_for_action(app_, "win.find-next",  nextAccels);
    gtk_application_set_accels_for_action(app_, "win.find-prev",  prevAccels);
    gtk_application_set_accels_for_action(app_, "win.copy",       copyAccels);
    gtk_application_set_accels_for_action(app_, "win.jump-pair",  jumpAccels);
    gtk_application_set_accels_for_action(app_, "win.focus-call", focusAccels);
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
    if (!doc_ || selectedLineId_ < 0 ||
        static_cast<size_t>(selectedLineId_) >= doc_->AllLines().size()) {
        gtk_widget_error_bell(window_);
        return;
    }
    const auto& line = doc_->AllLines()[selectedLineId_];
    const std::string_view raw = GetRawLine(doc_->MappedBase(), line);
    const std::string text(raw);
    GdkClipboard* clip = gtk_widget_get_clipboard(window_);
    gdk_clipboard_set_text(clip, text.c_str());
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