#include "ui/FilterSidebar.h"
#include "ui/ThemeColors.h"
#include "infra/Utf.h"
#include <commctrl.h>

FilterSidebar::FilterSidebar() {}
FilterSidebar::~FilterSidebar() {
    if (doc_) doc_->listeners.Remove(this);
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

    hwnd_ = CreateWindowExW(0, kClassName, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 200, 400, parent, nullptr, hInstance, this);

    CreatePresetButtons(hInstance);

    hwndList_ = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
        0, kPresetBarHeight, 200, 400 - kPresetBarHeight,
        hwnd_, nullptr, hInstance, nullptr);

    ListView_SetExtendedListViewStyle(hwndList_,
        LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

    LVCOLUMNW col = {};
    col.mask = LVCF_WIDTH;
    col.cx = 190;
    ListView_InsertColumn(hwndList_, 0, &col);

    RebuildList();
    return hwnd_;
}

void FilterSidebar::CreatePresetButtons(HINSTANCE hInstance) {
    const wchar_t* labels[5] = {L"All", L"Err", L"SQL", L"HTTP", L"+/-"};
    int xBase = 2;
    int btnW = 38;
    for (int i = 0; i < 5; ++i) {
        hwndPreset_[i] = CreateWindowExW(0, L"BUTTON", labels[i],
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            xBase + i * (btnW + 2), 2, btnW, kPresetBarHeight - 6,
            hwnd_, reinterpret_cast<HMENU>(static_cast<LONG_PTR>(kPresetButtonBase + i)),
            hInstance, nullptr);
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

void FilterSidebar::LayoutInternal() {
    if (hwndList_) {
        MoveWindow(hwndList_, 0, kPresetBarHeight,
                   clientW_, std::max(0, clientH_ - kPresetBarHeight), TRUE);
        // Stretch the single column
        ListView_SetColumnWidth(hwndList_, 0, std::max(50, clientW_ - 4));
    }
}

void FilterSidebar::RebuildList() {
    if (!hwndList_) return;
    suppressNotify_ = true;
    ListView_DeleteAllItems(hwndList_);

    // 1) Level rows
    uint64_t levelMask = doc_ ? doc_->EnabledLevelMask() : ~uint64_t(0);
    for (int i = 0; i < kLogLevelCount; ++i) {
        auto& info = GetLogLevelInfo(static_cast<LogLevel>(i));
        auto wlabel = Utf8ToWide(info.label);
        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.lParam = static_cast<LPARAM>(i);  // identify level rows
        item.pszText = const_cast<LPWSTR>(wlabel.c_str());
        ListView_InsertItem(hwndList_, &item);
        bool on = (levelMask >> i) & 1;
        ListView_SetCheckState(hwndList_, i, on);
    }

    // 2) Separator + Thread rows
    threadRowBase_ = kLogLevelCount;
    if (doc_) {
        const auto& active = doc_->ActiveThreads();
        uint64_t thMask = doc_->EnabledThreadMask();
        int row = threadRowBase_;
        for (int t : active) {
            wchar_t buf[32];
            swprintf(buf, 32, L"T%d", t);
            LVITEMW item = {};
            item.mask = LVIF_TEXT | LVIF_PARAM;
            item.iItem = row;
            item.lParam = static_cast<LPARAM>(1000 + t);  // identify thread rows
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
    uint64_t levelMask = 0;
    for (int i = 0; i < kLogLevelCount; ++i) {
        if (ListView_GetCheckState(hwndList_, i)) {
            levelMask |= (uint64_t(1) << i);
        }
    }
    uint64_t thMask = 0;
    const auto& active = doc_->ActiveThreads();
    for (size_t k = 0; k < active.size(); ++k) {
        int row = threadRowBase_ + static_cast<int>(k);
        if (ListView_GetCheckState(hwndList_, row)) {
            thMask |= (uint64_t(1) << active[k]);
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
    for (int i = 0; i < kLogLevelCount; ++i) {
        ListView_SetCheckState(hwndList_, i, (levelMask >> i) & 1);
    }
    suppressNotify_ = false;

    doc_->SetEnabledLevelMask(levelMask);
    doc_->SetEnabledThreadMask(~uint64_t(0));
    doc_->ApplyFilters();
    DocumentChanges changes;
    changes.flags = DocumentChanges::FiltersChanged;
    doc_->listeners.Notify(changes);
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

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id >= kPresetButtonBase && id < kPresetButtonBase + 5) {
                ApplyPreset(static_cast<Preset>(id - kPresetButtonBase));
            }
            return 0;
        }

        case WM_NOTIFY: {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (hdr && hdr->hwndFrom == hwndList_ && hdr->code == LVN_ITEMCHANGED) {
                auto* ch = reinterpret_cast<NMLISTVIEW*>(lParam);
                // Check-state change flips the upper bits of stateMask (LVIS_STATEIMAGEMASK)
                if (!suppressNotify_ &&
                    (ch->uChanged & LVIF_STATE) &&
                    ((ch->uOldState ^ ch->uNewState) & LVIS_STATEIMAGEMASK)) {
                    ReadCheckStatesIntoDoc();
                }
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}