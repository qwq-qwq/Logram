#pragma once
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
bool CopyToClipboard(HWND hwnd, std::string_view utf8Text);
#endif
