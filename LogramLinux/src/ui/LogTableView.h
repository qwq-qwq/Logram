#pragma once

#include <gtk/gtk.h>
#include <functional>
#include <vector>

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
    // Scroll/focus to the given position within the filtered list.
    void ScrollToPosition(unsigned position);
    // Show/hide the Duration column. Off by default (matches macOS/Windows).
    void SetDurationVisible(bool visible);
    bool IsDurationVisible() const { return durationVisible_; }

    // Fired when the user picks a new row. The argument is the document's
    // line id (index into AllLines), or -1 if nothing is selected.
    void SetOnSelectionChanged(std::function<void(int lineId)> cb) {
        onSelection_ = std::move(cb);
    }

    // Returns the document line IDs of all currently selected rows in
    // visual order (smallest filtered position first).
    std::vector<int> SelectedLineIds() const;

    // Position-in-filtered of the row most recently focused/selected
    // (the row shown in the detail panel). -1 if nothing is selected.
    int LeadPosition() const { return leadPos_; }

    // Internal — called from a static GTK callback.
    void OnSelectionRangeChanged(unsigned position, unsigned n_items);
    void ShowContextMenu(double x, double y);

private:
    GtkWidget* scroller_ = nullptr;
    GtkWidget* columnView_ = nullptr;
    GtkWidget* popover_ = nullptr;
    GtkColumnViewColumn* durationColumn_ = nullptr;
    GListModel* model_ = nullptr;
    GtkSelectionModel* selection_ = nullptr;
    LogDocument* doc_ = nullptr;
    unsigned lastCount_ = 0;
    int leadPos_ = -1; // position-in-filtered of the row shown in detail panel
    bool durationVisible_ = false;
    std::function<void(int)> onSelection_;
};