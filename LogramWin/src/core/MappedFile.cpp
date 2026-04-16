#include "core/MappedFile.h"

#ifdef _WIN32

MappedFile::~MappedFile() { Close(); }

MappedFile::MappedFile(MappedFile&& other) noexcept
    : data_(other.data_), size_(other.size_), path_(std::move(other.path_)),
      hFile_(other.hFile_), hMapping_(other.hMapping_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.hFile_ = INVALID_HANDLE_VALUE;
    other.hMapping_ = nullptr;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        Close();
        data_ = other.data_;
        size_ = other.size_;
        path_ = std::move(other.path_);
        hFile_ = other.hFile_;
        hMapping_ = other.hMapping_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.hFile_ = INVALID_HANDLE_VALUE;
        other.hMapping_ = nullptr;
    }
    return *this;
}

bool MappedFile::Open(const wchar_t* path) {
    Close();

    hFile_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile_ == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile_, &fileSize)) {
        Close();
        return false;
    }
    size_ = static_cast<uint64_t>(fileSize.QuadPart);
    if (size_ == 0) {
        Close();
        return false;
    }

    hMapping_ = CreateFileMappingW(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMapping_) {
        Close();
        return false;
    }

    data_ = static_cast<const uint8_t*>(MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, 0));
    if (!data_) {
        Close();
        return false;
    }

    path_ = path;
    return true;
}

void MappedFile::Close() {
    if (data_) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (hMapping_) {
        CloseHandle(hMapping_);
        hMapping_ = nullptr;
    }
    if (hFile_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
    }
    size_ = 0;
    path_.clear();
}

#else
// POSIX stub for compilation on macOS/Linux (testing only)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

MappedFile::~MappedFile() { Close(); }

MappedFile::MappedFile(MappedFile&& other) noexcept
    : data_(other.data_), size_(other.size_), path_(std::move(other.path_)),
      fd_(other.fd_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.fd_ = -1;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        Close();
        data_ = other.data_;
        size_ = other.size_;
        path_ = std::move(other.path_);
        fd_ = other.fd_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.fd_ = -1;
    }
    return *this;
}

bool MappedFile::Open(const wchar_t* path) {
    Close();
    // Convert wchar_t to char for POSIX
    std::string narrow;
    for (const wchar_t* p = path; *p; ++p) narrow.push_back(static_cast<char>(*p));

    fd_ = open(narrow.c_str(), O_RDONLY);
    if (fd_ < 0) return false;

    struct stat st;
    if (fstat(fd_, &st) != 0) { Close(); return false; }
    size_ = static_cast<uint64_t>(st.st_size);
    if (size_ == 0) { Close(); return false; }

    void* ptr = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (ptr == MAP_FAILED) { Close(); return false; }

    data_ = static_cast<const uint8_t*>(ptr);
    path_ = path;
    return true;
}

void MappedFile::Close() {
    if (data_) {
        munmap(const_cast<uint8_t*>(data_), size_);
        data_ = nullptr;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    size_ = 0;
    path_.clear();
}
#endif
