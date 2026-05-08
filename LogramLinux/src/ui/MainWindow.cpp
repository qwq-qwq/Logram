#include "ui/MainWindow.h"
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

    statusLabel_ = gtk_label_new("Откройте файл лога: кнопка Open…");
    gtk_widget_set_halign(statusLabel_, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(statusLabel_, GTK_ALIGN_CENTER);
    gtk_label_set_wrap(GTK_LABEL(statusLabel_), TRUE);
    gtk_window_set_child(GTK_WINDOW(window_), statusLabel_);
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

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
                  "%s\n%d lines · %zu threads · %d errors · %s",
                  utf8Path,
                  doc_->TotalEvents(),
                  doc_->ActiveThreads().size(),
                  doc_->ErrorCount(),
                  doc_->DurationFormatted().c_str());
    gtk_label_set_text(GTK_LABEL(statusLabel_), buf);
}