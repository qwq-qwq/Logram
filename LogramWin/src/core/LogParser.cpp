#include "core/LogParser.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>

// ISO8601 date parsing (simplified for log header PRTL line)
static int64_t ParseISO8601(std::string_view str) {
    // Try "YYYY-MM-DDTHH:MM:SS" format (minimum)
    if (str.size() < 19) return -1;
    if (str[4] != '-' || str[7] != '-' || str[10] != 'T' ||
        str[13] != ':' || str[16] != ':') return -1;

    auto d2 = [&](int i) -> int { return (str[i] - '0') * 10 + (str[i + 1] - '0'); };
    auto d4 = [&](int i) -> int {
        return (str[i] - '0') * 1000 + (str[i + 1] - '0') * 100 +
               (str[i + 2] - '0') * 10 + (str[i + 3] - '0');
    };

    int year = d4(0), month = d2(5), day = d2(8);
    int hour = d2(11), minute = d2(14), sec = d2(17);

    int64_t days = LogParser::DaysFromEpoch(year, month, day);
    int64_t cs = days * 8'640'000LL + hour * 360'000LL + minute * 6'000LL + sec * 100LL;

    // Fractional seconds
    size_t pos = 19;
    if (pos < str.size() && str[pos] == '.') {
        pos++;
        if (pos < str.size()) cs += (str[pos] - '0') * 10;
        if (pos + 1 < str.size()) cs += (str[pos + 1] - '0');
    }

    // Timezone offset
    for (size_t i = 19; i < str.size() && i < 32; ++i) {
        if (str[i] == '+' || str[i] == '-') {
            int64_t sign = (str[i] == '-') ? -1 : 1;
            if (i + 4 < str.size()) {
                int tzH = d2(static_cast<int>(i + 1));
                int tzM = d2(static_cast<int>(i + 3));
                cs -= sign * (tzH * 360'000LL + tzM * 6'000LL);
            }
            break;
        }
    }
    return cs;
}

LogParser::LogParser(const std::vector<std::string_view>& headerLines) {
    if (headerLines.size() > 0) {
        ubVersion_ = std::string(headerLines[0]);
    }
    if (headerLines.size() > 1) {
        hostInfo_ = std::string(headerLines[1]);
        auto freqPos = headerLines[1].find("Freq=");
        if (freqPos != std::string_view::npos) {
            freqPos += 5; // skip "Freq="
            int64_t freq = 0;
            while (freqPos < headerLines[1].size() &&
                   headerLines[1][freqPos] >= '0' && headerLines[1][freqPos] <= '9') {
                freq = freq * 10 + (headerLines[1][freqPos] - '0');
                freqPos++;
            }
            if (freq > 0) hiResFreq_ = freq / 1000;
        }
    }

    // Find PRTL line for start date
    for (size_t i = 0; i < std::min(headerLines.size(), size_t(5)); ++i) {
        auto prtlPos = headerLines[i].find("PRTL ");
        if (prtlPos != std::string_view::npos) {
            auto dateStr = headerLines[i].substr(prtlPos + 5);
            // Trim whitespace
            while (!dateStr.empty() && (dateStr.front() == ' ' || dateStr.front() == '\t'))
                dateStr.remove_prefix(1);
            while (!dateStr.empty() && (dateStr.back() == ' ' || dateStr.back() == '\t' ||
                                         dateStr.back() == '\r' || dateStr.back() == '\n'))
                dateStr.remove_suffix(1);
            startEpochCS_ = ParseISO8601(dateStr);
        }
    }

    // Detect format from probe line
    size_t probeIdx;
    if (headerLines.size() > 5) probeIdx = 4;
    else if (headerLines.size() > 4) probeIdx = 4;
    else probeIdx = std::min(headerLines.size() - 1, size_t(1));

    if (probeIdx < headerLines.size()) {
        auto probe = headerLines[probeIdx];
        if (probe.starts_with("00000000000000")) {
            format_ = Format::Mormot2;
            thPos_ = 18;
        } else if (probe.size() > 31 && probe.substr(26, 1).front() == '+') {
            format_ = Format::Journald;
            auto bracketPos = probe.find("]:");
            if (bracketPos != std::string_view::npos) {
                thPos_ = static_cast<int>(bracketPos + 2 + 2); // "]: " + space + thread
            }
        } else if (probe.size() > 2 && probe.starts_with("  ")) {
            format_ = Format::Console;
            thPos_ = 2;
        }
    }
}

LogParser::ParseResult LogParser::ParseLine(const uint8_t* buf, uint32_t len,
                                            uint32_t rawOffset, uint32_t index) const {
    ParseResult result;
    auto& hot = result.hot;
    hot.rawOffset = rawOffset;
    hot.rawLength = len;
    hot.epochCS = -1;
    hot.level = static_cast<uint8_t>(LogLevel::Unknown);
    hot.thread = -1;
    hot.messageOffset = 0;

    if (len <= static_cast<uint32_t>(thPos_ + 8)) return result;

    // Extract thread
    uint8_t thChar = buf[thPos_];
    if (thChar < kThBase || thChar >= kThBase + kMaxThreads) return result;
    int threadIdx = static_cast<int>(thChar - kThBase);

    // Extract level (6 chars starting at thPos + 2)
    int levelStart = thPos_ + 2;
    if (len <= static_cast<uint32_t>(levelStart + 6)) return result;

    uint64_t packedKey = Pack6At(buf, levelStart);
    auto& levelMap = GetPackedLevelMap();
    auto it = levelMap.find(packedKey);
    if (it == levelMap.end()) return result;

    LogLevel level = it->second;
    int msgStart = levelStart + 6;
    uint16_t msgOffset = static_cast<uint16_t>(std::min(msgStart, static_cast<int>(len)));

    hot.level = static_cast<uint8_t>(level);
    hot.thread = static_cast<int8_t>(threadIdx);
    hot.messageOffset = msgOffset;
    hot.epochCS = ParseTimestamp(buf, len);

    // Parse duration from leave lines
    if (level == LogLevel::Leave && msgStart < static_cast<int>(len)) {
        result.durationUS = ParseDuration(buf, len, msgStart);
    }

    return result;
}

int64_t LogParser::ParseTimestamp(const uint8_t* buf, uint32_t len) const {
    switch (format_) {
        case Format::Mormot1:  return ParseMormot1(buf, len);
        case Format::Mormot2:  return ParseMormot2(buf, len);
        case Format::Journald: return ParseJournald(buf, len);
        case Format::Console:  return -1;
    }
    return -1;
}

int64_t LogParser::ParseMormot1(const uint8_t* buf, uint32_t len) const {
    if (len < 17) return -1;
    if (buf[0] < 0x30 || buf[0] > 0x39) return -1;

    auto d2 = [&](int i) -> int { return (buf[i] - 0x30) * 10 + (buf[i + 1] - 0x30); };
    auto d4 = [&](int i) -> int {
        return (buf[i] - 0x30) * 1000 + (buf[i + 1] - 0x30) * 100 +
               (buf[i + 2] - 0x30) * 10 + (buf[i + 3] - 0x30);
    };

    int year   = d4(0);
    int month  = d2(4);
    int day    = d2(6);
    int hour   = d2(9);
    int minute = d2(11);
    int sec    = d2(13);
    int cs     = d2(15);

    int64_t days = DaysFromEpoch(year, month, day);
    return days * 8'640'000LL + hour * 360'000LL + minute * 6'000LL + sec * 100LL + cs;
}

int64_t LogParser::ParseMormot2(const uint8_t* buf, uint32_t len) const {
    if (startEpochCS_ < 0 || len < 16) return -1;
    uint64_t ticks = 0;
    for (int i = 0; i < 16; ++i) {
        uint8_t c = buf[i];
        uint64_t v;
        if (c >= 0x30 && c <= 0x39)      v = c - 0x30;
        else if (c >= 0x41 && c <= 0x46)  v = c - 0x41 + 10;
        else if (c >= 0x61 && c <= 0x66)  v = c - 0x61 + 10;
        else return -1;
        ticks = ticks * 16 + v;
    }
    double ms = static_cast<double>(ticks) / static_cast<double>(hiResFreq_);
    return startEpochCS_ + static_cast<int64_t>(ms / 10.0);
}

int64_t LogParser::ParseJournald(const uint8_t* buf, uint32_t len) const {
    if (len < 31) return -1;
    // 2022-10-27T00:00:00.358664+0300
    auto d2 = [&](int i) -> int { return (buf[i] - 0x30) * 10 + (buf[i + 1] - 0x30); };
    auto d4 = [&](int i) -> int {
        return (buf[i] - 0x30) * 1000 + (buf[i + 1] - 0x30) * 100 +
               (buf[i + 2] - 0x30) * 10 + (buf[i + 3] - 0x30);
    };

    int year   = d4(0);
    int month  = d2(5);
    int day    = d2(8);
    int hour   = d2(11);
    int minute = d2(14);
    int sec    = d2(17);

    int cs = 0;
    if (len > 19 && buf[19] == 0x2E) {
        if (len > 20) cs += (buf[20] - 0x30) * 10;
        if (len > 21) cs += (buf[21] - 0x30);
    }

    int64_t tzOffsetCS = 0;
    for (uint32_t i = 19; i < std::min(len, 32u); ++i) {
        if (buf[i] == 0x2B || buf[i] == 0x2D) {
            int64_t sign = (buf[i] == 0x2D) ? -1 : 1;
            if (i + 4 < len) {
                int tzH = d2(static_cast<int>(i + 1));
                int tzM = d2(static_cast<int>(i + 3));
                tzOffsetCS = sign * (tzH * 360'000LL + tzM * 6'000LL);
            }
            break;
        }
    }

    int64_t days = DaysFromEpoch(year, month, day);
    int64_t baseCS = days * 8'640'000LL + hour * 360'000LL +
                     minute * 6'000LL + sec * 100LL + cs;
    return baseCS - tzOffsetCS;
}

int64_t LogParser::ParseDuration(const uint8_t* buf, uint32_t len, int start) {
    // Skip leading whitespace
    int i = start;
    while (i < static_cast<int>(len) && buf[i] == 0x20) ++i;

    // Parse "SS.MMM.UUU" — three dot-separated groups
    int64_t parts[3];
    int partCount = 0;
    int64_t current = 0;
    bool hasDigits = false;

    while (i < static_cast<int>(len) && partCount < 3) {
        uint8_t c = buf[i];
        if (c >= 0x30 && c <= 0x39) {
            current = current * 10 + (c - 0x30);
            hasDigits = true;
        } else if (c == 0x2E) { // '.'
            if (!hasDigits) return -1;
            parts[partCount++] = current;
            current = 0;
            hasDigits = false;
        } else {
            break;
        }
        ++i;
    }
    if (hasDigits) parts[partCount++] = current;

    if (partCount != 3) return -1;
    return parts[0] * 1'000'000 + parts[1] * 1'000 + parts[2];
}

void LogParser::ParseHTTPFields(const uint8_t* buf, uint32_t len, int start,
                                const char* rawPtr, LogLineHttp& http) {
    if (len <= static_cast<uint32_t>(start + 4)) return;

    for (int i = start; i < static_cast<int>(len) - 3; ++i) {
        if (buf[i] == 0x20 && buf[i + 2] == 0x20) {
            if (buf[i + 1] == 0x2D && i + 3 < static_cast<int>(len) && buf[i + 3] == 0x3E) {
                // " -> " — request; mirror Swift's split(..., omittingEmpty: true)
                // by skipping any leading spaces before METHOD.
                int afterArrow = i + 4;
                if (afterArrow < static_cast<int>(len)) {
                    std::string_view msg(rawPtr + afterArrow, len - afterArrow);
                    size_t p = 0;
                    while (p < msg.size() && msg[p] == ' ') ++p;
                    msg.remove_prefix(p);
                    auto spacePos = msg.find(' ');
                    if (spacePos != std::string_view::npos) {
                        http.method = std::string(msg.substr(0, spacePos));
                        auto rest = msg.substr(spacePos + 1);
                        while (!rest.empty() && rest.front() == ' ') rest.remove_prefix(1);
                        http.path = std::string(rest);
                    } else {
                        http.method = std::string(msg);
                    }
                }
                return;
            } else if (buf[i + 1] == 0x3C && i + 3 < static_cast<int>(len) && buf[i + 3] == 0x2D) {
                // " <- " — response
                int afterArrow = i + 4;
                int j = afterArrow;
                while (j < static_cast<int>(len) && buf[j] == 0x20) ++j;
                if (j + 2 < static_cast<int>(len) &&
                    buf[j] >= 0x30 && buf[j] <= 0x39 &&
                    buf[j + 1] >= 0x30 && buf[j + 1] <= 0x39 &&
                    buf[j + 2] >= 0x30 && buf[j + 2] <= 0x39) {
                    http.status = static_cast<int16_t>(
                        (buf[j] - 0x30) * 100 + (buf[j + 1] - 0x30) * 10 + (buf[j + 2] - 0x30));
                }
                return;
            }
        }
    }
}

int LogParser::DaysFromEpoch(int year, int month, int day) {
    int y = year;
    if (month <= 2) y -= 1;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
