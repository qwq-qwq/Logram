#pragma once
#include "core/LogLevel.h"
#include "core/LogLine.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class LogParser {
public:
    enum class Format { Mormot1, Mormot2, Journald, Console };

    explicit LogParser(const std::vector<std::string_view>& headerLines);

    Format GetFormat() const { return format_; }
    int GetThreadPos() const { return thPos_; }
    int64_t GetHiResFreq() const { return hiResFreq_; }
    int64_t GetStartEpochCS() const { return startEpochCS_; }
    const std::string& GetUBVersion() const { return ubVersion_; }
    const std::string& GetHostInfo() const { return hostInfo_; }

    static constexpr uint8_t kThBase = 0x21; // '!'

    // Parse a single log line from raw bytes.
    // Returns populated LogLineHot. HTTP/Duration stored in out params if present.
    struct ParseResult {
        LogLineHot hot;
        LogLineHttp* http = nullptr;   // caller-owned, set if HTTP line
        int64_t durationUS = -1;       // set if leave line
    };

    ParseResult ParseLine(const uint8_t* buf, uint32_t len,
                          uint32_t rawOffset, uint32_t index) const;

    // Days since Unix epoch (1970-01-01) using Hinnant's algorithm
    static int DaysFromEpoch(int year, int month, int day);

    // HTTP field extraction is public because LogDocument calls it
    // lazily for Http lines during parallel parsing.
    static void ParseHTTPFields(const uint8_t* buf, uint32_t len, int start,
                                const char* rawPtr, LogLineHttp& http);

private:
    int64_t ParseTimestamp(const uint8_t* buf, uint32_t len) const;
    int64_t ParseMormot1(const uint8_t* buf, uint32_t len) const;
    int64_t ParseMormot2(const uint8_t* buf, uint32_t len) const;
    int64_t ParseJournald(const uint8_t* buf, uint32_t len) const;
    static int64_t ParseDuration(const uint8_t* buf, uint32_t len, int start);

    Format format_ = Format::Mormot1;
    int thPos_ = 19;
    int64_t hiResFreq_ = 1000;
    int64_t startEpochCS_ = -1;
    std::string ubVersion_;
    std::string hostInfo_;
};
