#include "ui/TimingDialog.h"
#include "ui/ThemeColors.h"
#include "infra/Utf.h"
#include <commctrl.h>
#include <cstdio>
#include <algorithm>

static constexpr int IDC_LIST = 101;
static constexpr int IDC_GOTO = 102;

struct TimingDlgData {
    LogDocument* doc;
    std::vector<const MethodTiming*> sorted;
    int sortCol = 2;      // default sort by Duration
    bool sortAsc = false;
    HWND hwndList = nullptr;
    HWND hwndGoto = nullptr;
    HWND hwndStatus = nullptr;
    HFONT hFont = nullptr;
    HBRUSH hBgBrush = nullptr;
};

static void SortTimings(TimingDlgData* data) {
    std::sort(data->sorted.begin(), data->sorted.end(),
        [&](const MethodTiming* a, const MethodTiming* b) {
            int cmp = 0;
            switch (data->sortCol) {
                case 0: cmp = (a->lineId < b->lineId) ? -1 : (a->lineId > b->lineId) ? 1 : 0; break;
                case 1: cmp = a->thread - b->thread; break;
                case 2: cmp = (a->durationMS < b->durationMS) ? -1 :
                              (a->durationMS > b->durationMS) ? 1 : 0; break;
                case 3: cmp = a->method.compare(b->method); break;
            }
            return data->sortAsc ? (cmp < 0) : (cmp > 0);
        });
}

static void GoToSelected(HWND hwnd, TimingDlgData* data) {
    if (!data || !data->hwndList || !data->doc) return;
    int sel = ListView_GetNextItem(data->hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(data->sorted.size())) return;

    const auto* t = data->sorted[sel];

    // Filter to show only this thread (like Mac version)
    data->doc->SetEnabledThreadMask(uint64_t(1) << t->thread);
    data->doc->ApplyFilters();
    data->doc->SetSelectedLineId(static_cast<int>(t->lineId));

    // Notify filters changed + selection — listeners will scroll to line
    DocumentChanges ch;
    ch.flags = DocumentChanges::FiltersChanged | DocumentChanges::SelectionChanged;
    data->doc->listeners.Notify(ch);

    // Focus the main window so the user sees the result
    HWND parent = GetParent(hwnd);
    if (parent) SetForegroundWindow(parent);

    SendMessageW(hwnd, WM_CLOSE, 0, 0);
}

static LRESULT CALLBACK TimingWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* data = reinterpret_cast<TimingDlgData*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* doc = static_cast<LogDocument*>(cs->lpCreateParams);
            auto* d = new TimingDlgData();
            d->doc = doc;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

            UINT dpi = GetDpiForWindow(hwnd);
            if (dpi == 0) dpi = 96;
            d->hFont = CreateFontW(-MulDiv(11, dpi, 72), 0, 0, 0, FW_NORMAL,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH | FF_SWISS, L"Segoe UI");

            auto& theme = CurrentTheme();
            d->hBgBrush = CreateSolidBrush(ToCOLORREF(theme.background));

            HINSTANCE hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));

            d->hwndList = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDATA | LVS_SHOWSELALWAYS,
                0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(IDC_LIST), hInst, nullptr);
            ListView_SetExtendedListViewStyle(d->hwndList,
                LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            ListView_SetBkColor(d->hwndList, ToCOLORREF(theme.background));
            ListView_SetTextBkColor(d->hwndList, ToCOLORREF(theme.background));
            ListView_SetTextColor(d->hwndList, ToCOLORREF(theme.foreground));
            if (d->hFont) SendMessageW(d->hwndList, WM_SETFONT,
                                       reinterpret_cast<WPARAM>(d->hFont), TRUE);

            LVCOLUMNW col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
            col.fmt = LVCFMT_RIGHT;
            col.pszText = const_cast<LPWSTR>(L"#");
            col.cx = MulDiv(70, dpi, 96);
            ListView_InsertColumn(d->hwndList, 0, &col);
            col.fmt = LVCFMT_CENTER;
            col.pszText = const_cast<LPWSTR>(L"Thread");
            col.cx = MulDiv(50, dpi, 96);
            ListView_InsertColumn(d->hwndList, 1, &col);
            col.fmt = LVCFMT_RIGHT;
            col.pszText = const_cast<LPWSTR>(L"Duration");
            col.cx = MulDiv(100, dpi, 96);
            ListView_InsertColumn(d->hwndList, 2, &col);
            col.fmt = LVCFMT_LEFT;
            col.pszText = const_cast<LPWSTR>(L"Method");
            col.cx = MulDiv(430, dpi, 96);
            ListView_InsertColumn(d->hwndList, 3, &col);

            d->hwndGoto = CreateWindowExW(0, L"BUTTON", L"Go to Line",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                0, 0, MulDiv(90, dpi, 96), MulDiv(28, dpi, 96),
                hwnd, reinterpret_cast<HMENU>(IDC_GOTO), hInst, nullptr);
            if (d->hFont) SendMessageW(d->hwndGoto, WM_SETFONT,
                                       reinterpret_cast<WPARAM>(d->hFont), TRUE);

            d->hwndStatus = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                0, 0, 100, MulDiv(28, dpi, 96),
                hwnd, nullptr, hInst, nullptr);
            if (d->hFont) SendMessageW(d->hwndStatus, WM_SETFONT,
                                       reinterpret_cast<WPARAM>(d->hFont), TRUE);

            // Build timings and populate
            doc->BuildMethodTimings();
            const auto& timings = doc->Timings();
            d->sorted.reserve(timings.size());
            for (const auto& t : timings) d->sorted.push_back(&t);
            SortTimings(d);

            ListView_SetItemCountEx(d->hwndList, static_cast<int>(d->sorted.size()),
                                    LVSICF_NOSCROLL);

            wchar_t title[128];
            swprintf(title, 128, L"Method Timing — %zu methods >= 10ms", timings.size());
            SetWindowTextW(hwnd, title);

            wchar_t status[64];
            swprintf(status, 64, L"%zu methods", timings.size());
            SetWindowTextW(d->hwndStatus, status);

            return 0;
        }

        case WM_SIZE: {
            if (!data) break;
            RECT rc;
            GetClientRect(hwnd, &rc);
            UINT dpi = GetDpiForWindow(hwnd);
            if (dpi == 0) dpi = 96;
            int barH = MulDiv(36, dpi, 96);
            int pad = MulDiv(6, dpi, 96);
            int btnW = MulDiv(90, dpi, 96);
            int btnH = MulDiv(26, dpi, 96);

            if (data->hwndList)
                MoveWindow(data->hwndList, 0, 0, rc.right, rc.bottom - barH, TRUE);
            if (data->hwndGoto)
                MoveWindow(data->hwndGoto, pad, rc.bottom - barH + pad/2, btnW, btnH, TRUE);
            if (data->hwndStatus)
                MoveWindow(data->hwndStatus, btnW + 2*pad, rc.bottom - barH + pad/2,
                           rc.right - btnW - 3*pad, btnH, TRUE);
            return 0;
        }

        case WM_DRAWITEM: {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (dis && dis->CtlType == ODT_BUTTON) {
                DrawThemedButton(dis);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            if (!data) break;
            auto& theme = CurrentTheme();
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, ToCOLORREF(theme.foreground));
            SetBkColor(hdc, ToCOLORREF(theme.background));
            return reinterpret_cast<LRESULT>(data->hBgBrush);
        }

        case WM_ERASEBKGND: {
            auto& theme = CurrentTheme();
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(ToCOLORREF(theme.background));
            FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
            DeleteObject(brush);
            return 1;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_GOTO) {
                GoToSelected(hwnd, data);
                return 0;
            }
            break;

        case WM_NOTIFY: {
            auto* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (!hdr || !data || hdr->idFrom != IDC_LIST) break;

            if (hdr->code == LVN_GETDISPINFOW) {
                auto* di = reinterpret_cast<NMLVDISPINFOW*>(lParam);
                int idx = di->item.iItem;
                if (idx < 0 || idx >= static_cast<int>(data->sorted.size())) break;
                const auto* t = data->sorted[idx];
                static thread_local wchar_t buf[512];

                if (di->item.mask & LVIF_TEXT) {
                    switch (di->item.iSubItem) {
                        case 0:
                            swprintf(buf, 512, L"%u", t->lineId + 1);
                            di->item.pszText = buf;
                            break;
                        case 1: {
                            wchar_t ch = static_cast<wchar_t>(t->thread + 0x21);
                            swprintf(buf, 512, L"%c", ch);
                            di->item.pszText = buf;
                            break;
                        }
                        case 2:
                            if (t->durationMS >= 1000.0)
                                swprintf(buf, 512, L"%.1f s", t->durationMS / 1000.0);
                            else
                                swprintf(buf, 512, L"%.0f ms", t->durationMS);
                            di->item.pszText = buf;
                            break;
                        case 3: {
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

            if (hdr->code == LVN_COLUMNCLICK) {
                auto* nm = reinterpret_cast<NMLISTVIEW*>(lParam);
                if (nm->iSubItem == data->sortCol)
                    data->sortAsc = !data->sortAsc;
                else {
                    data->sortCol = nm->iSubItem;
                    data->sortAsc = (nm->iSubItem == 3); // method asc, others desc
                }
                SortTimings(data);
                InvalidateRect(data->hwndList, nullptr, FALSE);
                return TRUE;
            }

            if (hdr->code == NM_DBLCLK) {
                GoToSelected(hwnd, data);
                return TRUE;
            }
            break;
        }

        case WM_CLOSE:
            if (data) {
                if (data->hFont) DeleteObject(data->hFont);
                if (data->hBgBrush) DeleteObject(data->hBgBrush);
                delete data;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (data) {
                if (data->hFont) DeleteObject(data->hFont);
                if (data->hBgBrush) DeleteObject(data->hBgBrush);
                delete data;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static const wchar_t* kTimingClassName = L"LogramTimingWindow";
static bool g_timingClassRegistered = false;

void ShowTimingDialog(HWND parent, LogDocument& doc) {
    if (!g_timingClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = TimingWndProc;
        wc.hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kTimingClassName;
        RegisterClassExW(&wc);
        g_timingClassRegistered = true;
    }

    UINT dpi = GetDpiForWindow(parent);
    if (dpi == 0) dpi = 96;
    int w = MulDiv(750, dpi, 96);
    int h = MulDiv(500, dpi, 96);

    CreateWindowExW(0, kTimingClassName, L"Method Timing",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        parent, nullptr,
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)),
        &doc);
}