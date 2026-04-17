#include "ui/StatsDialog.h"
#include "infra/Utf.h"
#include <commctrl.h>
#include <cstdio>

static INT_PTR CALLBACK StatsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            auto* doc = reinterpret_cast<const LogDocument*>(lParam);
            HWND hList = GetDlgItem(hwnd, 101);
            if (!hList || !doc) break;

            SendMessageW(hList, WM_SETREDRAW, FALSE, 0);

            LVCOLUMNW col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.pszText = const_cast<LPWSTR>(L"Property");
            col.cx = 180;
            ListView_InsertColumn(hList, 0, &col);
            col.pszText = const_cast<LPWSTR>(L"Value");
            col.cx = 280;
            ListView_InsertColumn(hList, 1, &col);

            auto addRow = [&](const wchar_t* key, const std::wstring& value) {
                int n = ListView_GetItemCount(hList);
                LVITEMW item = {};
                item.mask = LVIF_TEXT;
                item.iItem = n;
                item.pszText = const_cast<LPWSTR>(key);
                ListView_InsertItem(hList, &item);
                ListView_SetItemText(hList, n, 1, const_cast<LPWSTR>(value.c_str()));
            };

            if (!doc->UBVersion().empty())
                addRow(L"UB Version", Utf8ToWide(doc->UBVersion()));
            if (!doc->HostInfo().empty())
                addRow(L"Host", Utf8ToWide(doc->HostInfo()));
            addRow(L"File", Utf8ToWide(doc->FileName()));

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

            SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            HWND hList = GetDlgItem(hwnd, 101);
            if (hList) MoveWindow(hList, 0, 0, rc.right, rc.bottom - 40, TRUE);
            HWND hOk = GetDlgItem(hwnd, IDOK);
            if (hOk) MoveWindow(hOk, rc.right - 90, rc.bottom - 34, 80, 28, TRUE);
            return TRUE;
        }

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

void ShowStatsDialog(HWND parent, const LogDocument& doc) {
    // Build a template in memory — avoids needing a .rc dialog resource.
    alignas(4) char tmpl[1024] = {};
    auto* dt = reinterpret_cast<DLGTEMPLATE*>(tmpl);
    dt->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | DS_MODALFRAME | DS_CENTER;
    dt->cdit = 2;   // list + OK button
    dt->cx = 240;
    dt->cy = 180;

    // Title "Statistics"
    auto* ptr = reinterpret_cast<WORD*>(dt + 1);
    *ptr++ = 0; // menu
    *ptr++ = 0; // class
    const wchar_t* title = L"Statistics";
    size_t titleLen = wcslen(title) + 1;
    memcpy(ptr, title, titleLen * sizeof(WORD));
    ptr += titleLen;

    // Align to DWORD
    ptr = reinterpret_cast<WORD*>((reinterpret_cast<uintptr_t>(ptr) + 3) & ~uintptr_t(3));

    // Item 1: ListView (WC_LISTVIEW == SysListView32)
    auto* di1 = reinterpret_cast<DLGITEMTEMPLATE*>(ptr);
    di1->style = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL;
    di1->x = 0; di1->y = 0; di1->cx = 240; di1->cy = 152;
    di1->id = 101;
    ptr = reinterpret_cast<WORD*>(di1 + 1);
    // Class: SysListView32 (0xFFFF followed by classAtom doesn't apply; use string)
    const wchar_t* lvClass = L"SysListView32";
    size_t lvLen = wcslen(lvClass) + 1;
    memcpy(ptr, lvClass, lvLen * sizeof(WORD));
    ptr += lvLen;
    *ptr++ = 0; // title (empty)
    *ptr++ = 0; // extra data

    // Align
    ptr = reinterpret_cast<WORD*>((reinterpret_cast<uintptr_t>(ptr) + 3) & ~uintptr_t(3));

    // Item 2: OK button
    auto* di2 = reinterpret_cast<DLGITEMTEMPLATE*>(ptr);
    di2->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;
    di2->x = 180; di2->y = 158; di2->cx = 50; di2->cy = 14;
    di2->id = IDOK;
    ptr = reinterpret_cast<WORD*>(di2 + 1);
    *ptr++ = 0xFFFF; *ptr++ = 0x0080; // button class
    const wchar_t* ok = L"OK";
    memcpy(ptr, ok, 3 * sizeof(WORD));
    ptr += 3;
    *ptr++ = 0; // extra

    DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
        dt, parent, StatsDlgProc,
        reinterpret_cast<LPARAM>(&doc));
}