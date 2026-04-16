#pragma once
#include <windows.h>
#include "core/LogDocument.h"
#include "core/Observer.h"
#include <string>

class DetailPanel : public IDocumentListener {
public:
    DetailPanel();
    ~DetailPanel();

    static void RegisterClass(HINSTANCE hInstance);
    HWND Create(HWND parent, HINSTANCE hInstance, LogDocument* doc);
    HWND GetHwnd() const { return hwnd_; }
    void SetDocument(LogDocument* doc);
    void ShowLine(int lineId);
    void OnDocumentChanged(DocumentChanges changes) override;

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void LayoutInternal();
    int Scale(int dip) const;

    HWND hwnd_ = nullptr;
    HWND hwndHeader_ = nullptr;   // STATIC label for stats line
    HWND hwndEdit_ = nullptr;
    HWND hwndParams_ = nullptr;   // Params toggle button
    HWND hwndCopy_ = nullptr;     // Copy button
    HFONT hFont_ = nullptr;
    HFONT hFontSmall_ = nullptr;
    LogDocument* doc_ = nullptr;
    int lastLineId_ = -1;
    bool paramsEnabled_ = false;

    std::string lastSql_;     // raw SQL before/after param subst
    std::string lastStats_;   // formatted stats header

    static constexpr const wchar_t* kClassName = L"LogramDetailPanel";
    static constexpr int kHeaderHeightDip = 24;
    static constexpr int IDC_PARAMS_BTN = 9200;
    static constexpr int IDC_COPY_BTN   = 9201;
};
