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

    // EDIT as the panel root — MoveWindow from parent directly resizes the
    // control. A STATIC wrapper does not forward WM_SIZE to child EDITs, so
    // the text box stayed at its initial 400x200 no matter the layout.
    // TODO(Этап 4): replace with Scintilla.
    hwnd_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 400, 200, parent, nullptr, hInstance, nullptr);
    hwndEdit_ = hwnd_;

    // DPI-scaled monospaced font
    UINT dpi = GetDpiForWindow(parent);
    if (dpi == 0) dpi = 96;
    int fontHeight = -MulDiv(10, dpi, 72); // 10pt
    HFONT hFont = CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
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
