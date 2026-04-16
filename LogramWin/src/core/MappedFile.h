#pragma once
#include <cstdint>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

class MappedFile {
public:
    MappedFile() = default;
    ~MappedFile();

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    MappedFile(MappedFile&& other) noexcept;
    MappedFile& operator=(MappedFile&& other) noexcept;

    bool Open(const wchar_t* path);
    void Close();

    const uint8_t* Data() const { return data_; }
    uint64_t Size() const { return size_; }
    bool IsOpen() const { return data_ != nullptr; }
    const std::wstring& Path() const { return path_; }

private:
    const uint8_t* data_ = nullptr;
    uint64_t size_ = 0;
    std::wstring path_;

#ifdef _WIN32
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    HANDLE hMapping_ = nullptr;
#else
    int fd_ = -1;
#endif
};
