#pragma once
#include "core/LogLevel.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Hot path struct — kept small for cache efficiency.
// raw text is accessed via MappedFile base + rawOffset.
// 24 bytes with 8-byte alignment (int64 epochCS); 20 bytes of payload.
struct LogLineHot {
    uint32_t rawOffset;
    uint32_t rawLength;
    int64_t  epochCS;       // centiseconds since Unix epoch, -1 if unknown
    uint8_t  level;         // LogLevel
    int8_t   thread;        // -1 = unknown
    uint16_t messageOffset; // byte offset where message starts in raw
};

static_assert(sizeof(LogLineHot) == 24, "LogLineHot layout changed unexpectedly");

struct LogLineHttp {
    uint32_t lineId;
    std::string method;
    std::string path;
    int16_t status = -1;
};

struct LogLineDuration {
    uint32_t lineId;
    int64_t  durationUS; // microseconds
};

// Utility: get raw string_view from mapped base
inline std::string_view GetRawLine(const uint8_t* mappedBase, const LogLineHot& line) {
    return {reinterpret_cast<const char*>(mappedBase + line.rawOffset), line.rawLength};
}

// Utility: get message string_view
inline std::string_view GetMessage(const uint8_t* mappedBase, const LogLineHot& line) {
    auto offset = line.messageOffset;
    if (offset == 0 || offset >= line.rawLength) {
        return GetRawLine(mappedBase, line);
    }
    return {reinterpret_cast<const char*>(mappedBase + line.rawOffset + offset),
            line.rawLength - offset};
}

// Format timestamp as HH:MM:SS.cc (pure arithmetic, no calendar)
inline std::string FormatTime(int64_t epochCS) {
    if (epochCS < 0) return {};
    constexpr int64_t dayCS = 86400LL * 100;
    int64_t timeCS = ((epochCS % dayCS) + dayCS) % dayCS;
    int cs = static_cast<int>(timeCS % 100);
    int totalSecs = static_cast<int>(timeCS / 100);
    int s = totalSecs % 60;
    int m = (totalSecs / 60) % 60;
    int h = (totalSecs / 3600) % 24;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%02d", h, m, s, cs);
    return buf;
}

// Days since Unix epoch -> civil date (inverse of LogParser::DaysFromEpoch)
inline void CivilFromDays(int64_t z, int& year, int& month, int& day) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    year = static_cast<int>(month <= 2 ? y + 1 : y);
}

// Format timestamp as "YYYY-MM-DD HH:MM:SS.cc" (epochCS already shifted to local)
inline std::string FormatTimestampFull(int64_t epochCS) {
    if (epochCS < 0) return {};
    constexpr int64_t dayCS = 86400LL * 100;
    int64_t days = epochCS / dayCS;
    int64_t timeCS = epochCS - days * dayCS;
    if (timeCS < 0) { timeCS += dayCS; --days; }
    int year, month, day;
    CivilFromDays(days, year, month, day);
    int cs = static_cast<int>(timeCS % 100);
    int totalSecs = static_cast<int>(timeCS / 100);
    int s = totalSecs % 60;
    int m = (totalSecs / 60) % 60;
    int h = (totalSecs / 3600) % 24;
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%02d",
             year, month, day, h, m, s, cs);
    return buf;
}

// Build clipboard text for one line: the compact UTC timestamp prefix is replaced
// with a readable local "YYYY-MM-DD HH:MM:SS.cc", keeping thread/level/message.
// Falls back to the raw line when there is no timestamp (console/continuation).
inline std::string BuildClipboardLine(std::string_view raw, int64_t epochCS, int threadPos) {
    if (epochCS < 0 || threadPos < 2 || static_cast<size_t>(threadPos - 2) > raw.size())
        return std::string(raw);
    std::string out = FormatTimestampFull(epochCS);
    out.append(raw.substr(threadPos - 2));
    return out;
}

// Format duration in microseconds as human-readable
inline std::string FormatDuration(int64_t durationUS) {
    if (durationUS < 0) return {};
    char buf[32];
    if (durationUS >= 1'000'000) {
        snprintf(buf, sizeof(buf), "%.1fs", static_cast<double>(durationUS) / 1'000'000.0);
    } else if (durationUS >= 1'000) {
        snprintf(buf, sizeof(buf), "%.1fms", static_cast<double>(durationUS) / 1'000.0);
    } else {
        snprintf(buf, sizeof(buf), "%lldus", static_cast<long long>(durationUS));
    }
    return buf;
}
