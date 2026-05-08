#pragma once

#include <gtk/gtk.h>

class LogDocument;

// Wraps a GtkColumnView showing filtered lines from a LogDocument.
// Backed by a custom GListModel that reads directly from LogDocument's
// FilteredIndices()/AllLines() — no per-row copying.
class LogTableView {
public:
    LogTableView();
    ~LogTableView();

    LogTableView(const LogTableView&) = delete;
    LogTableView& operator=(const LogTableView&) = delete;

    GtkWidget* Widget() const { return scroller_; }

    void SetDocument(LogDocument* doc);
    // Call after the underlying document data changes (filters, reload).
    void Refresh();

private:
    GtkWidget* scroller_ = nullptr;
    GtkWidget* columnView_ = nullptr;
    GListModel* model_ = nullptr;
    LogDocument* doc_ = nullptr;
    unsigned lastCount_ = 0;
};