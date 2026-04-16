#include "ui/DetailPanel.h"
#include "ui/ThemeColors.h"
#include "infra/Utf.h"
#include "infra/Clipboard.h"
#include "sql/SqlStats.h"
#include "sql/SqlFormatter.h"
#include "sql/SqlParamSubst.h"
#include "sql/JsonPretty.h"

DetailPanel::DetailPanel() {}
DetailPanel::~DetailPanel() {
    if (doc_) doc_->listeners.Remove(this);
    if (hFont_) DeleteObject(hFont_);
    if (hFontSmall_) DeleteObject(hFontSmall_);
    if (hBgBrush_) DeleteObject(hBgBrush_);
}

void DetailPanel::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
}

int DetailPanel::Scale(int dip) const {
    UINT dpi = hwnd_ ? GetDpiForWindow(hwnd_) : 96;
    if (dpi == 0) dpi = 96;
    return MulDiv(dip, static_cast<int>(dpi), 96);
}

HWND DetailPanel::Create(HWND parent, HINSTANCE hInstance, LogDocument* doc) {
    doc_ = doc;
    if (doc_) doc_->listeners.Add(this);

    hwnd_ = CreateWindowExW(0, kClassName, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 400, 200, parent, nullptr, hInstance, this);

    UINT dpi = GetDpiForWindow(parent);
    if (dpi == 0) dpi = 96;

    hFont_ = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    hFontSmall_ = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");

    int headerH = Scale(kHeaderHeightDip);
    int btnW = Scale(60);

    // Stats header label
    hwndHeader_ = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, 0, 400, headerH, hwnd_, nullptr, hInstance, nullptr);
    if (hFontSmall_) SendMessageW(hwndHeader_, WM_SETFONT,
                                  reinterpret_cast<WPARAM>(hFontSmall_), TRUE);

    // Params button
    hwndParams_ = CreateWindowExW(0, L"BUTTON", L"Params",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
        0, 0, btnW, headerH - Scale(4), hwnd_,
        reinterpret_cast<HMENU>(static_cast<LONG_PTR>(IDC_PARAMS_BTN)),
        hInstance, nullptr);
    if (hFontSmall_) SendMessageW(hwndParams_, WM_SETFONT,
                                  reinterpret_cast<WPARAM>(hFontSmall_), TRUE);

    // Copy button
    hwndCopy_ = CreateWindowExW(0, L"BUTTON", L"Copy",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, btnW, headerH - Scale(4), hwnd_,
        reinterpret_cast<HMENU>(static_cast<LONG_PTR>(IDC_COPY_BTN)),
        hInstance, nullptr);
    if (hFontSmall_) SendMessageW(hwndCopy_, WM_SETFONT,
                                  reinterpret_cast<WPARAM>(hFontSmall_), TRUE);

    // Multi-line edit
    hwndEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, headerH, 400, 200 - headerH, hwnd_, nullptr, hInstance, nullptr);
    if (hFont_) SendMessageW(hwndEdit_, WM_SETFONT,
                             reinterpret_cast<WPARAM>(hFont_), TRUE);

    // Dark background brush matching the theme.
    auto& theme = CurrentTheme();
    hBgBrush_ = CreateSolidBrush(ToCOLORREF(theme.background));

    return hwnd_;
}

void DetailPanel::LayoutInternal() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int headerH = Scale(kHeaderHeightDip);
    int btnW = Scale(60);
    int pad = Scale(4);

    if (hwndHeader_)
        MoveWindow(hwndHeader_, pad, 0, w - 2 * btnW - 3 * pad, headerH, TRUE);

    if (hwndParams_)
        MoveWindow(hwndParams_, w - 2 * (btnW + pad), Scale(2), btnW, headerH - Scale(4), TRUE);

    if (hwndCopy_)
        MoveWindow(hwndCopy_, w - btnW - pad, Scale(2), btnW, headerH - Scale(4), TRUE);

    if (hwndEdit_)
        MoveWindow(hwndEdit_, 0, headerH, w, std::max(0, h - headerH), TRUE);
}

void DetailPanel::SetDocument(LogDocument* doc) {
    if (doc_) doc_->listeners.Remove(this);
    doc_ = doc;
    if (doc_) doc_->listeners.Add(this);
}

// Find Cust1 (Params) line adjacent to a SQL line on the same thread.
static std::string FindParamsForSql(LogDocument* doc, int lineId) {
    const auto& lines = doc->AllLines();
    const uint8_t* base = doc->MappedBase();
    const auto& sqlLine = lines[lineId];
    int thread = sqlLine.thread;

    // Look up to 5 lines before and after for a Cust1 on the same thread.
    int total = static_cast<int>(lines.size());
    for (int delta = -5; delta <= 5; ++delta) {
        int idx = lineId + delta;
        if (idx < 0 || idx >= total || idx == lineId) continue;
        const auto& candidate = lines[idx];
        if (candidate.thread != thread) continue;
        if (static_cast<LogLevel>(candidate.level) != LogLevel::Cust1) continue;
        auto msg = GetMessage(base, candidate);
        // Trim leading whitespace/tab — messageOffset may not skip the tab.
        while (!msg.empty() && (msg[0] == ' ' || msg[0] == '\t')) msg.remove_prefix(1);
        if (!msg.empty() && msg[0] == '{') return std::string(msg);
    }
    return {};
}

void DetailPanel::ShowLine(int lineId) {
    lastLineId_ = lineId;
    lastSql_.clear();
    lastStats_.clear();

    if (!doc_ || !hwndEdit_ || lineId < 0 ||
        lineId >= static_cast<int>(doc_->AllLines().size())) {
        SetWindowTextW(hwndEdit_, L"");
        SetWindowTextW(hwndHeader_, L"");
        return;
    }

    const auto& line = doc_->AllLines()[lineId];
    const uint8_t* base = doc_->MappedBase();
    auto level = static_cast<LogLevel>(line.level);
    auto msg = GetMessage(base, line);
    // Trim leading tab that separates level code from message in the raw line.
    while (!msg.empty() && (msg[0] == '\t' || msg[0] == ' ')) msg.remove_prefix(1);
    std::string msgStr(msg);

    std::string displayText;
    std::string headerText;

    // SQL lines: format SQL
    if (level == LogLevel::Sql || level == LogLevel::Cust2) {
        auto parsed = SqlStatsParse(msgStr);
        if (!parsed.entries.empty()) {
            for (auto& e : parsed.entries) {
                headerText += e.label + ": " + SqlStatsFormatValue(e) + "  ";
            }
        }
        lastSql_ = parsed.sql;

        if (paramsEnabled_) {
            auto paramsJson = FindParamsForSql(doc_, lineId);
            if (!paramsJson.empty()) {
                displayText = SqlFormat(SqlParamSubstitute(lastSql_, paramsJson));
            } else {
                displayText = SqlFormat(lastSql_);
            }
        } else {
            displayText = SqlFormat(lastSql_);
        }
        lastStats_ = headerText;
    }
    // JSON lines (Cust1/Params)
    else if (level == LogLevel::Cust1 ||
             (msgStr.size() > 1 && (msgStr[0] == '{' || msgStr[0] == '['))) {
        displayText = JsonPrettyPrint(msgStr);
    }
    // Error with embedded SQL
    else if (msgStr.find(" q=") != std::string::npos ||
             (!msgStr.empty() && msgStr[0] == 'q' && msgStr.size() > 1 && msgStr[1] == '=')) {
        auto parsed = SqlStatsParse(msgStr);
        lastSql_ = parsed.sql;
        displayText = SqlFormat(parsed.sql);
    }
    // Plain text
    else {
        std::string plain;
        for (size_t i = 0; i < msgStr.size(); ++i) {
            if (msgStr[i] == '\\' && i + 1 < msgStr.size() && msgStr[i + 1] == 'n') {
                plain += "\n";
                i++;
            } else {
                plain += msgStr[i];
            }
        }
        displayText = FormatStackTrace(plain);
    }

    // EDIT control requires \r\n
    std::string crlfText;
    crlfText.reserve(displayText.size() + displayText.size() / 20);
    for (size_t i = 0; i < displayText.size(); ++i) {
        if (displayText[i] == '\n' && (i == 0 || displayText[i - 1] != '\r')) {
            crlfText += "\r\n";
        } else {
            crlfText += displayText[i];
        }
    }

    SetWindowTextW(hwndHeader_, Utf8ToWide(headerText).c_str());
    SetWindowTextW(hwndEdit_, Utf8ToWide(crlfText).c_str());
}

void DetailPanel::OnDocumentChanged(DocumentChanges changes) {
    if (changes.Has(DocumentChanges::SelectionChanged) && doc_) {
        ShowLine(doc_->SelectedLineId());
    }
}

LRESULT CALLBACK DetailPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DetailPanel* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<DetailPanel*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<DetailPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT DetailPanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            LayoutInternal();
            return 0;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            auto& theme = CurrentTheme();
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, ToCOLORREF(theme.foreground));
            SetBkColor(hdc, ToCOLORREF(theme.background));
            return reinterpret_cast<LRESULT>(hBgBrush_);
        }

        case WM_ERASEBKGND: {
            auto& theme = CurrentTheme();
            RECT rc;
            GetClientRect(hwnd_, &rc);
            HBRUSH brush = CreateSolidBrush(ToCOLORREF(theme.background));
            FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
            DeleteObject(brush);
            return 1;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_PARAMS_BTN) {
                paramsEnabled_ = (SendMessageW(hwndParams_, BM_GETCHECK, 0, 0) == BST_CHECKED);
                if (lastLineId_ >= 0) ShowLine(lastLineId_);
                return 0;
            }
            if (id == IDC_COPY_BTN) {
                int len = GetWindowTextLengthW(hwndEdit_);
                if (len > 0) {
                    std::wstring buf(len + 1, L'\0');
                    GetWindowTextW(hwndEdit_, buf.data(), len + 1);
                    CopyToClipboard(hwnd_, WideToUtf8(buf.c_str()));
                }
                return 0;
            }
            break;
        }
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}