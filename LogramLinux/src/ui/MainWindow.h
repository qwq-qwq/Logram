#pragma once

#include <gtk/gtk.h>
#include <memory>

class LogDocument;
class LogTableView;
class FilterSidebar;
class DetailPanel;

class MainWindow {
public:
    explicit MainWindow(GtkApplication* app);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    GtkWidget* Widget() const { return window_; }

    void OnOpenClicked();
    void LoadFile(const char* utf8Path);

    // Search actions (invoked via GAction accelerators).
    void FocusSearch();
    void SearchNext();
    void SearchPrev();
    void OnSearchChanged();

    // Row actions (invoked via GAction accelerators).
    void CopySelectedLine();
    void JumpToPair();
    void ToggleFocusOnCall();
    void GotoError(bool forward);

    // Header-bar params toggle.
    void SetParamsEnabled(bool enabled);

private:
    void UpdateStatus();
    void OnFiltersChanged();
    void OnRowSelected(int lineId);
    void DoSearch(bool forward);
    void ResetSearch();
    void InstallActions();

    GtkApplication* app_ = nullptr;
    GtkWidget* window_ = nullptr;
    GtkWidget* statusLabel_ = nullptr;
    GtkWidget* searchEntry_ = nullptr;
    GtkWidget* paramsToggle_ = nullptr;
    int currentMatchPos_ = -1;
    int selectedLineId_ = -1;
    std::unique_ptr<LogTableView> table_;
    std::unique_ptr<FilterSidebar> sidebar_;
    std::unique_ptr<DetailPanel> detail_;
    std::unique_ptr<LogDocument> doc_;
};