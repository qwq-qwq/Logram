#include "ui/StatsDialog.h"
#include "ui/ThemeColors.h"
#include "infra/Utf.h"
#include "infra/Clipboard.h"
#include <commctrl.h>
#include <cstdio>

static constexpr int IDC_LIST = 101;
static constexpr int IDC_OK   = 102;
static constexpr int IDC_COPY = 103;

struct StatsDlgData {
    HWND hwndList = nullptr;
    HWND hwndOk = nullptr;
    HWND hwndCopy = nullptr;
    HFONT hFont = nullptr;
    HBRUSH hBgBrush = nullptr;
    std::wstring filePath; // for copy button
};

static LRESULT CALLBACK StatsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* data = reinterpret_cast<StatsDlgData*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* doc = static_cast<const LogDocument*>(cs->lpCreateParams);
            auto* d = new StatsDlgData();
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

            UINT dpi = GetDpiForWindow(hwnd);
            if (dpi == 0) dpi = 96;
            d->hFont = CreateFontW(-MulDiv(11, dpi, 72), 0, 0, 0, FW_NORMAL,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH | FF_SWISS, L"Segoe UI");

            auto& theme = CurrentTheme();
            d->hBgBrush = CreateSolidBrush(ToCOLORREF(theme.background));

            HINSTANCE hInst = reinterpret_cast<HINSTANCE>(
                GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));

            d->hwndList = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
                WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOCOLUMNHEADER,
                0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(IDC_LIST), hInst, nullptr);
            ListView_SetExtendedListViewStyle(d->hwndList,
                LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            ListView_SetBkColor(d->hwndList, ToCOLORREF(theme.background));
            ListView_SetTextBkColor(d->hwndList, ToCOLORREF(theme.background));
            ListView_SetTextColor(d->hwndList, ToCOLORREF(theme.foreground));
            if (d->hFont) SendMessageW(d->hwndList, WM_SETFONT,
                                       reinterpret_cast<WPARAM>(d->hFont), TRUE);

            LVCOLUMNW col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.pszText = const_cast<LPWSTR>(L"Property");
            col.cx = MulDiv(150, dpi, 96);
            ListView_InsertColumn(d->hwndList, 0, &col);
            col.pszText = const_cast<LPWSTR>(L"Value");
            col.cx = MulDiv(500, dpi, 96);
            ListView_InsertColumn(d->hwndList, 1, &col);

            d->hwndOk = CreateWindowExW(0, L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                0, 0, MulDiv(80, dpi, 96), MulDiv(28, dpi, 96),
                hwnd, reinterpret_cast<HMENU>(IDC_OK), hInst, nullptr);
            if (d->hFont) SendMessageW(d->hwndOk, WM_SETFONT,
                                       reinterpret_cast<WPARAM>(d->hFont), TRUE);

            d->hwndCopy = CreateWindowExW(0, L"BUTTON", L"Copy Path",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                0, 0, MulDiv(80, dpi, 96), MulDiv(28, dpi, 96),
                hwnd, reinterpret_cast<HMENU>(IDC_COPY), hInst, nullptr);
            if (d->hFont) SendMessageW(d->hwndCopy, WM_SETFONT,
                                       reinterpret_cast<WPARAM>(d->hFont), TRUE);

            // Populate data
            auto addRow = [&](const wchar_t* key, const std::wstring& value) {
                int n = ListView_GetItemCount(d->hwndList);
                LVITEMW item = {};
                item.mask = LVIF_TEXT;
                item.iItem = n;
                item.pszText = const_cast<LPWSTR>(key);
                ListView_InsertItem(d->hwndList, &item);
                ListView_SetItemText(d->hwndList, n, 1, const_cast<LPWSTR>(value.c_str()));
            };

            if (!doc->UBVersion().empty())
                addRow(L"UB Version", Utf8ToWide(doc->UBVersion()));
            if (!doc->HostInfo().empty())
                addRow(L"Host", Utf8ToWide(doc->HostInfo()));
            addRow(L"File", Utf8ToWide(doc->FileName()));

            auto fpath = doc->FilePath();
            if (!fpath.empty()) {
                addRow(L"Path", fpath);
                d->filePath = fpath;
            }

            char buf[64];
            double sizeMB = static_cast<double>(doc->FileSize()) / 1'000'000.0;
            snprintf(buf, sizeof(buf), "%.1f MB", sizeMB);
            addRow(L"Size", Utf8ToWide(buf));

            snprintf(buf, sizeof(buf), "%d", doc->TotalEvents());
            addRow(L"Total Events", Utf8ToWide(buf));

            snprintf(buf, sizeof(buf), "%d", doc->HttpRequests());
            addRow(L"HTTP Requests", Utf8ToWide(buf));

            snprintf(buf, sizeof(buf), "%d", doc->SqlQueries());
            addRow(L"SQL Queries", Utf8ToWide(buf));

            snprintf(buf, sizeof(buf), "%d", doc->ErrorCount());
            addRow(L"Errors", Utf8ToWide(buf));

            addRow(L"Duration", Utf8ToWide(doc->DurationFormatted()));

            std::string threads;
            for (int t : doc->ActiveThreads()) {
                if (!threads.empty()) threads += ", ";
                threads += std::to_string(t + 1);
            }
            addRow(L"Active Threads", Utf8ToWide(threads));

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
            int btnW = MulDiv(80, dpi, 96);
            int btnH = MulDiv(26, dpi, 96);

            if (data->hwndList)
                MoveWindow(data->hwndList, 0, 0, rc.right, rc.bottom - barH, TRUE);
            if (data->hwndCopy)
                MoveWindow(data->hwndCopy, pad, rc.bottom - barH + pad/2, btnW, btnH, TRUE);
            if (data->hwndOk)
                MoveWindow(data->hwndOk, rc.right - btnW - pad,
                           rc.bottom - barH + pad/2, btnW, btnH, TRUE);
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
            if (LOWORD(wParam) == IDC_OK) {
                SendMessageW(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (LOWORD(wParam) == IDC_COPY && data) {
                CopyToClipboard(hwnd, WideToUtf8(data->filePath.c_str()));
                return 0;
            }
            break;

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

static const wchar_t* kStatsClassName = L"LogramStatsWindow";
static bool g_statsClassRegistered = false;

void ShowStatsDialog(HWND parent, const LogDocument& doc) {
    if (!g_statsClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = StatsWndProc;
        wc.hInstance = reinterpret_cast<HINSTANCE>(
            GetWindowLongPtrW(parent, GWLP_HINSTANCE));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kStatsClassName;
        RegisterClassExW(&wc);
        g_statsClassRegistered = true;
    }

    UINT dpi = GetDpiForWindow(parent);
    if (dpi == 0) dpi = 96;
    int w = MulDiv(650, dpi, 96);
    int h = MulDiv(420, dpi, 96);

    // Modal behavior: disable parent, show stats, re-enable on close
    EnableWindow(parent, FALSE);
    HWND hwnd = CreateWindowExW(0, kStatsClassName, L"Statistics",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        parent, nullptr,
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)),
        const_cast<LogDocument*>(&doc));

    // Run a local message loop for modal behavior
    MSG msg;
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
}