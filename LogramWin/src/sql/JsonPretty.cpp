#include "sql/JsonPretty.h"
#include <cctype>

std::string FormatTruncatedJSON(std::string_view text) {
    std::string result;
    int depth = 0;
    bool inString = false;
    size_t len = text.size();

    auto addIndent = [&] {
        result += '\n';
        for (int i = 0; i < depth; ++i) result += "  ";
    };

    for (size_t i = 0; i < len; ++i) {
        char c = text[i];

        if (c == '"' && (i == 0 || text[i - 1] != '\\')) {
            inString = !inString;
            result += c;
            continue;
        }

        if (inString) {
            result += c;
            continue;
        }

        switch (c) {
            case '{': case '[':
                result += c;
                depth++;
                if (depth <= 2) addIndent();
                break;
            case '}': case ']':
                depth = (depth > 0) ? depth - 1 : 0;
                if (depth < 2) addIndent();
                result += c;
                break;
            case ',':
                result += c;
                if (depth <= 2) addIndent();
                break;
            default:
                result += c;
                break;
        }
    }
    return result;
}

// Try simple pretty-print by re-indenting valid JSON-like structure
std::string JsonPrettyPrint(std::string_view text) {
    // Trim
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);

    if (text.empty()) return {};

    // Check if it starts with { or [ — attempt structured formatting
    if (text.front() == '{' || text.front() == '[') {
        // Full re-indent pass
        std::string result;
        int depth = 0;
        bool inString = false;
        size_t len = text.size();

        auto addIndent = [&] {
            result += '\n';
            for (int i = 0; i < depth; ++i) result += "  ";
        };

        for (size_t i = 0; i < len; ++i) {
            char c = text[i];

            if (c == '"' && (i == 0 || text[i - 1] != '\\')) {
                inString = !inString;
                result += c;
                continue;
            }
            if (inString) { result += c; continue; }

            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;

            switch (c) {
                case '{': case '[':
                    result += c;
                    depth++;
                    addIndent();
                    break;
                case '}': case ']':
                    depth = (depth > 0) ? depth - 1 : 0;
                    addIndent();
                    result += c;
                    break;
                case ',':
                    result += c;
                    addIndent();
                    break;
                case ':':
                    result += ": ";
                    break;
                default:
                    result += c;
                    break;
            }
        }
        return result;
    }

    return FormatTruncatedJSON(text);
}

static bool IsJSStackEntry(std::string_view token) {
    auto atPos = token.find('@');
    if (atPos == std::string_view::npos || atPos == 0) return false;
    auto path = token.substr(atPos + 1);
    if (path.find('/') == std::string_view::npos) return false;
    return path.find(".js:") != std::string_view::npos ||
           path.find(".ts:") != std::string_view::npos ||
           path.find(".mjs:") != std::string_view::npos;
}

std::string FormatStackTrace(std::string_view text) {
    std::string result;
    size_t len = text.size();
    size_t i = 0;

    while (i < len) {
        if (text[i] == ' ' && i + 1 < len && text[i + 1] != ' ' && text[i + 1] != '\n') {
            // Peek at next token
            size_t j = i + 1;
            while (j < len && text[j] != ' ' && text[j] != '\n') j++;
            auto token = text.substr(i + 1, j - (i + 1));
            if (IsJSStackEntry(token)) {
                result += "\n  ";
                i++;
                continue;
            }
        }
        result += text[i];
        i++;
    }
    return result;
}
