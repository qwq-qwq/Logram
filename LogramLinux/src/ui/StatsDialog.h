#pragma once

#include <gtk/gtk.h>

class LogDocument;

// Modal-ish window with a snapshot of log statistics: file metadata,
// duration, total events, HTTP/SQL/error totals, derived rates and a
// per-level breakdown. Mirrors macOS StatsView.swift layout.
class StatsDialog {
public:
    StatsDialog(GtkWindow* parent, LogDocument* doc);
    void Show();

private:
    GtkWindow* parent_ = nullptr;
    LogDocument* doc_ = nullptr;
    GtkWidget* window_ = nullptr;
};