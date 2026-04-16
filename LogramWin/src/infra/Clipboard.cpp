#include "infra/Clipboard.h"
#include "infra/Utf.h"

#ifdef _WIN32
bool CopyToClipboard(HWND hwnd, std::string_view utf8Text) {
    if (!OpenClipboard(hwnd)) return false;
    EmptyClipboard();

    auto wide = Utf8ToWide(utf8Text);
    size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) { CloseClipboard(); return false; }

    auto* ptr = static_cast<wchar_t*>(GlobalLock(hMem));
    memcpy(ptr, wide.data(), wide.size() * sizeof(wchar_t));
    ptr[wide.size()] = L'\0';
    GlobalUnlock(hMem);

    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
}
#endif
