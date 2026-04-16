// Tests for LogParser — byte-level format detection and parsing.
// Compares against known-good values derived from the Swift implementation.
// Run: ./parser_tests (CTest will exit non-zero on any failure)

#include "core/LogLevel.h"
#include "core/LogLine.h"
#include "core/LogParser.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

static int g_failures = 0;
static int g_passed   = 0;

#define CHECK(cond) do {                                                \
    if (cond) { ++g_passed; }                                           \
    else {                                                              \
        ++g_failures;                                                   \
        std::fprintf(stderr, "FAIL [%s:%d]: %s\n",                      \
                     __FILE__, __LINE__, #cond);                        \
    }                                                                   \
} while (0)

#define CHECK_EQ(a, b) do {                                             \
    auto _va = (a);                                                     \
    auto _vb = (b);                                                     \
    if (_va == _vb) { ++g_passed; }                                     \
    else {                                                              \
        ++g_failures;                                                   \
        std::fprintf(stderr, "FAIL [%s:%d]: %s != %s\n",                \
                     __FILE__, __LINE__, #a, #b);                       \
    }                                                                   \
} while (0)

namespace {

std::vector<std::string_view> SplitLines(const std::string& text) {
    std::vector<std::string_view> out;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            size_t end = i;
            if (end > start && text[end - 1] == '\r') --end;
            out.emplace_back(text.data() + start, end - start);
            start = i + 1;
        }
    }
    if (start < text.size()) out.emplace_back(text.data() + start, text.size() - start);
    return out;
}

// ---- Hinnant's algorithm: known landmark days ------------------------

void TestDaysFromEpoch() {
    std::puts("== DaysFromEpoch ==");
    CHECK_EQ(LogParser::DaysFromEpoch(1970, 1, 1), 0);
    CHECK_EQ(LogParser::DaysFromEpoch(1970, 1, 2), 1);
    CHECK_EQ(LogParser::DaysFromEpoch(1969, 12, 31), -1);
    CHECK_EQ(LogParser::DaysFromEpoch(2000, 1, 1), 10957);
    CHECK_EQ(LogParser::DaysFromEpoch(2021, 7, 27), 18835);
    // Leap day
    CHECK_EQ(LogParser::DaysFromEpoch(2020, 3, 1) - LogParser::DaysFromEpoch(2020, 2, 29), 1);
    CHECK_EQ(LogParser::DaysFromEpoch(2100, 3, 1) - LogParser::DaysFromEpoch(2100, 2, 28), 1);
}

// ---- Level code packing ---------------------------------------------

void TestLevelMap() {
    std::puts("== LevelMap ==");
    auto& map = GetPackedLevelMap();
    CHECK_EQ(map.size(), static_cast<size_t>(kLogLevelCount));

    // Exact 6-char codes (with trailing spaces) must map back correctly.
    struct Probe { const char* code; LogLevel level; };
    Probe probes[] = {
        {"info  ", LogLevel::Info},
        {"SQL   ", LogLevel::Sql},
        {"ERROR ", LogLevel::Error},
        {" +    ", LogLevel::Enter},
        {" -    ", LogLevel::Leave},
        {"EXC   ", LogLevel::Exc},
        {"http  ", LogLevel::Http},
        {"cust1 ", LogLevel::Cust1},
    };
    for (auto& p : probes) {
        auto key = Pack6(reinterpret_cast<const uint8_t*>(p.code));
        auto it = map.find(key);
        CHECK(it != map.end());
        if (it != map.end()) CHECK_EQ(it->second, p.level);
    }
}

// ---- mORMot1: "20210727 08260933  $ SQL   SELECT..." ----------------

void TestMormot1() {
    std::puts("== mORMot1 ==");
    // Minimum header for format detection: probeIdx defaults to 1
    // when only 2 header lines are given, so line[1] is used as probe.
    std::string header =
        "D:\\ub\\server.log 1.18 (Win64 1.0.0.0)\r\n"
        "Host=mypc User=ub CPU=8*3000 OS=Win64 Wow=0 Freq=10000000\r\n";
    auto headerLines = SplitLines(header);
    LogParser parser(headerLines);
    CHECK_EQ(static_cast<int>(parser.GetFormat()),
             static_cast<int>(LogParser::Format::Mormot1));
    CHECK_EQ(parser.GetThreadPos(), 19);

    // Line: "20210727 08260933  $ SQL   SELECT 1"
    // thread '$' = 3, level "SQL   " → Sql
    const char* line = "20210727 08260933  $ SQL   SELECT 1";
    auto len = static_cast<uint32_t>(std::strlen(line));
    auto r = parser.ParseLine(reinterpret_cast<const uint8_t*>(line), len, 0, 0);

    CHECK_EQ(static_cast<int>(r.hot.level), static_cast<int>(LogLevel::Sql));
    CHECK_EQ(static_cast<int>(r.hot.thread), 3);

    // Expected epochCS (pure arithmetic, matches Swift):
    // days = DaysFromEpoch(2021, 7, 27) = 18835
    // base = 18835 * 8'640'000 + 08*360000 + 26*6000 + 9*100 + 33
    int64_t days = LogParser::DaysFromEpoch(2021, 7, 27);
    int64_t expectedCS = days * 8'640'000LL
                       + 8LL  * 360'000LL
                       + 26LL * 6'000LL
                       + 9LL  * 100LL
                       + 33LL;
    CHECK_EQ(r.hot.epochCS, expectedCS);
    CHECK_EQ(static_cast<int>(r.hot.messageOffset), 27);

    auto msg = GetMessage(reinterpret_cast<const uint8_t*>(line), r.hot);
    CHECK(msg.starts_with("SELECT 1"));
}

// ---- mORMot1: Leave line with duration -----------------------------

void TestMormot1LeaveDuration() {
    std::puts("== mORMot1 leave duration ==");
    std::string header =
        "server.log 1.18\r\n"
        "Host=x User=y CPU=8*3000 OS=Win64 Wow=0 Freq=10000000\r\n";
    auto headerLines = SplitLines(header);
    LogParser parser(headerLines);

    // " -    01.234.567" → 1*1M + 234*1K + 567 = 1'234'567 us
    const char* line = "20210727 08260933  !  -    01.234.567";
    auto len = static_cast<uint32_t>(std::strlen(line));
    auto r = parser.ParseLine(reinterpret_cast<const uint8_t*>(line), len, 0, 0);
    CHECK_EQ(static_cast<int>(r.hot.level), static_cast<int>(LogLevel::Leave));
    CHECK_EQ(r.durationUS, 1'234'567LL);
}

// ---- mORMot2: hex HiRes ---------------------------------------------

void TestMormot2() {
    std::puts("== mORMot2 ==");
    // probeIdx: count > 4 → index 4 (fifth line).
    // Header layout to mirror real UB logs:
    //   [0] path + version  (version string)
    //   [1] Host= ... Freq= (host info, freq)
    //   [2] TSynLog info
    //   [3] (blank / PRTL info combined)
    //   [4] first data line — must begin with 14 zeros
    // PRTL must appear in one of the first five lines.
    std::string header =
        "D:\\ub\\server.log 1.18\r\n"
        "Host=x User=y CPU=8*3000 OS=Win64 Wow=0 Freq=10000000\r\n"
        "TSynLog 1.18 placeholder\r\n"
        "TZ=+0300 PRTL 2022-10-27T00:00:00.000+0300\r\n"
        "00000000000000XX  ! info  probe\r\n";
    auto headerLines = SplitLines(header);
    LogParser parser(headerLines);
    CHECK_EQ(static_cast<int>(parser.GetFormat()),
             static_cast<int>(LogParser::Format::Mormot2));
    CHECK_EQ(parser.GetThreadPos(), 18);
    CHECK(parser.GetStartEpochCS() >= 0);

    // Line: "0000000000000000  ! info  hello"
    // ticks = 0 → ms = 0 → epochCS = startEpochCS
    const char* line = "0000000000000000  ! info  hello";
    auto len = static_cast<uint32_t>(std::strlen(line));
    auto r = parser.ParseLine(reinterpret_cast<const uint8_t*>(line), len, 0, 0);
    CHECK_EQ(static_cast<int>(r.hot.level), static_cast<int>(LogLevel::Info));
    CHECK_EQ(static_cast<int>(r.hot.thread), 0);
    CHECK_EQ(r.hot.epochCS, parser.GetStartEpochCS());

    // Non-zero ticks
    const char* line2 = "0000000000000C72  ! info  hello";
    // 0xC72 = 3186, freq from "Freq=10000000" → hiResFreq_ = 10000
    // ms = 3186 / 10000 = 0.3186 → cs = 0 (int cast of 0.03186)
    // Just verify parsing succeeds with consistent values:
    auto r2 = parser.ParseLine(reinterpret_cast<const uint8_t*>(line2),
                               static_cast<uint32_t>(std::strlen(line2)), 0, 0);
    CHECK_EQ(static_cast<int>(r2.hot.level), static_cast<int>(LogLevel::Info));
    CHECK(r2.hot.epochCS >= parser.GetStartEpochCS());
}

// ---- journald -------------------------------------------------------

void TestJournald() {
    std::puts("== journald ==");
    // probe at line index 1 (two lines total). Line must be >31 chars and
    // have '+' at offset 26 (after fractional seconds).
    std::string header =
        "header ignored\r\n"
        "2022-10-27T00:00:00.358664+0300 ub[657518]:  ! info  probe\r\n";
    auto headerLines = SplitLines(header);
    LogParser parser(headerLines);
    CHECK_EQ(static_cast<int>(parser.GetFormat()),
             static_cast<int>(LogParser::Format::Journald));

    // Thread position should land on the thread char '!'
    // "]: " + 2 = after "]:  " leaves the thread char position.
    const char* line = "2022-10-27T00:00:00.358664+0300 ub[657518]:  ! info  msg";
    auto len = static_cast<uint32_t>(std::strlen(line));
    auto r = parser.ParseLine(reinterpret_cast<const uint8_t*>(line), len, 0, 0);
    CHECK_EQ(static_cast<int>(r.hot.level), static_cast<int>(LogLevel::Info));
    CHECK_EQ(static_cast<int>(r.hot.thread), 0);

    // Expected epochCS: 2022-10-27 00:00:00.35 UTC+3 → 2022-10-26 21:00:00.35 UTC
    int64_t days = LogParser::DaysFromEpoch(2022, 10, 27);
    int64_t expected = days * 8'640'000LL + 35LL - 3LL * 360'000LL;
    CHECK_EQ(r.hot.epochCS, expected);
}

// ---- console (no timestamp) -----------------------------------------

void TestConsole() {
    std::puts("== console ==");
    std::string header =
        "banner\r\n"
        "  ! info  first line\r\n";
    auto headerLines = SplitLines(header);
    LogParser parser(headerLines);
    CHECK_EQ(static_cast<int>(parser.GetFormat()),
             static_cast<int>(LogParser::Format::Console));
    CHECK_EQ(parser.GetThreadPos(), 2);

    const char* line = "  ! info  hello world";
    auto len = static_cast<uint32_t>(std::strlen(line));
    auto r = parser.ParseLine(reinterpret_cast<const uint8_t*>(line), len, 0, 0);
    CHECK_EQ(static_cast<int>(r.hot.level), static_cast<int>(LogLevel::Info));
    CHECK_EQ(static_cast<int>(r.hot.thread), 0);
    CHECK_EQ(r.hot.epochCS, -1);
}

// ---- HTTP request/response parsing ---------------------------------

void TestHttpFields() {
    std::puts("== HTTP fields ==");
    std::string header =
        "server.log 1.18\r\n"
        "Host=x User=y CPU=8*3000 OS=Win64 Wow=0 Freq=10000000\r\n";
    auto headerLines = SplitLines(header);
    LogParser parser(headerLines);

    // The Swift reference parser scans for the 4-byte pattern
    //   ' ' '-' ' ' '>'   (request arrow)
    //   ' ' '<' ' ' '-'   (response arrow)
    // i.e. the inner char is itself separated by a space. Preserve that
    // exact pattern here so the test mirrors Swift behaviour.
    {
        const char* line = "20210727 08260933  ! http   - > GET /api/data";
        auto len = static_cast<uint32_t>(std::strlen(line));
        auto r = parser.ParseLine(reinterpret_cast<const uint8_t*>(line), len, 0, 0);
        CHECK_EQ(static_cast<int>(r.hot.level), static_cast<int>(LogLevel::Http));

        LogLineHttp http;
        http.lineId = 0;
        LogParser::ParseHTTPFields(reinterpret_cast<const uint8_t*>(line), len,
                                   r.hot.messageOffset, line, http);
        CHECK_EQ(http.method, std::string("GET"));
        CHECK_EQ(http.path, std::string("/api/data"));
    }

    {
        const char* line = "20210727 08260933  ! http   < - 200 OK";
        auto len = static_cast<uint32_t>(std::strlen(line));
        auto r = parser.ParseLine(reinterpret_cast<const uint8_t*>(line), len, 0, 0);

        LogLineHttp http;
        LogParser::ParseHTTPFields(reinterpret_cast<const uint8_t*>(line), len,
                                   r.hot.messageOffset, line, http);
        CHECK_EQ(static_cast<int>(http.status), 200);
    }
}

// ---- unknown lines stay unknown ------------------------------------

void TestUnknown() {
    std::puts("== unknown ==");
    std::string header =
        "server.log 1.18\r\n"
        "Host=x User=y CPU=8*3000 OS=Win64 Wow=0 Freq=10000000\r\n";
    auto headerLines = SplitLines(header);
    LogParser parser(headerLines);

    const char* line = "short";
    auto r = parser.ParseLine(reinterpret_cast<const uint8_t*>(line),
                              static_cast<uint32_t>(std::strlen(line)), 0, 0);
    CHECK_EQ(static_cast<int>(r.hot.level), static_cast<int>(LogLevel::Unknown));
    CHECK_EQ(static_cast<int>(r.hot.thread), -1);
}

// ---- time formatting -----------------------------------------------

void TestFormatTime() {
    std::puts("== FormatTime ==");
    // 10:20:30.45 on 1970-01-01 → 10*360000 + 20*6000 + 30*100 + 45
    int64_t cs = 10LL * 360'000LL + 20LL * 6'000LL + 30LL * 100LL + 45LL;
    CHECK_EQ(FormatTime(cs), std::string("10:20:30.45"));
    CHECK_EQ(FormatTime(-1), std::string());
}

// ---- Golden-file cross-check (optional) -----------------------------
// For each pair (X.log, X.expected.ndjson) under tests/golden/ verify
// that the C++ parser produces the same structured output as the Mac
// reference (generated by tools/golden_gen.swift). Missing files are a
// skip, not a failure — this keeps CI green until someone drops the
// fixtures in.

namespace gold {

std::string ReadAll(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream os; os << f.rdbuf();
    return os.str();
}

// Minimal JSON reader for the flat, machine-generated objects written by
// golden_gen.swift. Not a general-purpose parser — accepts exactly what
// the Swift generator emits (sorted keys, no escapes except \").
struct Fields {
    int id = -1;
    std::string level;
    int thread = -1;
    int64_t epochCS = -1;
    int messageOffset = 0;
    bool hasDuration = false; int64_t durationUS = -1;
    bool hasMethod = false; std::string method;
    bool hasPath = false; std::string path;
    bool hasStatus = false; int status = -1;
};

std::string ReadString(std::string_view& s) {
    if (s.empty() || s.front() != '"') return {};
    s.remove_prefix(1);
    std::string out;
    while (!s.empty() && s.front() != '"') {
        if (s.front() == '\\' && s.size() > 1) { out.push_back(s[1]); s.remove_prefix(2); }
        else { out.push_back(s.front()); s.remove_prefix(1); }
    }
    if (!s.empty()) s.remove_prefix(1);
    return out;
}
int64_t ReadNumber(std::string_view& s) {
    int64_t v = 0; bool neg = false;
    if (!s.empty() && s.front() == '-') { neg = true; s.remove_prefix(1); }
    while (!s.empty() && s.front() >= '0' && s.front() <= '9') {
        v = v * 10 + (s.front() - '0');
        s.remove_prefix(1);
    }
    return neg ? -v : v;
}

Fields ParseLine(std::string_view s) {
    Fields f;
    // Drop '{'
    while (!s.empty() && s.front() != '{') s.remove_prefix(1);
    if (!s.empty()) s.remove_prefix(1);
    while (!s.empty() && s.front() != '}') {
        while (!s.empty() && (s.front() == ' ' || s.front() == ',')) s.remove_prefix(1);
        if (s.empty() || s.front() == '}') break;
        auto key = ReadString(s);
        while (!s.empty() && (s.front() == ':' || s.front() == ' ')) s.remove_prefix(1);
        if (s.empty()) break;

        if (s.front() == 'n') { // null
            s.remove_prefix(std::min(s.size(), size_t(4)));
        } else if (s.front() == '"') {
            auto v = ReadString(s);
            if (key == "level") f.level = v;
            else if (key == "httpMethod") { f.hasMethod = true; f.method = v; }
            else if (key == "httpPath")   { f.hasPath = true;   f.path = v; }
        } else {
            auto v = ReadNumber(s);
            if      (key == "id")            f.id = static_cast<int>(v);
            else if (key == "thread")        f.thread = static_cast<int>(v);
            else if (key == "epochCS")       f.epochCS = v;
            else if (key == "messageOffset") f.messageOffset = static_cast<int>(v);
            else if (key == "durationUS")    { f.hasDuration = true; f.durationUS = v; }
            else if (key == "httpStatus")    { f.hasStatus = true;   f.status = static_cast<int>(v); }
        }
    }
    return f;
}

void RunOne(const std::filesystem::path& logPath,
            const std::filesystem::path& expectedPath) {
    std::printf("== golden: %s ==\n", logPath.filename().string().c_str());

    auto text = ReadAll(logPath);
    // Split into lines (strip trailing \r)
    std::vector<std::string> lines;
    {
        size_t start = 0;
        for (size_t i = 0; i <= text.size(); ++i) {
            if (i == text.size() || text[i] == '\n') {
                size_t end = i;
                if (end > start && text[end - 1] == '\r') --end;
                lines.emplace_back(text.data() + start, end - start);
                start = i + 1;
            }
        }
    }

    std::vector<std::string_view> headerLines;
    for (size_t i = 0; i < std::min(lines.size(), size_t(10)); ++i) {
        headerLines.emplace_back(lines[i]);
    }
    LogParser parser(headerLines);

    auto expected = ReadAll(expectedPath);
    size_t idx = 0;
    size_t start = 0;
    for (size_t i = 0; i <= expected.size(); ++i) {
        if (i == expected.size() || expected[i] == '\n') {
            if (i > start) {
                auto f = ParseLine(std::string_view(expected.data() + start, i - start));
                if (idx < lines.size()) {
                    auto& raw = lines[idx];
                    auto r = parser.ParseLine(
                        reinterpret_cast<const uint8_t*>(raw.data()),
                        static_cast<uint32_t>(raw.size()),
                        0, static_cast<uint32_t>(idx));
                    auto& info = GetLogLevelInfo(static_cast<LogLevel>(r.hot.level));
                    CHECK_EQ(std::string(info.code), f.level);
                    CHECK_EQ(static_cast<int>(r.hot.thread), f.thread);
                    CHECK_EQ(r.hot.epochCS, f.epochCS);
                    CHECK_EQ(static_cast<int>(r.hot.messageOffset), f.messageOffset);
                    if (f.hasDuration) CHECK_EQ(r.durationUS, f.durationUS);
                }
                ++idx;
            }
            start = i + 1;
        }
    }
}

void Run() {
    namespace fs = std::filesystem;
    fs::path goldenDir("tests/golden");
    if (!fs::exists(goldenDir)) return;

    for (const auto& entry : fs::directory_iterator(goldenDir)) {
        if (entry.path().extension() != ".log") continue;
        auto expected = entry.path();
        expected.replace_extension(".expected.ndjson");
        if (!fs::exists(expected)) continue;
        RunOne(entry.path(), expected);
    }
}

} // namespace gold

} // namespace

int main() {
    TestDaysFromEpoch();
    TestLevelMap();
    TestMormot1();
    TestMormot1LeaveDuration();
    TestMormot2();
    TestJournald();
    TestConsole();
    TestHttpFields();
    TestUnknown();
    TestFormatTime();

    gold::Run();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failures);
    return g_failures == 0 ? 0 : 1;
}