#pragma once

#include <gtk/gtk.h>
#include <cstdint>
#include <functional>

class LogDocument;

// Left-hand panel with two checkbox sections: log levels and threads.
// Toggling a checkbox flips the corresponding bit in the document's
// EnabledLevelMask / EnabledThreadMask, calls ApplyFilters(), and fires
// the OnChanged callback so the table can refresh.
class FilterSidebar {
public:
    FilterSidebar();
    ~FilterSidebar();

    FilterSidebar(const FilterSidebar&) = delete;
    FilterSidebar& operator=(const FilterSidebar&) = delete;

    GtkWidget* Widget() const { return scroller_; }
    void SetDocument(LogDocument* doc);
    // Re-syncs the checkboxes with the document's current masks. Use when
    // the document's enabled masks were changed externally (e.g. FocusOnCall).
    void Refresh();
    void SetOnChanged(std::function<void()> cb) { onChanged_ = std::move(cb); }

    // Internal — invoked from C-style GTK callbacks.
    void OnLevelToggled(int levelId, bool active);
    void OnThreadToggled(int threadId, bool active);
    void OnAllLevels(bool enable);
    void OnAllThreads(bool enable);
    void OnLevelOnlyThis(int levelId);
    void OnThreadOnlyThis(int threadId);
    void OnPresetClicked(uint64_t mask);
    void ShowRowContextMenu(GtkWidget* row, double x, double y);

private:
    void Rebuild();
    void ClearBox(GtkWidget* box);
    void NotifyChanged();

    GtkWidget* scroller_ = nullptr;
    GtkWidget* levelsBox_ = nullptr;
    GtkWidget* threadsBox_ = nullptr;
    GtkWidget* contextPopover_ = nullptr;
    LogDocument* doc_ = nullptr;
    std::function<void()> onChanged_;
};