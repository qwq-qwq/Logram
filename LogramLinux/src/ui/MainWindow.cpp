#include "ui/MainWindow.h"
#include "ui/LogTableView.h"
#include "ui/FilterSidebar.h"
#include "core/LogDocument.h"

#include <cstdio>
#include <cstring>
#include <string>

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

void OnFileChooserResponse(GtkNativeDialog* native, int response, gpointer self) {
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile* file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(native));
        if (file) {
            char* path = g_file_get_path(file);
            if (path) {
                static_cast<MainWindow*>(self)->LoadFile(path);
                g_free(path);
            }
            g_object_unref(file);
        }
    }
    g_object_unref(native);
}

const char* BaseName(const char* path) {
    const char* slash = std::strrchr(path, '/');
    return slash ? slash + 1 : path;
}

} // namespace

MainWindow::MainWindow(GtkApplication* app) {
    window_ = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_), "Logram");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1280, 800);

    GtkWidget* header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window_), header);

    GtkWidget* openBtn = gtk_button_new_with_label("Open…");
    g_signal_connect(openBtn, "clicked", G_CALLBACK(OnOpenButtonClicked), this);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), openBtn);

    sidebar_ = std::make_unique<FilterSidebar>();
    sidebar_->SetOnChanged([this]{ OnFiltersChanged(); });

    table_ = std::make_unique<LogTableView>();

    GtkWidget* paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned), sidebar_->Widget());
    gtk_paned_set_end_child(GTK_PANED(paned), table_->Widget());
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
}

MainWindow::~MainWindow() = default;

void MainWindow::OnOpenClicked() {
    GtkFileChooserNative* native = gtk_file_chooser_native_new(
        "Open Log File",
        GTK_WINDOW(window_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Open",
        "_Cancel");
    g_signal_connect(native, "response", G_CALLBACK(OnFileChooserResponse), this);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
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

    char title[512];
    std::snprintf(title, sizeof(title), "Logram — %s", BaseName(utf8Path));
    gtk_window_set_title(GTK_WINDOW(window_), title);

    UpdateStatus();
}

void MainWindow::OnFiltersChanged() {
    if (!doc_) return;
    table_->Refresh();
    UpdateStatus();
}

void MainWindow::UpdateStatus() {
    if (!doc_) {
        gtk_label_set_text(GTK_LABEL(statusLabel_),
                           "Откройте файл лога: кнопка Open…");
        return;
    }
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
                  "%d / %d lines · %zu threads · %d errors · %s",
                  doc_->FilteredCount(),
                  doc_->TotalEvents(),
                  doc_->ActiveThreads().size(),
                  doc_->ErrorCount(),
                  doc_->DurationFormatted().c_str());
    gtk_label_set_text(GTK_LABEL(statusLabel_), buf);
}