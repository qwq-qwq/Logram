#pragma once

#include <gtk/gtk.h>
#include <memory>

class LogDocument;

class MainWindow {
public:
    explicit MainWindow(GtkApplication* app);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    GtkWidget* Widget() const { return window_; }

    void OnOpenClicked();
    void LoadFile(const char* utf8Path);

private:
    GtkWidget* window_ = nullptr;
    GtkWidget* statusLabel_ = nullptr;
    std::unique_ptr<LogDocument> doc_;
};