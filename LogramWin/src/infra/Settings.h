#pragma once
#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

class Settings {
public:
    static Settings& Instance();

    std::string GetTheme() const;
    void SetTheme(const std::string& theme);

    bool GetShowDuration() const;
    void SetShowDuration(bool show);

    int GetSplitterPos(int id) const;
    void SetSplitterPos(int id, int pos);

    std::vector<std::wstring> GetRecentFiles() const;
    void AddRecentFile(const std::wstring& path);

private:
    Settings() = default;

#ifdef _WIN32
    static constexpr const wchar_t* kRegKey = L"Software\\Logram";

    DWORD ReadDword(const wchar_t* name, DWORD defaultVal) const;
    void WriteDword(const wchar_t* name, DWORD value);
    std::string ReadString(const wchar_t* name, const char* defaultVal) const;
    void WriteString(const wchar_t* name, const std::string& value);
    std::wstring ReadWString(const wchar_t* name) const;
    void WriteWString(const wchar_t* name, const std::wstring& value);
#endif
};
