#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

struct SqlStatEntry {
    std::string label;
    int value;
    std::string unit; // "" or "us"
};

struct SqlParseResult {
    std::string sql;
    std::vector<SqlStatEntry> entries;
};

// Parse UB SQL message: "r=1 t=821 fr=818 c=2 q=SELECT ..."
SqlParseResult SqlStatsParse(std::string_view message);

// Format value with unit
std::string SqlStatsFormatValue(const SqlStatEntry& entry);
