#include "sql/SqlParamSubst.h"
#include <map>
#include <cstring>
#include <cctype>

// Minimal JSON parser for {"P1":value, "P2s8":"str", ...}
// Only handles the specific format UB uses for cust1 params.

static int ExtractParamNumber(std::string_view key) {
    if (key.size() < 2 || key[0] != 'P') return -1;
    int num = 0;
    for (size_t i = 1; i < key.size(); ++i) {
        if (key[i] >= '0' && key[i] <= '9') {
            num = num * 10 + (key[i] - '0');
        } else {
            break;
        }
    }
    return (num > 0) ? num : -1;
}

// Extract a JSON string value (assumes pos points to opening quote)
static std::string ExtractJsonString(std::string_view json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '"') return {};
    pos++; // skip opening quote
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
            switch (json[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                default: result += '\\'; result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        pos++;
    }
    if (pos < json.size()) pos++; // skip closing quote
    return result;
}

// Skip whitespace
static void SkipWS(std::string_view json, size_t& pos) {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\n' || json[pos] == '\r')) pos++;
}

// Parse {"P1":123, "P2s8":"hello", "P3d":"2025-01-01", ...} into param map
static std::map<int, std::string> ParseParamsJSON(std::string_view json) {
    std::map<int, std::string> result;

    // Trim
    while (!json.empty() && (json.front() == ' ' || json.front() == '\t')) json.remove_prefix(1);
    while (!json.empty() && (json.back() == ' ' || json.back() == '\t')) json.remove_suffix(1);

    if (json.empty() || json.front() != '{') return result;

    size_t pos = 1; // skip '{'
    while (pos < json.size()) {
        SkipWS(json, pos);
        if (pos >= json.size() || json[pos] == '}') break;
        if (json[pos] == ',') { pos++; continue; }

        // Parse key
        std::string key = ExtractJsonString(json, pos);
        int num = ExtractParamNumber(key);

        SkipWS(json, pos);
        if (pos >= json.size() || json[pos] != ':') break;
        pos++; // skip ':'
        SkipWS(json, pos);

        if (num < 0) {
            // Skip value
            if (json[pos] == '"') {
                ExtractJsonString(json, pos);
            } else {
                while (pos < json.size() && json[pos] != ',' && json[pos] != '}') pos++;
            }
            continue;
        }

        // Parse value
        std::string displayValue;
        if (json[pos] == '"') {
            std::string str = ExtractJsonString(json, pos);
            displayValue = "'" + str + "'";
        } else if (json[pos] == 't' && json.substr(pos).starts_with("true")) {
            displayValue = "true";
            pos += 4;
        } else if (json[pos] == 'f' && json.substr(pos).starts_with("false")) {
            displayValue = "false";
            pos += 5;
        } else if (json[pos] == 'n' && json.substr(pos).starts_with("null")) {
            displayValue = "NULL";
            pos += 4;
        } else {
            // Number
            size_t start = pos;
            while (pos < json.size() && json[pos] != ',' && json[pos] != '}' &&
                   json[pos] != ' ' && json[pos] != '\n') pos++;
            displayValue = std::string(json.substr(start, pos - start));
        }

        result[num] = displayValue;
    }

    return result;
}

std::string SqlParamSubstitute(std::string_view sql, std::string_view paramsJSON) {
    auto paramMap = ParseParamsJSON(paramsJSON);
    if (paramMap.empty()) return std::string(sql);

    std::string sqlStr(sql);

    // Detect format: :N (Oracle) or ? (positional)
    if (sqlStr.find(":1") != std::string::npos || sqlStr.find(":2") != std::string::npos) {
        // Oracle style — replace from highest to avoid :1 matching :10
        for (auto it = paramMap.rbegin(); it != paramMap.rend(); ++it) {
            std::string placeholder = ":" + std::to_string(it->first);
            size_t pos = 0;
            while ((pos = sqlStr.find(placeholder, pos)) != std::string::npos) {
                // Make sure next char is not a digit (avoid :1 matching :10)
                size_t afterPos = pos + placeholder.size();
                if (afterPos < sqlStr.size() && isdigit(static_cast<unsigned char>(sqlStr[afterPos]))) {
                    pos = afterPos;
                    continue;
                }
                sqlStr.replace(pos, placeholder.size(), it->second);
                pos += it->second.size();
            }
        }
        return sqlStr;
    } else if (sqlStr.find('?') != std::string::npos) {
        // Positional ? — replace sequentially
        std::string result;
        int paramIdx = 1;
        for (size_t i = 0; i < sqlStr.size(); ++i) {
            if (sqlStr[i] == '?') {
                auto it = paramMap.find(paramIdx);
                if (it != paramMap.end()) {
                    result += it->second;
                } else {
                    result += '?';
                }
                paramIdx++;
            } else {
                result += sqlStr[i];
            }
        }
        return result;
    }

    return sqlStr;
}
