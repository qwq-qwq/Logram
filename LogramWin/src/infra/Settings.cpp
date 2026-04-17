#include "infra/Settings.h"

#ifdef _WIN32
#include <shlwapi.h>

Settings& Settings::Instance() {
    static Settings s;
    return s;
}

DWORD Settings::ReadDword(const wchar_t* name, DWORD defaultVal) const {
    DWORD value = defaultVal, size = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, kRegKey, name, RRF_RT_DWORD, nullptr, &value, &size);
    return value;
}

void Settings::WriteDword(const wchar_t* name, DWORD value) {
    HKEY hk;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hk, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
        RegCloseKey(hk);
    }
}

std::string Settings::ReadString(const wchar_t* name, const char* defaultVal) const {
    wchar_t buf[256] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegKey, name, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) {
        std::string result;
        for (const wchar_t* p = buf; *p; ++p) result.push_back(static_cast<char>(*p));
        return result;
    }
    return defaultVal ? defaultVal : "";
}

void Settings::WriteString(const wchar_t* name, const std::string& value) {
    std::wstring wide(value.begin(), value.end());
    WriteWString(name, wide);
}

std::wstring Settings::ReadWString(const wchar_t* name) const {
    wchar_t buf[1024] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegKey, name, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) {
        return buf;
    }
    return {};
}

void Settings::WriteWString(const wchar_t* name, const std::wstring& value) {
    HKEY hk;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hk, name, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(value.c_str()),
                       static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hk);
    }
}

std::string Settings::GetTheme() const { return ReadString(L"Theme", "TokyoNight"); }
void Settings::SetTheme(const std::string& theme) { WriteString(L"Theme", theme); }

bool Settings::GetShowDuration() const { return ReadDword(L"ShowDuration", 0) != 0; }
void Settings::SetShowDuration(bool show) { WriteDword(L"ShowDuration", show ? 1 : 0); }

int Settings::GetSplitterPos(int id) const {
    wchar_t name[32];
    wsprintfW(name, L"Splitter%d", id);
    return static_cast<int>(ReadDword(name, 0xFFFFFFFF));
}

void Settings::SetSplitterPos(int id, int pos) {
    wchar_t name[32];
    wsprintfW(name, L"Splitter%d", id);
    WriteDword(name, static_cast<DWORD>(pos));
}

std::vector<std::wstring> Settings::GetRecentFiles() const {
    std::vector<std::wstring> result;
    for (int i = 0; i < 10; ++i) {
        wchar_t name[32];
        wsprintfW(name, L"Recent%d", i);
        auto path = ReadWString(name);
        if (!path.empty()) result.push_back(path);
    }
    return result;
}

void Settings::AddRecentFile(const std::wstring& path) {
    auto files = GetRecentFiles();
    // Remove if already present
    files.erase(std::remove(files.begin(), files.end(), path), files.end());
    files.insert(files.begin(), path);
    if (files.size() > 10) files.resize(10);
    for (int i = 0; i < static_cast<int>(files.size()); ++i) {
        wchar_t name[32];
        wsprintfW(name, L"Recent%d", i);
        WriteWString(name, files[i]);
    }
}

#else
// Stub for non-Windows
Settings& Settings::Instance() { static Settings s; return s; }
std::string Settings::GetTheme() const { return "TokyoNight"; }
void Settings::SetTheme(const std::string&) {}
bool Settings::GetShowDuration() const { return false; }
void Settings::SetShowDuration(bool) {}
int Settings::GetSplitterPos(int) const { return -1; }
void Settings::SetSplitterPos(int,int) {}
std::vector<std::wstring> Settings::GetRecentFiles() const { return {}; }
void Settings::AddRecentFile(const std::wstring&) {}
#endif
