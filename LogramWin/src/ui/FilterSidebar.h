#pragma once
#include <windows.h>
#include "core/LogDocument.h"
#include "core/Observer.h"

// Left-side filter pane. Contains a preset-button strip at the top and
// a ListView with level/thread check-boxes below it. Owns its own window
// class so it can consume WM_NOTIFY from the embedded ListView directly.
class FilterSidebar : public IDocumentListener {
public:
    FilterSidebar();
    ~FilterSidebar();

    static void RegisterClass(HINSTANCE hInstance);

    HWND Create(HWND parent, HINSTANCE hInstance, LogDocument* doc);
    HWND GetHwnd() const { return hwnd_; }
    void SetDocument(LogDocument* doc);
    void OnDocumentChanged(DocumentChanges changes) override;

    // Called by MainWindow on WM_SIZE / splitter drag.
    void Resize(int w, int h);

    // Preset kinds for the toolbar row
    enum Preset { PresetAll, PresetErrors, PresetSql, PresetHttp, PresetCallPairs };

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void RebuildList();                       // re-fill ListView (levels + active threads)
    void ReadCheckStatesIntoDoc();            // collapse checkboxes into level/thread masks
    void ApplyPreset(Preset p);
    void CreatePresetButtons(HINSTANCE hInstance);
    void LayoutInternal();

    HWND hwnd_      = nullptr;
    HWND hwndList_  = nullptr;
    HWND hwndPreset_[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    LogDocument* doc_ = nullptr;

    int clientW_ = 0;
    int clientH_ = 0;
    bool suppressNotify_ = false;   // true while programmatically toggling checks
    int threadRowBase_ = 0;         // index of first thread row in the list

    static constexpr const wchar_t* kClassName = L"LogramFilterSidebar";
    static constexpr int kPresetBarHeight = 28;
    static constexpr int kPresetButtonBase = 9100;
};