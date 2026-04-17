#include "ui/DetailPanel.h"
#include "ui/ThemeColors.h"
#include "infra/Utf.h"
#include "infra/Clipboard.h"
#include "sql/SqlStats.h"
#include "sql/SqlFormatter.h"
#include "sql/SqlParamSubst.h"
#include "sql/JsonPretty.h"
#include <richedit.h>

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

    // RichEdit for syntax-highlighted SQL/JSON.
    LoadLibraryW(L"Msftedit.dll");
    hwndEdit_ = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOOLEDRAGDROP,
        0, headerH, 400, 200 - headerH, hwnd_, nullptr, hInstance, nullptr);
    if (hFont_) SendMessageW(hwndEdit_, WM_SETFONT,
                             reinterpret_cast<WPARAM>(hFont_), TRUE);

    // Dark background for RichEdit + brush for WM_CTLCOLOR*.
    auto& theme = CurrentTheme();
    hBgBrush_ = CreateSolidBrush(ToCOLORREF(theme.background));
    SendMessageW(hwndEdit_, EM_SETBKGNDCOLOR, 0, ToCOLORREF(theme.background));

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

// Find Cust1 (Params) line before a SQL line on the same thread.
// Mirrors the Mac version: search backwards up to 50000 lines, counting
// only same-thread lines (stop after 200 same-thread misses).  Stop at
// another SQL/Cust2 boundary (= different query).
static std::string FindParamsForSql(LogDocument* doc, int lineId) {
    const auto& lines = doc->AllLines();
    const uint8_t* base = doc->MappedBase();
    const auto& sqlLine = lines[lineId];
    int thread = sqlLine.thread;
    if (thread < 0) return {};

    int sameThreadSeen = 0;
    int earliest = std::max(0, lineId - 50000);
    for (int i = lineId - 1; i >= earliest; --i) {
        const auto& prev = lines[i];
        if (prev.thread != thread) continue;
        ++sameThreadSeen;
        if (sameThreadSeen > 200) break;

        if (static_cast<LogLevel>(prev.level) == LogLevel::Cust1) {
            auto msg = GetMessage(base, prev);
            while (!msg.empty() && (msg[0] == ' ' || msg[0] == '\t')) msg.remove_prefix(1);
            if (!msg.empty() && msg[0] == '{') return std::string(msg);
        }
        // Another SQL/Cust2 = different query boundary → stop
        auto lvl = static_cast<LogLevel>(prev.level);
        if (lvl == LogLevel::Sql || lvl == LogLevel::Cust2) break;
    }
    return {};
}

static void SetRangeColor(HWND hwnd, int start, int end, COLORREF color) {
    CHARRANGE cr = {start, end};
    SendMessageW(hwnd, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr));
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));
}

enum class TokType { Keyword, String, Number, Operator, Bind, Comment, Ident };

struct HlToken {
    int start;
    int end;
    TokType type;
};

static bool IsSqlKeyword(const std::string& upper) {
    static const char* kw[] = {
        "SELECT","FROM","WHERE","AND","OR","NOT","IN","IS","NULL","AS","ON",
        "JOIN","LEFT","RIGHT","INNER","OUTER","CROSS","FULL","ORDER","BY",
        "GROUP","HAVING","LIMIT","OFFSET","INSERT","INTO","VALUES","UPDATE",
        "SET","DELETE","CREATE","ALTER","DROP","TABLE","INDEX","VIEW",
        "UNION","ALL","DISTINCT","CASE","WHEN","THEN","ELSE","END",
        "EXISTS","BETWEEN","LIKE","CAST","COALESCE","COUNT","SUM","MAX",
        "MIN","AVG","TOP","WITH","DECLARE","BEGIN","RETURN","FOR","XML",
        "PATH","STUFF","ISNULL","CONVERT","NVARCHAR","BIGINT","INT",
        nullptr};
    for (int i = 0; kw[i]; ++i) if (upper == kw[i]) return true;
    return false;
}

static std::vector<HlToken> TokenizeSqlForHighlight(const std::wstring& text) {
    std::vector<HlToken> tokens;
    int len = static_cast<int>(text.size());
    int i = 0;
    while (i < len) {
        wchar_t c = text[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }

        if (c == '\'') {
            int start = i++;
            while (i < len) {
                if (text[i] == '\'' && i + 1 < len && text[i + 1] == '\'') { i += 2; continue; }
                if (text[i] == '\'') { i++; break; }
                i++;
            }
            tokens.push_back({start, i, TokType::String});
            continue;
        }
        if (c == '-' && i + 1 < len && text[i + 1] == '-') {
            int start = i;
            while (i < len && text[i] != '\n') i++;
            tokens.push_back({start, i, TokType::Comment});
            continue;
        }
        if (c == ':' || c == '?') {
            int start = i++;
            if (c == ':') while (i < len && (iswalnum(text[i]) || text[i] == '_')) i++;
            tokens.push_back({start, i, TokType::Bind});
            continue;
        }
        if (iswdigit(c) || (c == '.' && i + 1 < len && iswdigit(text[i + 1]))) {
            int start = i++;
            while (i < len && (iswdigit(text[i]) || text[i] == '.')) i++;
            tokens.push_back({start, i, TokType::Number});
            continue;
        }
        if (iswalpha(c) || c == '_' || c == '@' || c == '#') {
            int start = i++;
            while (i < len && (iswalnum(text[i]) || text[i] == '_' || text[i] == '.' ||
                               text[i] == '@' || text[i] == '#')) i++;
            std::string upper;
            for (int k = start; k < i; ++k) {
                wchar_t ch = text[k];
                if (ch >= 'a' && ch <= 'z') ch -= 32;
                upper += static_cast<char>(ch);
            }
            tokens.push_back({start, i, IsSqlKeyword(upper) ? TokType::Keyword : TokType::Ident});
            continue;
        }
        if (c == '(' || c == ')' || c == ',' || c == '=' || c == '<' || c == '>' ||
            c == '+' || c == '-' || c == '*' || c == '/' || c == '|' || c == ';') {
            tokens.push_back({i, i + 1, TokType::Operator});
            i++;
            continue;
        }
        i++;
    }
    return tokens;
}

static std::vector<HlToken> TokenizeJsonForHighlight(const std::wstring& text) {
    std::vector<HlToken> tokens;
    int len = static_cast<int>(text.size());
    int i = 0;
    while (i < len) {
        wchar_t c = text[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }
        if (c == '"') {
            int start = i++;
            while (i < len && text[i] != '"') {
                if (text[i] == '\\') i++;
                i++;
            }
            if (i < len) i++;
            // Key if followed by ':'
            int j = i;
            while (j < len && (text[j] == ' ' || text[j] == '\t')) j++;
            TokType type = (j < len && text[j] == ':') ? TokType::Keyword : TokType::String;
            tokens.push_back({start, i, type});
            continue;
        }
        if (iswdigit(c) || c == '-') {
            int start = i++;
            while (i < len && (iswdigit(text[i]) || text[i] == '.' || text[i] == 'e' ||
                               text[i] == 'E' || text[i] == '+' || text[i] == '-')) i++;
            tokens.push_back({start, i, TokType::Number});
            continue;
        }
        if (c == 't' || c == 'f' || c == 'n') {
            int start = i;
            while (i < len && iswalpha(text[i])) i++;
            tokens.push_back({start, i, TokType::Bind});
            continue;
        }
        if (c == '{' || c == '}' || c == '[' || c == ']' || c == ',' || c == ':') {
            tokens.push_back({i, i + 1, TokType::Operator});
            i++;
            continue;
        }
        i++;
    }
    return tokens;
}

enum class HighlightMode { None, Sql, Json };

static void ApplyHighlighting(HWND hwnd, const std::wstring& text, HighlightMode mode) {
    auto& theme = CurrentTheme();

    SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);

    // Always set default foreground (RichEdit defaults to black otherwise).
    CHARRANGE crAll = {0, -1};
    SendMessageW(hwnd, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&crAll));
    CHARFORMAT2W cfDef = {};
    cfDef.cbSize = sizeof(cfDef);
    cfDef.dwMask = CFM_COLOR;
    cfDef.crTextColor = ToCOLORREF(theme.foreground);
    SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cfDef));

    if (mode == HighlightMode::None) {
        CHARRANGE crStart = {0, 0};
        SendMessageW(hwnd, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&crStart));
        SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    std::vector<HlToken> tokens;
    if (mode == HighlightMode::Sql)
        tokens = TokenizeSqlForHighlight(text);
    else
        tokens = TokenizeJsonForHighlight(text);

    for (const auto& t : tokens) {
        COLORREF color;
        if (mode == HighlightMode::Sql) {
            switch (t.type) {
                case TokType::Keyword:  color = ToCOLORREF(theme.sqlKeyword); break;
                case TokType::String:   color = ToCOLORREF(theme.sqlString); break;
                case TokType::Number:   color = ToCOLORREF(theme.sqlNumber); break;
                case TokType::Comment:  color = ToCOLORREF(theme.sqlComment); break;
                case TokType::Operator: color = ToCOLORREF(theme.sqlOperator); break;
                case TokType::Bind:     color = ToCOLORREF(theme.sqlBind); break;
                default: continue;
            }
        } else {
            switch (t.type) {
                case TokType::Keyword:  color = ToCOLORREF(theme.jsonKey); break;
                case TokType::String:   color = ToCOLORREF(theme.jsonString); break;
                case TokType::Number:   color = ToCOLORREF(theme.jsonNumber); break;
                case TokType::Bind:     color = ToCOLORREF(theme.jsonBool); break;
                case TokType::Operator: color = ToCOLORREF(theme.jsonBracket); break;
                default: continue;
            }
        }
        SetRangeColor(hwnd, t.start, t.end, color);
    }

    // Reset selection to start
    CHARRANGE crStart = {0, 0};
    SendMessageW(hwnd, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&crStart));
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwnd, nullptr, FALSE);
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
    HighlightMode hlMode = HighlightMode::None;

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
        hlMode = HighlightMode::Sql;
    }
    // JSON lines (Cust1/Params)
    else if (level == LogLevel::Cust1 ||
             (msgStr.size() > 1 && (msgStr[0] == '{' || msgStr[0] == '['))) {
        displayText = JsonPrettyPrint(msgStr);
        hlMode = HighlightMode::Json;
    }
    // Error with embedded SQL
    else if (msgStr.find(" q=") != std::string::npos ||
             (!msgStr.empty() && msgStr[0] == 'q' && msgStr.size() > 1 && msgStr[1] == '=')) {
        auto parsed = SqlStatsParse(msgStr);
        lastSql_ = parsed.sql;
        displayText = SqlFormat(parsed.sql);
        hlMode = HighlightMode::Sql;
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

    SetWindowTextW(hwndHeader_, Utf8ToWide(headerText).c_str());

    auto wideText = Utf8ToWide(displayText);
    SetWindowTextW(hwndEdit_, wideText.c_str());

    // Re-read text from RichEdit to tokenize exactly what it stores internally.
    // Use EM_GETTEXTLENGTHEX / EM_GETTEXTEX to get the raw internal text
    // (line endings as \r) — GetWindowTextW may re-expand them to \r\n.
    GETTEXTLENGTHEX gtl = {};
    gtl.flags = GTL_NUMCHARS | GTL_PRECISE;
    gtl.codepage = 1200; // UTF-16
    int reLen = static_cast<int>(SendMessageW(hwndEdit_, EM_GETTEXTLENGTHEX,
                                              reinterpret_cast<WPARAM>(&gtl), 0));
    std::wstring reText(reLen + 1, L'\0');
    GETTEXTEX gt = {};
    gt.cb = static_cast<DWORD>((reLen + 1) * sizeof(wchar_t));
    gt.flags = GT_DEFAULT;
    gt.codepage = 1200;
    SendMessageW(hwndEdit_, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&gt),
                 reinterpret_cast<LPARAM>(reText.data()));
    reText.resize(reLen);

    ApplyHighlighting(hwndEdit_, reText, hlMode);
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