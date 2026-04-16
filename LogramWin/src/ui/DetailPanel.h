#pragma once
#include <windows.h>
#include "core/LogDocument.h"
#include "core/Observer.h"

class DetailPanel : public IDocumentListener {
public:
    DetailPanel();
    ~DetailPanel();

    HWND Create(HWND parent, HINSTANCE hInstance, LogDocument* doc);
    HWND GetHwnd() const { return hwnd_; }
    void SetDocument(LogDocument* doc);
    void ShowLine(int lineId);
    void OnDocumentChanged(DocumentChanges changes) override;

private:
    HWND hwnd_ = nullptr;
    HWND hwndEdit_ = nullptr; // RichEdit or Scintilla (placeholder: EDIT for now)
    LogDocument* doc_ = nullptr;
};
