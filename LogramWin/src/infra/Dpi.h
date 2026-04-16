#pragma once

#ifdef _WIN32
#include <windows.h>

int GetDpiForHwnd(HWND hwnd);
int ScaleForDpi(int value, int dpi);
#endif
