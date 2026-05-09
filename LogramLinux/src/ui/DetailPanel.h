#pragma once

#include <gtk/gtk.h>
#include <string>

class LogDocument;

// Bottom panel showing the (formatted) text of the currently selected log
// line. Applies SQL or JSON syntax highlighting via GtkTextBuffer tags.
class DetailPanel {
public:
    DetailPanel();
    ~DetailPanel();

    DetailPanel(const DetailPanel&) = delete;
    DetailPanel& operator=(const DetailPanel&) = delete;

    GtkWidget* Widget() const { return scroller_; }

    void SetLine(LogDocument* doc, int lineId);
    void Clear();

private:
    void ApplyTagBytes(const char* tag, const std::string& src, int charBase,
                       size_t byteStart, size_t byteEnd);
    void HighlightSql(const std::string& src, int charBase);
    void HighlightJson(const std::string& src, int charBase);

    GtkWidget* scroller_ = nullptr;
    GtkWidget* textView_ = nullptr;
    GtkTextBuffer* buffer_ = nullptr;
};