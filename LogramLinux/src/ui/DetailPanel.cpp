#include "ui/DetailPanel.h"
#include "core/LogDocument.h"
#include "core/LogLevel.h"
#include "core/LogLine.h"
#include "sql/SqlFormatter.h"
#include "sql/JsonPretty.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

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

    scroller_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller_),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller_), textView_);
}

DetailPanel::~DetailPanel() = default;

void DetailPanel::Clear() {
    gtk_text_buffer_set_text(buffer_, "", 0);
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

    char header[256];
    std::snprintf(header, sizeof(header),
                  "Line %d · Thread %d · %s · %s\n\n",
                  lineId + 1,
                  line.thread,
                  info.label,
                  ::FormatTime(line.epochCS).c_str());

    std::string body;
    if (LevelIsSqlLike(level)) {
        body = SqlFormat(message);
    } else if (LooksLikeJson(message)) {
        body = JsonPrettyPrint(message);
    } else {
        body.assign(message.data(), message.size());
    }

    std::string out;
    out.reserve(std::strlen(header) + body.size());
    out.append(header);
    out.append(body);

    gtk_text_buffer_set_text(buffer_, out.data(),
                             static_cast<int>(out.size()));
}