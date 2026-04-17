#include "ui/MainWindow.h"
#include "ui/App.h"
#include "ui/ThemeColors.h"
#include "ui/StatsDialog.h"
#include "ui/TimingDialog.h"
#include "infra/Utf.h"
#include "infra/Settings.h"
#include "resource.h"
#include <shobjidl.h>
#include <shellapi.h>
#include <commctrl.h>
#include <algorithm>

// Child-control IDs (must not overlap with menu IDs in resource.h)
namespace {
constexpr int IDC_SEARCH_EDIT   = 9001;
constexpr int IDC_BTN_FINDNEXT  = 9010;
constexpr int IDC_BTN_FINDPREV  = 9011;
constexpr int IDC_BTN_ERRNEXT   = 9012;
constexpr int IDC_BTN_ERRPREV   = 9013;
} // namespace

int MainWindow::Scale(int dip) const {
    UINT dpi = hwnd_ ? GetDpiForWindow(hwnd_) : 96;
    if (dpi == 0) dpi = 96;
    return MulDiv(dip, static_cast<int>(dpi), 96);
}

MainWindow::MainWindow() {
    doc_.listeners.Add(this);
}

MainWindow::~MainWindow() {
    doc_.listeners.Remove(this);
    if (loadThread_.joinable()) loadThread_.join();
    if (hBgBrush_) DeleteObject(hBgBrush_);
    if (hToolbarFont_) DeleteObject(hToolbarFont_);
}

void MainWindow::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    auto rect = Settings::Instance().GetWindowRect();

    // Default size in DIPs → physical pixels using primary monitor DPI.
    UINT defDpi = GetDpiForSystem();
    if (defDpi == 0) defDpi = 96;
    int defW = MulDiv(1200, defDpi, 96);
    int defH = MulDiv(800,  defDpi, 96);

    int x = rect.valid ? rect.x : CW_USEDEFAULT;
    int y = rect.valid ? rect.y : CW_USEDEFAULT;
    int w = rect.valid ? rect.w : defW;
    int h = rect.valid ? rect.h : defH;

    hwnd_ = CreateWindowExW(
        0, kClassName, L"Logram",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y, w, h,
        nullptr, nullptr, hInstance, this);

    if (!hwnd_) return false;

    BOOL useDark = TRUE;
    DwmSetWindowAttribute(hwnd_, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &useDark, sizeof(useDark));

    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate();
            return 0;

        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_COMMAND:
            OnCommand(LOWORD(wParam), HIWORD(wParam),
                      reinterpret_cast<HWND>(lParam));
            return 0;

        case WM_NOTIFY:
            OnNotify(reinterpret_cast<NMHDR*>(lParam));
            return 0;

        case WM_DROPFILES:
            OnDropFiles(reinterpret_cast<HDROP>(wParam));
            return 0;

        case WM_APP_DOC_LOADED: {
            DocumentChanges changes;
            changes.flags = DocumentChanges::DataLoaded | DocumentChanges::StatisticsChanged;
            doc_.listeners.Notify(changes);
            UpdateTitle();
            UpdateStatusBar();
            return 0;
        }

        case WM_CLOSE: {
            RECT rc;
            if (GetWindowRect(hwnd_, &rc)) {
                Settings::Instance().SetWindowRect(rc.left, rc.top,
                    rc.right - rc.left, rc.bottom - rc.top);
            }
            DestroyWindow(hwnd_);
            return 0;
        }

        case WM_DESTROY:
            OnDestroy();
            PostQuitMessage(0);
            return 0;

        case WM_ERASEBKGND: {
            auto& theme = CurrentTheme();
            RECT rc;
            GetClientRect(hwnd_, &rc);
            HBRUSH brush = CreateSolidBrush(ToCOLORREF(theme.background));
            FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
            DeleteObject(brush);
            return 1;
        }

        case WM_DRAWITEM: {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (dis && dis->CtlType == ODT_BUTTON) {
                DrawThemedButton(dis);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLOREDIT: {
            auto& theme = CurrentTheme();
            HDC hdc = reinterpret_cast<HDC>(wParam);
            // Slightly lighter background so the search box is visible
            ColorRGBA editBg;
            editBg.r = std::min(1.0f, theme.background.r + 0.06f);
            editBg.g = std::min(1.0f, theme.background.g + 0.06f);
            editBg.b = std::min(1.0f, theme.background.b + 0.06f);
            editBg.a = 1.0f;
            SetTextColor(hdc, ToCOLORREF(theme.foreground));
            SetBkColor(hdc, ToCOLORREF(editBg));
            if (!hBgBrush_)
                hBgBrush_ = CreateSolidBrush(ToCOLORREF(editBg));
            return reinterpret_cast<LRESULT>(hBgBrush_);
        }
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void MainWindow::OnCreate() {
    HINSTANCE hInst = LogramApp::Get()->GetInstance();

    // Status bar (bottom)
    hwndStatus_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hwnd_, nullptr, hInst, nullptr);

    int parts[] = {200, 350, 500, 650, -1};
    SendMessageW(hwndStatus_, SB_SETPARTS, 5, reinterpret_cast<LPARAM>(parts));
    SendMessageW(hwndStatus_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Ready"));

    // Search box in the toolbar strip (top). Exact rect is set later in
    // LayoutChildren; use DPI-scaled defaults here.
    hwndSearch_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        Scale(8), Scale(6), Scale(300), Scale(kToolbarHeightDip - 12),
        hwnd_,
        reinterpret_cast<HMENU>(static_cast<LONG_PTR>(IDC_SEARCH_EDIT)),
        hInst, nullptr);

    // DPI-scaled font for toolbar controls
    UINT dpi = GetDpiForWindow(hwnd_);
    if (dpi == 0) dpi = 96;
    hToolbarFont_ = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        VARIABLE_PITCH | FF_SWISS, L"Segoe UI");
    if (hToolbarFont_) SendMessageW(hwndSearch_, WM_SETFONT,
                            reinterpret_cast<WPARAM>(hToolbarFont_), TRUE);
    SendMessageW(hwndSearch_, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(L"Search..."));
    SendMessageW(hwndSearch_, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(4), 0));

    // Navigation buttons next to search
    auto makeBtn = [&](const wchar_t* label, int id) -> HWND {
        HWND btn = CreateWindowExW(0, L"BUTTON", label,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, Scale(32), Scale(kToolbarHeightDip - 12),
            hwnd_, reinterpret_cast<HMENU>(static_cast<LONG_PTR>(id)),
            hInst, nullptr);
        if (hToolbarFont_) SendMessageW(btn, WM_SETFONT,
                                        reinterpret_cast<WPARAM>(hToolbarFont_), TRUE);
        return btn;
    };
    hwndBtnFindPrev_ = makeBtn(L"\x25B2", IDC_BTN_FINDPREV);     // ▲
    hwndBtnFindNext_ = makeBtn(L"\x25BC", IDC_BTN_FINDNEXT);     // ▼
    hwndBtnErrPrev_  = makeBtn(L"Err \x25B2", IDC_BTN_ERRPREV);  // Err ▲
    hwndBtnErrNext_  = makeBtn(L"Err \x25BC", IDC_BTN_ERRNEXT);  // Err ▼

    // Populate sidebarWidth_ / detailHeight_ from stored prefs (values are
    // physical pixels as last persisted). detailHeight_=-1 means "auto 70%"
    // which is resolved in the first LayoutChildren call once we know the window size.
    // Sidebar and detail heights always start as -1 (auto) — resolved in first LayoutChildren.

    // Filter sidebar (left)
    filterSidebar_ = std::make_unique<FilterSidebar>();
    filterSidebar_->Create(hwnd_, hInst, &doc_);

    // Table view (top-right)
    tableView_ = std::make_unique<LogTableView>();
    tableView_->Create(hwnd_, hInst, &doc_);

    // Detail panel (bottom-right)
    detailPanel_ = std::make_unique<DetailPanel>();
    detailPanel_->Create(hwnd_, hInst, &doc_);

    // Splitters (exact geometry set in LayoutChildren)
    sidebarSplitter_ = std::make_unique<Splitter>(Splitter::Orientation::Vertical);
    sidebarSplitter_->Create(hwnd_, hInst, 0, Scale(kToolbarHeightDip),
        Scale(kSplitterThicknessDip), Scale(100));

    detailSplitter_ = std::make_unique<Splitter>(Splitter::Orientation::Horizontal);
    detailSplitter_->Create(hwnd_, hInst, 0, 0,
        Scale(100), Scale(kSplitterThicknessDip));
}

void MainWindow::OnSize(int width, int height) {
    if (hwndStatus_) SendMessageW(hwndStatus_, WM_SIZE, 0, 0);

    // Splitter drag re-enters here via SendMessage(parent, WM_SIZE). Pull
    // current positions into our bookkeeping before laying children out.
    // Skip reading before first LayoutChildren resolves auto (-1) values.
    if (sidebarSplitter_ && sidebarWidth_ > 0) {
        int p = sidebarSplitter_->GetPosition();
        if (p > 0) sidebarWidth_ = p;
    }
    if (detailSplitter_ && detailHeight_ > 0) {
        int p = detailSplitter_->GetPosition();
        if (p > 0) {
            RECT rc; GetClientRect(hwnd_, &rc);
            int sbH = 0;
            if (hwndStatus_) {
                RECT sr; GetWindowRect(hwndStatus_, &sr);
                sbH = sr.bottom - sr.top;
            }
            const int splitterPx = Scale(kSplitterThicknessDip);
            int maxY = rc.bottom - sbH - splitterPx;
            if (p < maxY) {
                detailHeight_ = rc.bottom - sbH - p - splitterPx;
            }
        }
    }

    (void)width; (void)height;
    LayoutChildren();
}

void MainWindow::LayoutChildren() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int totalW = rc.right;
    int totalH = rc.bottom;

    int sbH = 0;
    if (hwndStatus_) {
        RECT sr; GetWindowRect(hwndStatus_, &sr);
        sbH = sr.bottom - sr.top;
    }

    // All constants below are in DIPs; scale to physical pixels for Win32.
    const int toolbarPx      = Scale(kToolbarHeightDip);
    const int splitterPx     = Scale(kSplitterThicknessDip);
    const int minSidebarPx   = Scale(kMinSidebarWidthDip);
    const int minDetailPx    = Scale(kMinDetailHeightDip);
    const int minTablePx     = Scale(200);

    // Clamp dimensions so children always have room
    int workH = std::max(0, totalH - toolbarPx - sbH);
    int workY = toolbarPx;
    int workBottom = workY + workH;

    // First layout: sidebar = 15% of window width.
    if (sidebarWidth_ < 0) sidebarWidth_ = totalW * 15 / 100;

    if (sidebarWidth_ < minSidebarPx) sidebarWidth_ = minSidebarPx;
    if (sidebarWidth_ > totalW - minTablePx)
        sidebarWidth_ = std::max(minSidebarPx, totalW - minTablePx);

    int rightX = sidebarWidth_ + splitterPx;
    int rightW = std::max(0, totalW - rightX);

    // First layout: table gets 70%, detail gets 30%.
    if (detailHeight_ < 0) detailHeight_ = workH * 30 / 100;

    int detailH = std::min(detailHeight_, std::max(0, workH - minDetailPx));
    if (detailH < minDetailPx) detailH = minDetailPx;
    if (detailH > workH - minDetailPx) detailH = std::max(minDetailPx, workH - minDetailPx);
    detailHeight_ = detailH;

    int topH = std::max(0, workH - detailH - splitterPx);

    // Toolbar: search box + navigation buttons across the right pane.
    {
        const int pad = Scale(6);
        const int btnY = Scale(4);
        const int ctrlH = toolbarPx - Scale(8);
        const int btnW = Scale(36);
        const int errBtnW = Scale(50);

        // Buttons from right edge: ErrNext, ErrPrev, FindNext, FindPrev
        int x = totalW - pad;
        if (hwndBtnErrNext_) { x -= errBtnW; MoveWindow(hwndBtnErrNext_, x, btnY, errBtnW, ctrlH, TRUE); x -= pad/2; }
        if (hwndBtnErrPrev_) { x -= errBtnW; MoveWindow(hwndBtnErrPrev_, x, btnY, errBtnW, ctrlH, TRUE); x -= pad; }
        if (hwndBtnFindNext_) { x -= btnW; MoveWindow(hwndBtnFindNext_, x, btnY, btnW, ctrlH, TRUE); x -= pad/2; }
        if (hwndBtnFindPrev_) { x -= btnW; MoveWindow(hwndBtnFindPrev_, x, btnY, btnW, ctrlH, TRUE); x -= pad; }

        // Search box fills remaining space
        if (hwndSearch_) {
            const int searchX = sidebarWidth_ + splitterPx + pad;
            const int searchW = std::max(Scale(120), x - searchX);
            MoveWindow(hwndSearch_, searchX, btnY, searchW, ctrlH, TRUE);
        }
    }

    // Filter sidebar (left)
    if (filterSidebar_) {
        MoveWindow(filterSidebar_->GetHwnd(), 0, workY, sidebarWidth_, workH, TRUE);
        filterSidebar_->Resize(sidebarWidth_, workH);
    }

    // Vertical splitter (between sidebar and right pane)
    if (sidebarSplitter_) {
        MoveWindow(sidebarSplitter_->GetHwnd(),
                   sidebarWidth_, workY, splitterPx, workH, TRUE);
    }

    // Table view (top-right)
    if (tableView_) {
        MoveWindow(tableView_->GetHwnd(), rightX, workY, rightW, topH, TRUE);
    }

    // Horizontal splitter (between table and detail pane)
    int splitY = workY + topH;
    if (detailSplitter_) {
        MoveWindow(detailSplitter_->GetHwnd(),
                   rightX, splitY, rightW, splitterPx, TRUE);
        detailSplitter_->SetPosition(splitY); // keep in sync for drags
    }

    // Detail panel (bottom-right)
    int detailY = splitY + splitterPx;
    int detailActualH = std::max(0, workBottom - detailY);
    if (detailPanel_) {
        MoveWindow(detailPanel_->GetHwnd(), rightX, detailY, rightW, detailActualH, TRUE);
    }
}

void MainWindow::OnCommand(int id, int code, HWND ctrl) {
    // Search field — just reset the navigation index on text change.
    // Actual navigation happens via F3/Shift+F3 (RunSearch).
    // Search does NOT filter — it's a find-in-list, not a filter.
    if (ctrl && id == IDC_SEARCH_EDIT) {
        if (code == EN_CHANGE) lastFoundIdx_ = -1;
        return;
    }

    switch (id) {
        case IDC_BTN_FINDNEXT:  RunSearch(true);           return;
        case IDC_BTN_FINDPREV:  RunSearch(false);          return;
        case IDC_BTN_ERRNEXT:   JumpToErrorRelative(+1);   return;
        case IDC_BTN_ERRPREV:   JumpToErrorRelative(-1);   return;
        case ID_FILE_OPEN:
            ShowOpenDialog();
            break;
        case ID_FILE_EXIT:
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            break;
        case ID_VIEW_DURATION:
            Settings::Instance().SetShowDuration(!Settings::Instance().GetShowDuration());
            if (tableView_) InvalidateRect(tableView_->GetHwnd(), nullptr, FALSE);
            break;
        case ID_VIEW_STATS:
            ShowStatsDialog(hwnd_, doc_);
            break;
        case ID_VIEW_TIMING:
            ShowTimingDialog(hwnd_, doc_);
            break;
        case ID_THEME_TOKYONIGHT:
            SetCurrentTheme(ThemeId::TokyoNight);
            Settings::Instance().SetTheme("TokyoNight");
            InvalidateRect(hwnd_, nullptr, TRUE);
            break;
        case ID_THEME_TTY:
            SetCurrentTheme(ThemeId::TTY);
            Settings::Instance().SetTheme("TTY");
            InvalidateRect(hwnd_, nullptr, TRUE);
            break;
        case ID_NAV_FIND:
            if (hwndSearch_) {
                SetFocus(hwndSearch_);
                SendMessageW(hwndSearch_, EM_SETSEL, 0, -1);
            }
            break;
        case ID_NAV_FINDNEXT:
            RunSearch(true);
            break;
        case ID_NAV_FINDPREV:
            RunSearch(false);
            break;
        case ID_NAV_JUMPPAIR:
            JumpToMatchingPair();
            break;
        case ID_NAV_NEXTERROR:
            JumpToErrorRelative(+1);
            break;
        case ID_NAV_PREVERROR:
            JumpToErrorRelative(-1);
            break;
        case ID_HELP_ABOUT:
            MessageBoxW(hwnd_, L"Logram 1.0\nUnityBase Log Viewer\n\nWin32 + Direct2D",
                        L"About Logram", MB_OK | MB_ICONINFORMATION);
            break;
    }
}

void MainWindow::OnNotify(NMHDR* /*hdr*/) {
    // All ListView notifications are consumed by child windows
    // (FilterSidebar has its own WndProc). Nothing to do here.
}

void MainWindow::OnDropFiles(HDROP hDrop) {
    wchar_t path[MAX_PATH];
    if (DragQueryFileW(hDrop, 0, path, MAX_PATH) > 0) {
        LoadFile(path);
    }
    DragFinish(hDrop);
}

void MainWindow::OnDestroy() {
    // Splitter positions are not persisted — always start at defaults.
    if (loadThread_.joinable()) loadThread_.request_stop();
}

void MainWindow::RunSearch(bool forward) {
    wchar_t buf[512];
    GetWindowTextW(hwndSearch_, buf, 512);
    auto pattern = WideToUtf8(buf);
    if (pattern.empty()) return;

    auto dir = forward ? LogDocument::SearchDirection::Forward
                       : LogDocument::SearchDirection::Backward;
    int next = doc_.FindNext(pattern, dir, lastFoundIdx_);
    if (next >= 0) {
        lastFoundIdx_ = next;
        doc_.SetSelectedLineId(static_cast<int>(doc_.FilteredIndices()[next]));
        if (tableView_) {
            tableView_->ScrollToLine(next);
            tableView_->SelectLine(next);
        }
        DocumentChanges ch;
        ch.flags = DocumentChanges::SelectionChanged;
        doc_.listeners.Notify(ch);
    } else {
        MessageBeep(MB_ICONWARNING);
    }
}

void MainWindow::JumpToErrorRelative(int delta) {
    const auto& indices = doc_.FilteredIndices();
    const auto& lines   = doc_.AllLines();
    if (indices.empty()) return;

    // Starting position — the currently selected filtered row, else -1 for forward, end for backward.
    int startIdx = -1;
    int sel = doc_.SelectedLineId();
    if (sel >= 0) {
        for (size_t i = 0; i < indices.size(); ++i) {
            if (static_cast<int>(indices[i]) == sel) { startIdx = static_cast<int>(i); break; }
        }
    }

    auto isErr = [&](int filteredIdx) {
        auto& info = GetLogLevelInfo(
            static_cast<LogLevel>(lines[indices[filteredIdx]].level));
        return info.isError;
    };

    int total = static_cast<int>(indices.size());
    if (delta > 0) {
        for (int i = startIdx + 1; i < total; ++i) if (isErr(i)) {
            lastFoundIdx_ = i;
            doc_.SetSelectedLineId(static_cast<int>(indices[i]));
            if (tableView_) { tableView_->ScrollToLine(i); tableView_->SelectLine(i); }
            DocumentChanges ch; ch.flags = DocumentChanges::SelectionChanged;
            doc_.listeners.Notify(ch);
            return;
        }
    } else {
        int from = (startIdx < 0) ? total - 1 : startIdx - 1;
        for (int i = from; i >= 0; --i) if (isErr(i)) {
            lastFoundIdx_ = i;
            doc_.SetSelectedLineId(static_cast<int>(indices[i]));
            if (tableView_) { tableView_->ScrollToLine(i); tableView_->SelectLine(i); }
            DocumentChanges ch; ch.flags = DocumentChanges::SelectionChanged;
            doc_.listeners.Notify(ch);
            return;
        }
    }
    MessageBeep(MB_ICONWARNING);
}

void MainWindow::JumpToMatchingPair() {
    int sel = doc_.SelectedLineId();
    if (sel < 0) return;
    int pair = doc_.FindMatchingPair(sel);
    if (pair < 0) { MessageBeep(MB_ICONWARNING); return; }

    const auto& indices = doc_.FilteredIndices();
    for (size_t i = 0; i < indices.size(); ++i) {
        if (static_cast<int>(indices[i]) == pair) {
            doc_.SetSelectedLineId(pair);
            if (tableView_) { tableView_->ScrollToLine(static_cast<int>(i));
                              tableView_->SelectLine(static_cast<int>(i)); }
            DocumentChanges ch; ch.flags = DocumentChanges::SelectionChanged;
            doc_.listeners.Notify(ch);
            return;
        }
    }
    // Pair exists but filtered out — beep
    MessageBeep(MB_ICONWARNING);
}

void MainWindow::ShowOpenDialog() {
    IFileOpenDialog* pDialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&pDialog));
    if (FAILED(hr)) return;

    COMDLG_FILTERSPEC filters[] = {
        {L"Log Files", L"*.log;*.txt"},
        {L"All Files", L"*.*"},
    };
    pDialog->SetFileTypes(2, filters);
    pDialog->SetTitle(L"Open Log File");

    hr = pDialog->Show(hwnd_);
    if (SUCCEEDED(hr)) {
        IShellItem* pItem = nullptr;
        hr = pDialog->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            LPWSTR path = nullptr;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &path);
            if (SUCCEEDED(hr) && path) {
                LoadFile(path);
                CoTaskMemFree(path);
            }
            pItem->Release();
        }
    }
    pDialog->Release();
}

void MainWindow::LoadFile(const wchar_t* path) {
    if (loadThread_.joinable()) loadThread_.join();

    std::wstring pathStr = path;
    HWND hwnd = hwnd_;

    loadThread_ = std::jthread([this, pathStr, hwnd](std::stop_token) {
        bool ok = doc_.Load(pathStr.c_str(), [](double) {});
        if (ok) {
            Settings::Instance().AddRecentFile(pathStr);
            PostMessageW(hwnd, WM_APP_DOC_LOADED, 0, 0);
        }
    });
}

void MainWindow::UpdateTitle() {
    std::wstring title = L"Logram";
    auto name = doc_.FileName();
    if (!name.empty()) {
        title += L" - " + Utf8ToWide(name);
    }
    SetWindowTextW(hwnd_, title.c_str());
}

void MainWindow::UpdateStatusBar() {
    if (!hwndStatus_) return;

    auto name = doc_.FileName();
    SendMessageW(hwndStatus_, SB_SETTEXTW, 0,
                 reinterpret_cast<LPARAM>(Utf8ToWide(name).c_str()));

    uint64_t size = doc_.FileSize();
    wchar_t buf[64];
    if (size >= 1'000'000'000) {
        swprintf(buf, 64, L"%.1f GB", static_cast<double>(size) / 1'000'000'000.0);
    } else if (size >= 1'000'000) {
        swprintf(buf, 64, L"%.1f MB", static_cast<double>(size) / 1'000'000.0);
    } else {
        swprintf(buf, 64, L"%.1f KB", static_cast<double>(size) / 1'000.0);
    }
    SendMessageW(hwndStatus_, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(buf));

    swprintf(buf, 64, L"%d / %d lines", doc_.FilteredCount(), doc_.TotalEvents());
    SendMessageW(hwndStatus_, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(buf));

    int errors = doc_.ErrorCount();
    if (errors > 0) {
        swprintf(buf, 64, L"%d errors", errors);
    } else {
        swprintf(buf, 64, L"No errors");
    }
    SendMessageW(hwndStatus_, SB_SETTEXTW, 3, reinterpret_cast<LPARAM>(buf));

    auto dur = doc_.DurationFormatted();
    SendMessageW(hwndStatus_, SB_SETTEXTW, 4,
                 reinterpret_cast<LPARAM>(Utf8ToWide(dur).c_str()));
}

void MainWindow::OnDocumentChanged(DocumentChanges changes) {
    if (changes.Has(DocumentChanges::DataLoaded) ||
        changes.Has(DocumentChanges::FiltersChanged)) {
        UpdateStatusBar();
    }
}