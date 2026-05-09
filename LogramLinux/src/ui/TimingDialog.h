#pragma once

#include <gtk/gtk.h>
#include <functional>

class LogDocument;

// Modal dialog showing the top-1000 slowest method-timing pairs (+/- pairs)
// sorted by duration descending. Double-click on a row jumps to that line in
// the main table.
class TimingDialog {
public:
    explicit TimingDialog(GtkWindow* parent, LogDocument* doc,
                          std::function<void(int lineId)> onGoTo);
    void Show();

    // Internal — called from a static callback.
    void ActivateRow(unsigned position);

private:
    GtkWindow* parent_ = nullptr;
    LogDocument* doc_ = nullptr;
    std::function<void(int)> onGoTo_;
    GtkWidget* window_ = nullptr;
    GListStore* store_ = nullptr;
};