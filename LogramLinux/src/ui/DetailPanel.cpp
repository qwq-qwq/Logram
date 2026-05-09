#include "ui/DetailPanel.h"
#include "core/LogDocument.h"
#include "core/LogLevel.h"
#include "core/LogLine.h"
#include "sql/SqlFormatter.h"
#include "sql/JsonPretty.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

bool LooksLikeJson(std::string_view s) {
    for (char c : s) {
        if (c == ' ' || c == '\t') continue;
        return c == '{' || c == '[';
    }
    return false;
}

bool LevelIsSqlLike(LogLevel level) {
    return level == LogLevel::Sql
        || level == LogLevel::Cache
        || level == LogLevel::Res
        || level == LogLevel::Db;
}

bool IsAlphaUnder(unsigned char c) { return std::isalpha(c) || c == '_'; }
bool IsAlnumUnder(unsigned char c) { return std::isalnum(c) || c == '_'; }

const std::unordered_set<std::string>& SqlKeywords() {
    static const std::unordered_set<std::string> kKw = {
        "SELECT", "FROM", "WHERE", "AND", "OR", "NOT", "INSERT", "UPDATE",
        "DELETE", "JOIN", "INNER", "LEFT", "RIGHT", "OUTER", "FULL", "CROSS",
        "ON", "USING", "GROUP", "BY", "ORDER", "HAVING", "LIMIT", "OFFSET",
        "AS", "DISTINCT", "UNION", "INTERSECT", "EXCEPT", "ALL", "IN",
        "BETWEEN", "LIKE", "ILIKE", "IS", "NULL", "TRUE", "FALSE", "CASE",
        "WHEN", "THEN", "ELSE", "END", "EXISTS", "WITH", "INTO", "VALUES",
        "SET", "RETURNING", "DESC", "ASC", "CAST", "COALESCE", "NULLIF",
    };
    return kKw;
}

} // namespace

DetailPanel::DetailPanel() {
    textView_ = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(textView_), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textView_), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(textView_), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textView_), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_margin_start(textView_,  6);
    gtk_widget_set_margin_end(textView_,    6);
    gtk_widget_set_margin_top(textView_,    6);
    gtk_widget_set_margin_bottom(textView_, 6);

    buffer_ = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textView_));

    // Highlight tags. Colors mirror LogramWin/src/ui/ThemeColors.cpp.
    gtk_text_buffer_create_tag(buffer_, "header",
        "foreground", "#565f89", nullptr);
    // SQL
    gtk_text_buffer_create_tag(buffer_, "sql-kw",
        "foreground", "#337aff", "weight", PANGO_WEIGHT_BOLD, nullptr);
    gtk_text_buffer_create_tag(buffer_, "sql-str",
        "foreground", "#33c759", nullptr);
    gtk_text_buffer_create_tag(buffer_, "sql-num",
        "foreground", "#ff9d0a", nullptr);
    gtk_text_buffer_create_tag(buffer_, "sql-com",
        "foreground", "#808080", "style", PANGO_STYLE_ITALIC, nullptr);
    gtk_text_buffer_create_tag(buffer_, "sql-bind",
        "foreground", "#ff2e54", nullptr);
    // JSON
    gtk_text_buffer_create_tag(buffer_, "json-key",
        "foreground", "#82a1c7", "weight", PANGO_WEIGHT_BOLD, nullptr);
    gtk_text_buffer_create_tag(buffer_, "json-str",
        "foreground", "#82ab82", nullptr);
    gtk_text_buffer_create_tag(buffer_, "json-num",
        "foreground", "#d1ad63", nullptr);
    gtk_text_buffer_create_tag(buffer_, "json-bool",
        "foreground", "#a191bf", nullptr);

    scroller_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller_),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller_), textView_);
}

DetailPanel::~DetailPanel() = default;

void DetailPanel::Clear() {
    gtk_text_buffer_set_text(buffer_, "", 0);
}

void DetailPanel::ApplyTagBytes(const char* tag, const std::string& src,
                                int charBase, size_t byteStart, size_t byteEnd) {
    if (byteStart >= byteEnd || byteEnd > src.size()) return;
    const int cs = static_cast<int>(g_utf8_pointer_to_offset(
        src.c_str(), src.c_str() + byteStart));
    const int ce = static_cast<int>(g_utf8_pointer_to_offset(
        src.c_str(), src.c_str() + byteEnd));
    GtkTextIter s, e;
    gtk_text_buffer_get_iter_at_offset(buffer_, &s, charBase + cs);
    gtk_text_buffer_get_iter_at_offset(buffer_, &e, charBase + ce);
    gtk_text_buffer_apply_tag_by_name(buffer_, tag, &s, &e);
}

void DetailPanel::HighlightSql(const std::string& src, int charBase) {
    const auto& kw = SqlKeywords();
    size_t i = 0;
    while (i < src.size()) {
        const unsigned char c = static_cast<unsigned char>(src[i]);

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }

        // -- line comment
        if (c == '-' && i + 1 < src.size() && src[i + 1] == '-') {
            const size_t start = i;
            while (i < src.size() && src[i] != '\n') ++i;
            ApplyTagBytes("sql-com", src, charBase, start, i);
            continue;
        }
        // /* block comment */
        if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            const size_t start = i;
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) ++i;
            if (i + 1 < src.size()) i += 2;
            ApplyTagBytes("sql-com", src, charBase, start, i);
            continue;
        }
        // 'string'
        if (c == '\'') {
            const size_t start = i;
            ++i;
            while (i < src.size()) {
                if (src[i] == '\'') {
                    if (i + 1 < src.size() && src[i + 1] == '\'') i += 2;
                    else { ++i; break; }
                } else {
                    ++i;
                }
            }
            ApplyTagBytes("sql-str", src, charBase, start, i);
            continue;
        }
        // :bind
        if (c == ':' && i + 1 < src.size() &&
            IsAlnumUnder(static_cast<unsigned char>(src[i + 1]))) {
            const size_t start = i;
            ++i;
            while (i < src.size() &&
                   IsAlnumUnder(static_cast<unsigned char>(src[i]))) ++i;
            ApplyTagBytes("sql-bind", src, charBase, start, i);
            continue;
        }
        // Number
        if (std::isdigit(c)) {
            const size_t start = i;
            while (i < src.size() &&
                   (std::isdigit(static_cast<unsigned char>(src[i])) ||
                    src[i] == '.')) ++i;
            ApplyTagBytes("sql-num", src, charBase, start, i);
            continue;
        }
        // Word → maybe keyword
        if (IsAlphaUnder(c)) {
            const size_t start = i;
            while (i < src.size() &&
                   IsAlnumUnder(static_cast<unsigned char>(src[i]))) ++i;
            std::string upper(src.data() + start, i - start);
            for (char& ch : upper) {
                ch = static_cast<char>(std::toupper(
                    static_cast<unsigned char>(ch)));
            }
            if (kw.count(upper)) {
                ApplyTagBytes("sql-kw", src, charBase, start, i);
            }
            continue;
        }
        ++i;
    }
}

void DetailPanel::HighlightJson(const std::string& src, int charBase) {
    size_t i = 0;
    while (i < src.size()) {
        const unsigned char c = static_cast<unsigned char>(src[i]);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }

        // "string"
        if (c == '"') {
            const size_t start = i;
            ++i;
            while (i < src.size() && src[i] != '"') {
                if (src[i] == '\\' && i + 1 < src.size()) i += 2;
                else ++i;
            }
            if (i < src.size()) ++i; // skip closing "
            // Lookahead — is this a key (followed by ':')?
            size_t k = i;
            while (k < src.size() &&
                   (src[k] == ' ' || src[k] == '\t' || src[k] == '\n')) ++k;
            const char* tag = (k < src.size() && src[k] == ':') ? "json-key"
                                                                : "json-str";
            ApplyTagBytes(tag, src, charBase, start, i);
            continue;
        }
        // Number (incl. leading '-')
        if (std::isdigit(c) ||
            (c == '-' && i + 1 < src.size() &&
             std::isdigit(static_cast<unsigned char>(src[i + 1])))) {
            const size_t start = i;
            ++i;
            while (i < src.size()) {
                const unsigned char x = static_cast<unsigned char>(src[i]);
                if (std::isdigit(x) || x == '.' || x == 'e' || x == 'E' ||
                    x == '+' || x == '-') ++i;
                else break;
            }
            ApplyTagBytes("json-num", src, charBase, start, i);
            continue;
        }
        // true / false / null
        if (c == 't' || c == 'f' || c == 'n') {
            const size_t start = i;
            while (i < src.size() &&
                   std::isalpha(static_cast<unsigned char>(src[i]))) ++i;
            std::string_view word(src.data() + start, i - start);
            if (word == "true" || word == "false" || word == "null") {
                ApplyTagBytes("json-bool", src, charBase, start, i);
            }
            continue;
        }
        ++i;
    }
}

void DetailPanel::SetLine(LogDocument* doc, int lineId) {
    if (!doc || lineId < 0 ||
        static_cast<size_t>(lineId) >= doc->AllLines().size()) {
        Clear();
        return;
    }

    const auto& line = doc->AllLines()[static_cast<size_t>(lineId)];
    const LogLevel level = static_cast<LogLevel>(line.level);
    const auto& info = GetLogLevelInfo(level);
    const std::string_view message = GetMessage(doc->MappedBase(), line);

    char headerBuf[256];
    std::snprintf(headerBuf, sizeof(headerBuf),
                  "Line %d · Thread %d · %s · %s\n\n",
                  lineId + 1,
                  line.thread,
                  info.label,
                  ::FormatTime(line.epochCS).c_str());
    const std::string header(headerBuf);

    std::string body;
    bool isSql = false, isJson = false;
    if (LevelIsSqlLike(level)) {
        body = SqlFormat(message);
        isSql = true;
    } else if (LooksLikeJson(message)) {
        body = JsonPrettyPrint(message);
        isJson = true;
    } else {
        body.assign(message.data(), message.size());
    }

    std::string out;
    out.reserve(header.size() + body.size());
    out.append(header);
    out.append(body);
    gtk_text_buffer_set_text(buffer_, out.data(),
                             static_cast<int>(out.size()));

    // Header tag spans the first chunk (in characters).
    const int headerChars = static_cast<int>(
        g_utf8_strlen(header.c_str(), static_cast<gssize>(header.size())));
    GtkTextIter h0, h1;
    gtk_text_buffer_get_iter_at_offset(buffer_, &h0, 0);
    gtk_text_buffer_get_iter_at_offset(buffer_, &h1, headerChars);
    gtk_text_buffer_apply_tag_by_name(buffer_, "header", &h0, &h1);

    if (isSql)        HighlightSql(body, headerChars);
    else if (isJson)  HighlightJson(body, headerChars);
}