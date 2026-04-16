#pragma once
#include <windows.h>
#include "core/LogDocument.h"
#include "core/Observer.h"
#include "ui/LogTableView.h"
#include "ui/FilterSidebar.h"
#include "ui/DetailPanel.h"
#include "ui/Splitter.h"
#include <string>
#include <thread>
#include <memory>

class MainWindow : public IDocumentListener {
public:
    MainWindow();
    ~MainWindow();

    static void RegisterClass(HINSTANCE hInstance);
    bool Create(HINSTANCE hInstance, int nCmdShow);
    HWND GetHwnd() const { return hwnd_; }

    void LoadFile(const wchar_t* path);
    void ShowOpenDialog();

    // IDocumentListener
    void OnDocumentChanged(DocumentChanges changes) override;

    LogDocument& Doc() { return doc_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnCreate();
    void OnSize(int width, int height);
    void OnCommand(int id, int code, HWND ctrl);
    void OnNotify(NMHDR* hdr);
    void OnDropFiles(HDROP hDrop);
    void OnDestroy();
    void UpdateTitle();
    void UpdateStatusBar();
    void LayoutChildren();

    // Search helpers
    void RunSearch(bool forward);
    void JumpToErrorRelative(int delta);  // +1 next, -1 prev
    void JumpToMatchingPair();

    HWND hwnd_ = nullptr;
    HWND hwndStatus_ = nullptr;
    HWND hwndToolbar_ = nullptr;
    HWND hwndSearch_ = nullptr;

    std::unique_ptr<LogTableView>  tableView_;
    std::unique_ptr<FilterSidebar> filterSidebar_;
    std::unique_ptr<DetailPanel>   detailPanel_;
    std::unique_ptr<Splitter>      sidebarSplitter_;
    std::unique_ptr<Splitter>      detailSplitter_;

    LogDocument doc_;
    std::jthread loadThread_;

    int lastFoundIdx_ = -1;

    static constexpr const wchar_t* kClassName = L"LogramMainWindow";

    // Child-window pixel dimensions
    static constexpr int kToolbarHeight = 36;
    static constexpr int kSplitterThickness = 4;
    static constexpr int kMinSidebarWidth = 120;
    static constexpr int kMinDetailHeight = 80;
    int sidebarWidth_ = 220;   // left pane width
    int detailHeight_ = 260;   // bottom detail pane height
};