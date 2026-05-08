#pragma once

#include <gtk/gtk.h>

class LogDocument;

// Bottom panel showing the raw text (and pretty-formatted SQL/JSON) of the
// currently selected log line. No syntax highlighting yet — that arrives
// alongside theming.
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
    GtkWidget* scroller_ = nullptr;
    GtkWidget* textView_ = nullptr;
    GtkTextBuffer* buffer_ = nullptr;
};