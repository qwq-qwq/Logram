#pragma once
#include <string>
#include <string_view>

#ifdef _WIN32
std::wstring Utf8ToWide(std::string_view utf8);
std::string WideToUtf8(std::wstring_view wide);
#endif
