#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include "core/LogDocument.h"
#include "core/Observer.h"
#include <set>

class LogTableView : public IDocumentListener {
public:
    LogTableView();
    ~LogTableView();

    static void RegisterClass(HINSTANCE hInstance);
    HWND Create(HWND parent, HINSTANCE hInstance, LogDocument* doc);
    HWND GetHwnd() const { return hwnd_; }

    void SetDocument(LogDocument* doc);
    void ScrollToLine(int filteredIdx);
    void SelectLine(int filteredIdx);

    // IDocumentListener
    void OnDocumentChanged(DocumentChanges changes) override;

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void OnVScroll(int code, int pos);
    void OnMouseWheel(int delta);
    void OnLButtonDown(int x, int y, WPARAM keys);
    void OnKeyDown(WPARAM vk, LPARAM flags);
    void OnSize(int w, int h);

    void CreateRenderTarget();
    void DiscardRenderTarget();
    void UpdateScrollInfo();
    int HitTestRow(int y) const;

    HWND hwnd_ = nullptr;
    LogDocument* doc_ = nullptr;

    ID2D1HwndRenderTarget* rt_ = nullptr;
    ID2D1SolidColorBrush* brush_ = nullptr;
    IDWriteTextFormat* textFormat_ = nullptr;

    int topRow_ = 0;
    int rowHeight_ = 20;
    int clientWidth_ = 0;
    int clientHeight_ = 0;
    std::set<size_t> selectedRows_;
    size_t anchorRow_ = 0;
    bool showDuration_ = false;

    static constexpr const wchar_t* kClassName = L"LogramLogTable";
};
