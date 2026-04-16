#include "ui/TimingDialog.h"
#include "infra/Utf.h"
#include <commctrl.h>
#include <cstdio>
#include <algorithm>

struct TimingDlgData {
    LogDocument* doc;
    std::vector<const MethodTiming*> sorted;
    int sortCol = 0;     // 0=duration, 1=thread, 2=method
    bool sortAsc = false; // default: descending by duration
};

static void SortTimings(TimingDlgData* data) {
    std::sort(data->sorted.begin(), data->sorted.end(),
        [&](const MethodTiming* a, const MethodTiming* b) {
            int cmp = 0;
            switch (data->sortCol) {
                case 0: cmp = (a->durationMS < b->durationMS) ? -1 :
                              (a->durationMS > b->durationMS) ? 1 : 0; break;
                case 1: cmp = a->thread - b->thread; break;
                case 2: cmp = a->method.compare(b->method); break;
            }
            return data->sortAsc ? (cmp < 0) : (cmp > 0);
        });
}

static INT_PTR CALLBACK TimingDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* data = reinterpret_cast<TimingDlgData*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_INITDIALOG: {
            auto* doc = reinterpret_cast<LogDocument*>(lParam);
            auto* d = new TimingDlgData();
            d->doc = doc;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

            doc->BuildMethodTimings();
            const auto& timings = doc->Timings();
            d->sorted.reserve(timings.size());
            for (const auto& t : timings) d->sorted.push_back(&t);
            SortTimings(d);

            HWND hList = GetDlgItem(hwnd, 101);
            if (!hList) break;
            ListView_SetExtendedListViewStyle(hList,
                LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

            LVCOLUMNW col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
            col.fmt = LVCFMT_RIGHT;
            col.pszText = const_cast<LPWSTR>(L"Duration");
            col.cx = 100;
            ListView_InsertColumn(hList, 0, &col);
            col.fmt = LVCFMT_CENTER;
            col.pszText = const_cast<LPWSTR>(L"Thread");
            col.cx = 60;
            ListView_InsertColumn(hList, 1, &col);
            col.fmt = LVCFMT_LEFT;
            col.pszText = const_cast<LPWSTR>(L"Method");
            col.cx = 400;
            ListView_InsertColumn(hList, 2, &col);

            ListView_SetItemCountEx(hList, static_cast<int>(d->sorted.size()),
                                    LVSICF_NOSCROLL);

            wchar_t title[64];
            swprintf(title, 64, L"Method Timing (%zu methods)", timings.size());
            SetWindowTextW(hwnd, title);
            return TRUE;
        }

        case WM_NOTIFY: {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (!hdr || hdr->idFrom != 101) break;

            if (hdr->code == LVN_GETDISPINFOW && data) {
                auto* di = reinterpret_cast<NMLVDISPINFOW*>(lParam);
                int idx = di->item.iItem;
                if (idx < 0 || idx >= static_cast<int>(data->sorted.size())) break;
                const auto* t = data->sorted[idx];
                static wchar_t buf[512];

                if (di->item.mask & LVIF_TEXT) {
                    switch (di->item.iSubItem) {
                        case 0:
                            swprintf(buf, 512, L"%.3f ms", t->durationMS);
                            di->item.pszText = buf;
                            break;
                        case 1:
                            swprintf(buf, 512, L"T%d", t->thread + 1);
                            di->item.pszText = buf;
                            break;
                        case 2: {
                            auto w = Utf8ToWide(t->method);
                            wcsncpy(buf, w.c_str(), 511);
                            buf[511] = L'\0';
                            di->item.pszText = buf;
                            break;
                        }
                    }
                }
                return TRUE;
            }

            if (hdr->code == LVN_COLUMNCLICK && data) {
                auto* nm = reinterpret_cast<NMLISTVIEW*>(lParam);
                if (nm->iSubItem == data->sortCol)
                    data->sortAsc = !data->sortAsc;
                else {
                    data->sortCol = nm->iSubItem;
                    data->sortAsc = (nm->iSubItem == 2); // method asc, duration desc
                }
                SortTimings(data);
                HWND hList = GetDlgItem(hwnd, 101);
                if (hList) InvalidateRect(hList, nullptr, FALSE);
                return TRUE;
            }

            if (hdr->code == NM_DBLCLK && data) {
                auto* nm = reinterpret_cast<NMITEMACTIVATE*>(lParam);
                if (nm->iItem >= 0 && nm->iItem < static_cast<int>(data->sorted.size())) {
                    const auto* t = data->sorted[nm->iItem];
                    data->doc->SetSelectedLineId(static_cast<int>(t->lineId));
                    DocumentChanges ch;
                    ch.flags = DocumentChanges::SelectionChanged;
                    data->doc->listeners.Notify(ch);
                }
                return TRUE;
            }
            break;
        }

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            HWND hList = GetDlgItem(hwnd, 101);
            if (hList) MoveWindow(hList, 0, 0, rc.right, rc.bottom, TRUE);
            return TRUE;
        }

        case WM_CLOSE:
        case WM_COMMAND:
            if (msg == WM_CLOSE || LOWORD(wParam) == IDCANCEL) {
                if (data) { delete data; SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); }
                DestroyWindow(hwnd);
                return TRUE;
            }
            break;

        case WM_DESTROY:
            if (data) { delete data; SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); }
            return TRUE;
    }
    return FALSE;
}

void ShowTimingDialog(HWND parent, LogDocument& doc) {
    alignas(4) char tmpl[1024] = {};
    auto* dt = reinterpret_cast<DLGTEMPLATE*>(tmpl);
    dt->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | DS_CENTER;
    dt->cdit = 1;
    dt->cx = 320;
    dt->cy = 220;

    auto* ptr = reinterpret_cast<WORD*>(dt + 1);
    *ptr++ = 0; // menu
    *ptr++ = 0; // class
    const wchar_t* title = L"Method Timing";
    size_t titleLen = wcslen(title) + 1;
    memcpy(ptr, title, titleLen * sizeof(WORD));
    ptr += titleLen;

    ptr = reinterpret_cast<WORD*>((reinterpret_cast<uintptr_t>(ptr) + 3) & ~uintptr_t(3));

    // Virtual ListView
    auto* di1 = reinterpret_cast<DLGITEMTEMPLATE*>(ptr);
    di1->style = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDATA;
    di1->x = 0; di1->y = 0; di1->cx = 320; di1->cy = 220;
    di1->id = 101;
    ptr = reinterpret_cast<WORD*>(di1 + 1);
    const wchar_t* lvClass = L"SysListView32";
    size_t lvLen = wcslen(lvClass) + 1;
    memcpy(ptr, lvClass, lvLen * sizeof(WORD));
    ptr += lvLen;
    *ptr++ = 0;
    *ptr++ = 0;

    // Modeless dialog: user can interact with main window while timing is open.
    CreateDialogIndirectParamW(GetModuleHandleW(nullptr),
        dt, parent, TimingDlgProc,
        reinterpret_cast<LPARAM>(&doc));
}