#include "sql/SqlFormatter.h"
#include <algorithm>
#include <set>
#include <cctype>

static bool IsUpper(const std::string& s, const std::set<std::string>& set) {
    std::string upper;
    upper.reserve(s.size());
    for (char c : s) upper.push_back(static_cast<char>(toupper(static_cast<unsigned char>(c))));
    return set.count(upper) > 0;
}

static std::string ToUpper(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) r.push_back(static_cast<char>(toupper(static_cast<unsigned char>(c))));
    return r;
}

static const std::set<std::string> kTopKeywords = {
    "SELECT", "FROM", "WHERE", "ORDER", "GROUP", "HAVING",
    "LIMIT", "OFFSET", "UNION", "EXCEPT", "INTERSECT",
    "INSERT", "INTO", "VALUES", "UPDATE", "SET", "DELETE",
    "WITH", "RETURNING",
};

static const std::set<std::string> kSubKeywords = {
    "AND", "OR", "LEFT", "RIGHT", "INNER", "OUTER",
    "CROSS", "FULL", "JOIN", "ON",
};

static const std::set<std::string> kBlockOpenKeywords = {
    "DECLARE", "BEGIN", "LOOP", "EXCEPTION",
};

static const std::set<std::string> kPlsqlLineKeywords = {
    "IF", "ELSIF", "ELSE", "THEN", "WHEN", "END", "FOR", "WHILE",
    "RETURN", "RAISE",
};

std::vector<std::string> SqlTokenize(std::string_view sql) {
    std::vector<std::string> tokens;
    size_t len = sql.size();
    size_t i = 0;

    while (i < len) {
        if (sql[i] == ' ' || sql[i] == '\t' || sql[i] == '\n' || sql[i] == '\r') { i++; continue; }

        // String literal
        if (sql[i] == '\'') {
            size_t j = i + 1;
            while (j < len) {
                if (sql[j] == '\'') {
                    j++;
                    if (j < len && sql[j] == '\'') { j++; continue; }
                    break;
                }
                j++;
            }
            tokens.emplace_back(sql.substr(i, j - i));
            i = j;
            continue;
        }

        // Quoted identifier [name]
        if (sql[i] == '[') {
            size_t j = i + 1;
            while (j < len && sql[j] != ']') j++;
            if (j < len) j++;
            tokens.emplace_back(sql.substr(i, j - i));
            i = j;
            continue;
        }
        // Quoted identifier "name"
        if (sql[i] == '"') {
            size_t j = i + 1;
            while (j < len && sql[j] != '"') j++;
            if (j < len) j++;
            tokens.emplace_back(sql.substr(i, j - i));
            i = j;
            continue;
        }

        // Colon: := or :name/:1
        if (sql[i] == ':') {
            if (i + 1 < len && sql[i + 1] == '=') {
                tokens.emplace_back(":=");
                i += 2;
                continue;
            }
            size_t j = i + 1;
            while (j < len && (isalnum(static_cast<unsigned char>(sql[j])) || sql[j] == '_')) j++;
            tokens.emplace_back(sql.substr(i, j - i));
            i = j;
            continue;
        }

        // Word / identifier / number
        if (isalpha(static_cast<unsigned char>(sql[i])) || sql[i] == '_' ||
            sql[i] == '@' || sql[i] == '#' || isdigit(static_cast<unsigned char>(sql[i]))) {
            size_t j = i + 1;
            while (j < len && (isalnum(static_cast<unsigned char>(sql[j])) || sql[j] == '_' ||
                               sql[j] == '.' || sql[j] == '@' || sql[j] == '#')) j++;
            tokens.emplace_back(sql.substr(i, j - i));
            i = j;
            continue;
        }

        // Multi-char operators
        if (i + 1 < len) {
            char c0 = sql[i], c1 = sql[i + 1];
            if ((c0 == '>' && c1 == '=') || (c0 == '<' && c1 == '=') ||
                (c0 == '<' && c1 == '>') || (c0 == '!' && c1 == '=') ||
                (c0 == '|' && c1 == '|')) {
                tokens.emplace_back(sql.substr(i, 2));
                i += 2;
                continue;
            }
        }

        // Single char
        tokens.emplace_back(1, sql[i]);
        i++;
    }

    return tokens;
}

std::string SqlFormat(std::string_view sql) {
    // Trim
    while (!sql.empty() && (sql.front() == ' ' || sql.front() == '\n' || sql.front() == '\r' || sql.front() == '\t'))
        sql.remove_prefix(1);
    while (!sql.empty() && (sql.back() == ' ' || sql.back() == '\n' || sql.back() == '\r' || sql.back() == '\t'))
        sql.remove_suffix(1);
    if (sql.empty()) return {};

    auto tokens = SqlTokenize(sql);
    std::string result;
    int indent = 0;
    std::string prevUpper;
    bool lineStart = true;

    auto indentStr = [](int n) -> std::string {
        std::string s;
        for (int i = 0; i < n; ++i) s += "    ";
        return s;
    };

    for (const auto& token : tokens) {
        std::string upper = ToUpper(token);

        if (token == ";") {
            result += ";";
            result += "\n" + indentStr(indent);
            prevUpper = upper;
            lineStart = true;
            continue;
        }

        if (token == "(") {
            result += token;
            indent++;
            prevUpper = upper;
            lineStart = false;
            continue;
        }

        if (token == ")") {
            indent = std::max(0, indent - 1);
            result += token;
            prevUpper = upper;
            lineStart = false;
            continue;
        }

        if (token == ",") {
            result += ",";
            result += "\n" + indentStr(indent + 1);
            prevUpper = upper;
            lineStart = true;
            continue;
        }

        if (kBlockOpenKeywords.count(upper)) {
            if (!result.empty()) result += "\n" + indentStr(indent);
            result += token;
            indent++;
            prevUpper = upper;
            lineStart = false;
            continue;
        }

        if (upper == "END") {
            indent = std::max(0, indent - 1);
            result += "\n" + indentStr(indent);
            result += token;
            prevUpper = upper;
            lineStart = false;
            continue;
        }

        if (kPlsqlLineKeywords.count(upper)) {
            result += "\n" + indentStr(indent);
            result += token;
            prevUpper = upper;
            lineStart = false;
            continue;
        }

        if (kTopKeywords.count(upper)) {
            if (upper == "BY" && (prevUpper == "ORDER" || prevUpper == "GROUP")) {
                result += " " + token;
                prevUpper = upper;
                lineStart = false;
                continue;
            }
            if (!result.empty()) result += "\n" + indentStr(indent);
            result += token;
            prevUpper = upper;
            lineStart = false;
            continue;
        }

        if (kSubKeywords.count(upper)) {
            if (upper == "JOIN" && (prevUpper == "LEFT" || prevUpper == "RIGHT" ||
                                    prevUpper == "INNER" || prevUpper == "OUTER" ||
                                    prevUpper == "CROSS" || prevUpper == "FULL")) {
                result += " " + token;
                prevUpper = upper;
                lineStart = false;
                continue;
            }
            result += "\n" + indentStr(indent) + "  " + token;
            prevUpper = upper;
            lineStart = false;
            continue;
        }

        if (!lineStart && !result.empty()) result += " ";
        result += token;
        prevUpper = upper;
        lineStart = false;
    }

    return result;
}
