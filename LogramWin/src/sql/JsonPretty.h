#pragma once
#include <string>
#include <string_view>

// Pretty-print JSON (full parse or truncated fallback)
std::string JsonPrettyPrint(std::string_view text);

// Simple formatting for truncated JSON
std::string FormatTruncatedJSON(std::string_view text);

// Format JS stack traces: space-separated `func@/path:line:col` entries → one per line
std::string FormatStackTrace(std::string_view text);
