#include "ui/FilterSidebar.h"
#include "ui/ThemeColors.h"
#include "infra/Utf.h"
#include <commctrl.h>
#include <windowsx.h>

FilterSidebar::FilterSidebar() {}
FilterSidebar::~FilterSidebar() {
    if (doc_) doc_->listeners.Remove(this);
    if (hFont_) DeleteObject(hFont_);
}

void FilterSidebar::RegisterClass(HINSTANCE hInstance) {
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

HWND FilterSidebar::Create(HWND parent, HINSTANCE hInstance, LogDocument* doc) {
    doc_ = doc;
    if (doc_) doc_->listeners.Add(this);

    UINT dpi = GetDpiForWindow(parent);
    if (dpi == 0) dpi = 96;
    dpiScale_ = dpi / 96.0f;

    hwnd_ = CreateWindowExW(0, kClassName, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, Scale(200), Scale(400), parent, nullptr, hInstance, this);

    // DPI-scaled UI font shared by preset buttons and the list.
    hFont_ = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI");

    CreatePresetButtons(hInstance);

    hwndList_ = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
        0, Scale(kPresetBarHeightDip), Scale(200),
        Scale(400 - kPresetBarHeightDip),
        hwnd_, nullptr, hInstance, nullptr);

    ListView_SetExtendedListViewStyle(hwndList_,
        LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    // Dark background for the entire ListView surface.
    auto& theme = CurrentTheme();
    ListView_SetBkColor(hwndList_, ToCOLORREF(theme.background));
    ListView_SetTextBkColor(hwndList_, ToCOLORREF(theme.background));
    ListView_SetTextColor(hwndList_, ToCOLORREF(theme.foreground));

    if (hFont_) SendMessageW(hwndList_, WM_SETFONT,
                             reinterpret_cast<WPARAM>(hFont_), TRUE);

    LVCOLUMNW col = {};
    col.mask = LVCF_WIDTH;
    col.cx = Scale(190);
    ListView_InsertColumn(hwndList_, 0, &col);

    RebuildList();
    return hwnd_;
}

void FilterSidebar::CreatePresetButtons(HINSTANCE hInstance) {
    const wchar_t* labels[5] = {L"All", L"Err", L"SQL", L"HTTP", L"+/-"};
    int xBase = Scale(4);
    int btnW  = Scale(44);
    int btnH  = Scale(kPresetBarHeightDip - 6);
    int btnY  = Scale(3);
    int gap   = Scale(3);
    for (int i = 0; i < 5; ++i) {
        hwndPreset_[i] = CreateWindowExW(0, L"BUTTON", labels[i],
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            xBase + i * (btnW + gap), btnY, btnW, btnH,
            hwnd_, reinterpret_cast<HMENU>(static_cast<LONG_PTR>(kPresetButtonBase + i)),
            hInstance, nullptr);
        if (hFont_) SendMessageW(hwndPreset_[i], WM_SETFONT,
                                 reinterpret_cast<WPARAM>(hFont_), TRUE);
    }
}

void FilterSidebar::SetDocument(LogDocument* doc) {
    if (doc_) doc_->listeners.Remove(this);
    doc_ = doc;
    if (doc_) doc_->listeners.Add(this);
    RebuildList();
}

void FilterSidebar::Resize(int w, int h) {
    clientW_ = w;
    clientH_ = h;
    LayoutInternal();
}

void FilterSidebar::OnThemeChanged() {
    auto& theme = CurrentTheme();
    if (hwndList_) {
        ListView_SetBkColor(hwndList_, ToCOLORREF(theme.background));
        ListView_SetTextBkColor(hwndList_, ToCOLORREF(theme.background));
        ListView_SetTextColor(hwndList_, ToCOLORREF(theme.foreground));
    }
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

void FilterSidebar::LayoutInternal() {
    const int barH = Scale(kPresetBarHeightDip);
    if (hwndList_) {
        MoveWindow(hwndList_, 0, barH,
                   clientW_, std::max(0, clientH_ - barH), TRUE);
        ListView_SetColumnWidth(hwndList_, 0, std::max(Scale(50), clientW_ - Scale(4)));
    }
}

void FilterSidebar::RebuildList() {
    if (!hwndList_) return;
    suppressNotify_ = true;
    ListView_DeleteAllItems(hwndList_);

    // Enable group view for "Log Levels" / "Threads" headers.
    ListView_EnableGroupView(hwndList_, TRUE);

    LVGROUP grp = {};
    grp.cbSize = sizeof(grp);
    grp.mask = LVGF_HEADER | LVGF_GROUPID;
    grp.pszHeader = const_cast<LPWSTR>(L"Log Levels");
    grp.iGroupId = 1;
    ListView_InsertGroup(hwndList_, -1, &grp);

    grp.pszHeader = const_cast<LPWSTR>(L"Threads");
    grp.iGroupId = 2;
    ListView_InsertGroup(hwndList_, -1, &grp);

    // 1) Level rows — only levels present in the loaded file
    uint64_t levelMask = doc_ ? doc_->EnabledLevelMask() : ~uint64_t(0);
    const int* counts = doc_ ? doc_->PerLevelCount() : nullptr;
    int row = 0;
    for (int i = 0; i < kLogLevelCount; ++i) {
        if (counts && counts[i] == 0) continue;  // skip absent levels
        auto& info = GetLogLevelInfo(static_cast<LogLevel>(i));
        auto wlabel = Utf8ToWide(info.label);
        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_GROUPID;
        item.iItem = row;
        item.iGroupId = 1;
        item.lParam = static_cast<LPARAM>(i);
        item.pszText = const_cast<LPWSTR>(wlabel.c_str());
        ListView_InsertItem(hwndList_, &item);
        bool on = (levelMask >> i) & 1;
        ListView_SetCheckState(hwndList_, row, on);
        row++;
    }

    // 2) Thread rows
    threadRowBase_ = row;
    if (doc_) {
        const auto& active = doc_->ActiveThreads();
        uint64_t thMask = doc_->EnabledThreadMask();
        int row = threadRowBase_;
        for (int t : active) {
            wchar_t buf[32];
            wchar_t thChar = static_cast<wchar_t>(t + 0x21); // !, ", #, $, %, &...
            swprintf(buf, 32, L"%c  Thread %d", thChar, t + 1);
            LVITEMW item = {};
            item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_GROUPID;
            item.iItem = row;
            item.iGroupId = 2;
            item.lParam = static_cast<LPARAM>(1000 + t);
            item.pszText = buf;
            ListView_InsertItem(hwndList_, &item);
            bool on = (thMask >> t) & 1;
            ListView_SetCheckState(hwndList_, row, on);
            row++;
        }
    }

    suppressNotify_ = false;
}

void FilterSidebar::ReadCheckStatesIntoDoc() {
    if (!doc_ || !hwndList_) return;
    int total = ListView_GetItemCount(hwndList_);

    // Start with all levels enabled, then read visible checkboxes.
    // Absent levels (not in list) stay enabled so their lines aren't hidden.
    uint64_t levelMask = ~uint64_t(0);
    uint64_t thMask = 0;

    for (int row = 0; row < total; ++row) {
        LVITEMW li = {};
        li.mask = LVIF_PARAM;
        li.iItem = row;
        ListView_GetItem(hwndList_, &li);
        LPARAM lp = li.lParam;
        bool checked = ListView_GetCheckState(hwndList_, row) != 0;

        if (lp < 1000) {
            // Level row
            int lvl = static_cast<int>(lp);
            if (checked)
                levelMask |= (uint64_t(1) << lvl);
            else
                levelMask &= ~(uint64_t(1) << lvl);
        } else {
            // Thread row
            int t = static_cast<int>(lp) - 1000;
            if (checked) thMask |= (uint64_t(1) << t);
        }
    }

    doc_->SetEnabledLevelMask(levelMask);
    doc_->SetEnabledThreadMask(thMask);
    doc_->ApplyFilters();
    DocumentChanges changes;
    changes.flags = DocumentChanges::FiltersChanged;
    doc_->listeners.Notify(changes);
}

void FilterSidebar::OnDocumentChanged(DocumentChanges changes) {
    if (changes.Has(DocumentChanges::DataLoaded)) {
        RebuildList();
    } else if (changes.Has(DocumentChanges::FiltersChanged) && doc_ && hwndList_) {
        // Sync checkboxes with current masks (e.g. after Method Timing "Go to Line")
        suppressNotify_ = true;
        uint64_t levelMask = doc_->EnabledLevelMask();
        uint64_t thMask = doc_->EnabledThreadMask();
        int total = ListView_GetItemCount(hwndList_);
        for (int row = 0; row < total; ++row) {
            LVITEMW li = {};
            li.mask = LVIF_PARAM;
            li.iItem = row;
            ListView_GetItem(hwndList_, &li);
            if (li.lParam < 1000) {
                ListView_SetCheckState(hwndList_, row, (levelMask >> li.lParam) & 1);
            } else {
                int t = static_cast<int>(li.lParam) - 1000;
                ListView_SetCheckState(hwndList_, row, (thMask >> t) & 1);
            }
        }
        suppressNotify_ = false;
    }
}

void FilterSidebar::ApplyPreset(Preset p) {
    if (!doc_ || !hwndList_) return;

    uint64_t levelMask = 0;
    switch (p) {
        case PresetAll:
            levelMask = ~uint64_t(0);
            break;
        case PresetErrors:
            levelMask =
                (uint64_t(1) << static_cast<int>(LogLevel::Error)) |
                (uint64_t(1) << static_cast<int>(LogLevel::Exc))   |
                (uint64_t(1) << static_cast<int>(LogLevel::ExcOs)) |
                (uint64_t(1) << static_cast<int>(LogLevel::OsErr)) |
                (uint64_t(1) << static_cast<int>(LogLevel::Fail))  |
                (uint64_t(1) << static_cast<int>(LogLevel::DddER));
            break;
        case PresetSql:
            levelMask =
                (uint64_t(1) << static_cast<int>(LogLevel::Sql))   |
                (uint64_t(1) << static_cast<int>(LogLevel::Cust1)) |
                (uint64_t(1) << static_cast<int>(LogLevel::Cust2));
            break;
        case PresetHttp:
            levelMask =
                (uint64_t(1) << static_cast<int>(LogLevel::Http))  |
                (uint64_t(1) << static_cast<int>(LogLevel::Clnt))  |
                (uint64_t(1) << static_cast<int>(LogLevel::Srvr));
            break;
        case PresetCallPairs:
            levelMask =
                (uint64_t(1) << static_cast<int>(LogLevel::Enter)) |
                (uint64_t(1) << static_cast<int>(LogLevel::Leave));
            break;
    }

    suppressNotify_ = true;
    int total = ListView_GetItemCount(hwndList_);
    for (int row = 0; row < total; ++row) {
        LVITEMW li = {};
        li.mask = LVIF_PARAM;
        li.iItem = row;
        ListView_GetItem(hwndList_, &li);
        if (li.lParam < 1000) {
            // Level row — set check based on mask
            ListView_SetCheckState(hwndList_, row, (levelMask >> li.lParam) & 1);
        } else {
            // Thread row — preset enables all threads
            ListView_SetCheckState(hwndList_, row, TRUE);
        }
    }
    suppressNotify_ = false;

    doc_->SetEnabledLevelMask(levelMask);
    doc_->SetEnabledThreadMask(~uint64_t(0));
    doc_->ApplyFilters();
    DocumentChanges changes;
    changes.flags = DocumentChanges::FiltersChanged;
    doc_->listeners.Notify(changes);
}

LRESULT FilterSidebar::OnCustomDraw(LPNMLVCUSTOMDRAW cd) {
    switch (cd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT: {
            LPARAM lp = cd->nmcd.lItemlParam;
            auto& theme = CurrentTheme();

            if (lp < 1000) {
                // Level row — set text to the level badge color.
                auto level = static_cast<LogLevel>(lp);
                auto c = theme.levelBadge[static_cast<int>(level)];
                cd->clrText = RGB(static_cast<BYTE>(c.r * 255),
                                  static_cast<BYTE>(c.g * 255),
                                  static_cast<BYTE>(c.b * 255));
            } else {
                // Thread row — use the thread color palette.
                int threadIdx = static_cast<int>(lp) - 1000;
                auto c = ThreadColor(threadIdx);
                cd->clrText = RGB(static_cast<BYTE>(c.r * 255),
                                  static_cast<BYTE>(c.g * 255),
                                  static_cast<BYTE>(c.b * 255));
            }

            cd->clrTextBk = RGB(static_cast<BYTE>(theme.background.r * 255),
                                static_cast<BYTE>(theme.background.g * 255),
                                static_cast<BYTE>(theme.background.b * 255));
            return CDRF_NEWFONT;
        }
    }
    return CDRF_DODEFAULT;
}

LRESULT CALLBACK FilterSidebar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FilterSidebar* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<FilterSidebar*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<FilterSidebar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT FilterSidebar::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            clientW_ = LOWORD(lParam);
            clientH_ = HIWORD(lParam);
            LayoutInternal();
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

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id >= kPresetButtonBase && id < kPresetButtonBase + 5) {
                ApplyPreset(static_cast<Preset>(id - kPresetButtonBase));
            }
            return 0;
        }

        case WM_NOTIFY: {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (!hdr || hdr->hwndFrom != hwndList_) break;

            if (hdr->code == LVN_ITEMCHANGED) {
                auto* ch = reinterpret_cast<NMLISTVIEW*>(lParam);
                if (!suppressNotify_ &&
                    (ch->uChanged & LVIF_STATE) &&
                    ((ch->uOldState ^ ch->uNewState) & LVIS_STATEIMAGEMASK)) {
                    ReadCheckStatesIntoDoc();
                }
                return 0;
            }

            if (hdr->code == NM_CUSTOMDRAW) {
                auto result = OnCustomDraw(reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam));
                SetWindowLongPtrW(hwnd_, DWLP_MSGRESULT, result);
                return result;
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}