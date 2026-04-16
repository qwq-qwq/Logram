#include "sql/SqlStats.h"
#include <cstring>
#include <algorithm>

struct StatDef {
    const char* key;
    const char* label;
    const char* unit;
};

static constexpr StatDef kStatDefs[] = {
    {"r",  "Rows",    ""},
    {"t",  "Total",   "us"},
    {"fr", "Fetch",   "us"},
    {"c",  "Cache",   ""},
    {"w",  "Write",   "us"},
    {"p",  "Prepare", "us"},
    {"e",  "Exec",    "us"},
    {"b",  "Bytes",   ""},
};

static bool ParseKeyValue(std::string_view token, SqlStatEntry& out) {
    auto eq = token.find('=');
    if (eq == std::string_view::npos) return false;

    auto key = token.substr(0, eq);
    auto valStr = token.substr(eq + 1);

    // Only accept known short keys (avoid matching SQL fragments)
    if (key.size() > 3) return false;
    for (char c : key) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    }

    int value = 0;
    bool hasDigits = false;
    for (char c : valStr) {
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            hasDigits = true;
        } else {
            break;
        }
    }
    if (!hasDigits) return false;

    for (const auto& def : kStatDefs) {
        if (key == def.key) {
            out.label = def.label;
            out.value = value;
            out.unit = def.unit;
            return true;
        }
    }

    // Unknown key — still show it
    out.label = std::string(key);
    out.value = value;
    out.unit = "";
    return true;
}

static std::vector<SqlStatEntry> ParseStats(std::string_view s) {
    std::vector<SqlStatEntry> entries;
    size_t i = 0;
    while (i < s.size()) {
        // Skip spaces
        while (i < s.size() && s[i] == ' ') ++i;
        size_t start = i;
        while (i < s.size() && s[i] != ' ') ++i;
        if (i > start) {
            SqlStatEntry entry;
            if (ParseKeyValue(s.substr(start, i - start), entry)) {
                entries.push_back(std::move(entry));
            }
        }
    }
    return entries;
}

SqlParseResult SqlStatsParse(std::string_view message) {
    // Trim whitespace
    while (!message.empty() && message.front() == ' ') message.remove_prefix(1);
    while (!message.empty() && (message.back() == ' ' || message.back() == '\r' || message.back() == '\n'))
        message.remove_suffix(1);

    // "q=..." at start (no stats prefix)
    if (message.size() >= 2 && message[0] == 'q' && message[1] == '=') {
        auto sql = message.substr(2);
        while (!sql.empty() && sql.front() == ' ') sql.remove_prefix(1);
        return {std::string(sql), {}};
    }

    // " q=" separates stats from SQL
    auto qPos = message.find(" q=");
    if (qPos != std::string_view::npos) {
        auto statsPart = message.substr(0, qPos);
        auto sqlPart = message.substr(qPos + 3);
        while (!sqlPart.empty() && sqlPart.front() == ' ') sqlPart.remove_prefix(1);
        return {std::string(sqlPart), ParseStats(statsPart)};
    }

    // No "q=" — try to find stats
    auto entries = ParseStats(message);
    if (!entries.empty()) return {"", std::move(entries)};

    return {std::string(message), {}};
}

std::string SqlStatsFormatValue(const SqlStatEntry& entry) {
    if (entry.unit == "us") {
        if (entry.value >= 1'000'000) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f s", static_cast<double>(entry.value) / 1'000'000.0);
            return buf;
        } else if (entry.value >= 1'000) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f ms", static_cast<double>(entry.value) / 1'000.0);
            return buf;
        } else {
            return std::to_string(entry.value) + " us";
        }
    }
    return std::to_string(entry.value);
}
