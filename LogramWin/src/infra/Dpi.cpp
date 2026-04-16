#include "infra/Dpi.h"

#ifdef _WIN32
int GetDpiForHwnd(HWND hwnd) {
    // GetDpiForWindow requires Windows 10 1607+
    UINT dpi = GetDpiForWindow(hwnd);
    return (dpi > 0) ? static_cast<int>(dpi) : 96;
}

int ScaleForDpi(int value, int dpi) {
    return MulDiv(value, dpi, 96);
}
#endif
