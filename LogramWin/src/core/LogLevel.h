#pragma once
#include <cstdint>
#include <array>
#include <unordered_map>
#include <string_view>

enum class LogLevel : uint8_t {
    Unknown = 0,
    Info, Debug, Trace, Warn, Error,
    Enter, Leave,
    OsErr, Exc, ExcOs,
    Mem, Stack, Fail,
    Sql, Cache, Res, Db,
    Http, Clnt, Srvr, Call, Ret,
    Auth,
    Cust1, Cust2, Cust3, Cust4,
    Rotat,
    DddER, DddIN,
    Mon,
    COUNT
};

constexpr int kLogLevelCount = static_cast<int>(LogLevel::COUNT);
constexpr int kMaxThreads = 64;

struct LogLevelInfo {
    const char* code;     // 6-char code as in log file (with trailing spaces)
    const char* label;
    bool isError;
};

const LogLevelInfo& GetLogLevelInfo(LogLevel level);

// Pack 6 bytes into uint64_t for fast lookup
inline constexpr uint64_t Pack6(const uint8_t* buf) {
    return (uint64_t(buf[0]) << 40) | (uint64_t(buf[1]) << 32) |
           (uint64_t(buf[2]) << 24) | (uint64_t(buf[3]) << 16) |
           (uint64_t(buf[4]) << 8)  |  uint64_t(buf[5]);
}

inline uint64_t Pack6At(const uint8_t* buf, int offset) {
    return (uint64_t(buf[offset])     << 40) | (uint64_t(buf[offset + 1]) << 32) |
           (uint64_t(buf[offset + 2]) << 24) | (uint64_t(buf[offset + 3]) << 16) |
           (uint64_t(buf[offset + 4]) << 8)  |  uint64_t(buf[offset + 5]);
}

const std::unordered_map<uint64_t, LogLevel>& GetPackedLevelMap();
