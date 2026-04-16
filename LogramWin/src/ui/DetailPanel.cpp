#include "ui/DetailPanel.h"
#include "infra/Utf.h"
#include "sql/SqlStats.h"
#include "sql/SqlFormatter.h"
#include "sql/SqlParamSubst.h"
#include "sql/JsonPretty.h"

DetailPanel::DetailPanel() {}
DetailPanel::~DetailPanel() {
    if (doc_) doc_->listeners.Remove(this);
}

HWND DetailPanel::Create(HWND parent, HINSTANCE hInstance, LogDocument* doc) {
    doc_ = doc;
    if (doc_) doc_->listeners.Add(this);

    hwnd_ = CreateWindowExW(0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 400, 200, parent, nullptr, hInstance, nullptr);

    // For now use a simple multi-line EDIT control as placeholder
    // Will be replaced with Scintilla in Этап 4
    hwndEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, 400, 200, hwnd_, nullptr, hInstance, nullptr);

    // Set monospaced font
    HFONT hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    if (hFont) SendMessageW(hwndEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    return hwnd_;
}

void DetailPanel::SetDocument(LogDocument* doc) {
    if (doc_) doc_->listeners.Remove(this);
    doc_ = doc;
    if (doc_) doc_->listeners.Add(this);
}

void DetailPanel::ShowLine(int lineId) {
    if (!doc_ || !hwndEdit_ || lineId < 0 ||
        lineId >= static_cast<int>(doc_->AllLines().size())) {
        SetWindowTextW(hwndEdit_, L"");
        return;
    }

    const auto& line = doc_->AllLines()[lineId];
    const uint8_t* base = doc_->MappedBase();
    auto level = static_cast<LogLevel>(line.level);
    auto msg = GetMessage(base, line);
    std::string msgStr(msg);

    std::string displayText;

    // SQL lines: format SQL
    if (level == LogLevel::Sql || level == LogLevel::Cust2) {
        auto parsed = SqlStatsParse(msgStr);
        if (!parsed.entries.empty()) {
            for (auto& e : parsed.entries) {
                displayText += e.label + ": " + SqlStatsFormatValue(e) + "  ";
            }
            displayText += "\r\n\r\n";
        }
        displayText += SqlFormat(parsed.sql);
    }
    // JSON lines
    else if (level == LogLevel::Cust1 ||
             (msgStr.size() > 1 && (msgStr[0] == '{' || msgStr[0] == '['))) {
        displayText = JsonPrettyPrint(msgStr);
    }
    // Error with embedded SQL
    else if (msgStr.find(" q=") != std::string::npos || msgStr.starts_with("q=")) {
        auto parsed = SqlStatsParse(msgStr);
        displayText = SqlFormat(parsed.sql);
    }
    // Plain text
    else {
        // Replace \n with actual newlines
        for (size_t i = 0; i < msgStr.size(); ++i) {
            if (msgStr[i] == '\\' && i + 1 < msgStr.size() && msgStr[i + 1] == 'n') {
                displayText += "\r\n";
                i++;
            } else {
                displayText += msgStr[i];
            }
        }
        displayText = FormatStackTrace(displayText);
    }

    auto wide = Utf8ToWide(displayText);
    SetWindowTextW(hwndEdit_, wide.c_str());
}

void DetailPanel::OnDocumentChanged(DocumentChanges changes) {
    if (changes.Has(DocumentChanges::SelectionChanged) && doc_) {
        ShowLine(doc_->SelectedLineId());
    }
}
