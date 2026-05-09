#pragma once

#include <gtk/gtk.h>
#include <string>

class LogDocument;

// Bottom panel showing the (formatted) text of the currently selected log
// line. Applies SQL or JSON syntax highlighting via GtkTextBuffer tags, and
// for SQL lines optionally substitutes bind parameters from the matching
// preceding Cust1 line on the same thread.
class DetailPanel {
public:
    DetailPanel();
    ~DetailPanel();

    DetailPanel(const DetailPanel&) = delete;
    DetailPanel& operator=(const DetailPanel&) = delete;

    GtkWidget* Widget() const { return root_; }

    void SetLine(LogDocument* doc, int lineId);
    void Clear();

    // Internal — toggled via the GtkCheckButton in the toolbar.
    void SetParamsEnabled(bool enabled);

private:
    void Render();
    void ApplyTagBytes(const char* tag, const std::string& src, int charBase,
                       size_t byteStart, size_t byteEnd);
    void HighlightSql(const std::string& src, int charBase);
    void HighlightJson(const std::string& src, int charBase);

    GtkWidget* root_ = nullptr;
    GtkWidget* paramsToggle_ = nullptr;
    GtkWidget* textView_ = nullptr;
    GtkTextBuffer* buffer_ = nullptr;
    LogDocument* doc_ = nullptr;
    int lineId_ = -1;
    bool paramsEnabled_ = true;
};