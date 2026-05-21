#include "ui/StatsDialog.h"
#include "ui/Theme.h"
#include "core/LogDocument.h"
#include "core/LogLevel.h"

#include <cstdio>
#include <string>

namespace {

std::string FormatSize(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1'000'000) {
        std::snprintf(buf, sizeof(buf), "%.1f MB",
                      static_cast<double>(bytes) / 1'000'000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f KB",
                      static_cast<double>(bytes) / 1'000.0);
    }
    return buf;
}

GtkWidget* MakeKeyLabel(const char* text) {
    GtkWidget* l = gtk_label_new(nullptr);
    char* markup = g_markup_printf_escaped(
        "<span foreground=\"#565f89\">%s</span>", text);
    gtk_label_set_markup(GTK_LABEL(l), markup);
    g_free(markup);
    gtk_widget_set_halign(l, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
    return l;
}

GtkWidget* MakeValueLabel(const std::string& text) {
    GtkWidget* l = gtk_label_new(text.c_str());
    gtk_widget_set_halign(l, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
    gtk_label_set_selectable(GTK_LABEL(l), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    return l;
}

void AppendKeyValue(GtkGrid* grid, int& row,
                    const char* key, const std::string& value,
                    const char* valueColor = nullptr) {
    gtk_grid_attach(grid, MakeKeyLabel(key), 0, row, 1, 1);
    GtkWidget* v;
    if (valueColor) {
        v = gtk_label_new(nullptr);
        char* markup = g_markup_printf_escaped(
            "<span foreground=\"%s\">%s</span>", valueColor, value.c_str());
        gtk_label_set_markup(GTK_LABEL(v), markup);
        g_free(markup);
        gtk_widget_set_halign(v, GTK_ALIGN_START);
        gtk_label_set_xalign(GTK_LABEL(v), 0.0f);
        gtk_label_set_selectable(GTK_LABEL(v), TRUE);
    } else {
        v = MakeValueLabel(value);
    }
    gtk_grid_attach(grid, v, 1, row, 1, 1);
    ++row;
}

void AppendSeparator(GtkGrid* grid, int& row) {
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 4);
    gtk_widget_set_margin_bottom(sep, 4);
    gtk_grid_attach(grid, sep, 0, row, 2, 1);
    ++row;
}

GtkWidget* MakeColorSwatch(const char* hex) {
    GtkWidget* sw = gtk_label_new("  ");
    GtkCssProvider* p = gtk_css_provider_new();
    char css[256];
    std::snprintf(css, sizeof(css),
                  "label {"
                  "  background-color: %s;"
                  "  border-radius: 2px;"
                  "  min-width: 10px;"
                  "  min-height: 10px;"
                  "}", hex);
    gtk_css_provider_load_from_string(p, css);
    gtk_style_context_add_provider(gtk_widget_get_style_context(sw),
        GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 10);
    g_object_unref(p);
    gtk_widget_set_size_request(sw, 10, 10);
    gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
    return sw;
}

} // namespace

StatsDialog::StatsDialog(GtkWindow* parent, LogDocument* doc)
    : parent_(parent), doc_(doc) {}

void StatsDialog::Show() {
    if (!doc_) return;

    window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window_), "Log Statistics");
    gtk_window_set_default_size(GTK_WINDOW(window_), 460, 600);
    gtk_window_set_transient_for(GTK_WINDOW(window_), parent_);
    gtk_window_set_modal(GTK_WINDOW(window_), FALSE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(window_), TRUE);

    GtkWidget* hb = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window_), hb);

    // Main grid for key/value rows.
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    int row = 0;

    GtkGrid* g = GTK_GRID(grid);
    AppendKeyValue(g, row, "File:",         doc_->FileName());
    AppendKeyValue(g, row, "Size:",         FormatSize(doc_->FileSize()));
    if (!doc_->UBVersion().empty())
        AppendKeyValue(g, row, "UB Version:", doc_->UBVersion());
    if (!doc_->HostInfo().empty())
        AppendKeyValue(g, row, "Host:",     doc_->HostInfo());
    {
        auto startStr = doc_->StartTimeFormatted();
        if (!startStr.empty())
            AppendKeyValue(g, row, "Start:", startStr);
        auto endStr = doc_->EndTimeFormatted();
        if (!endStr.empty())
            AppendKeyValue(g, row, "End:", endStr);
    }
    AppendKeyValue(g, row, "Duration:",     doc_->DurationFormatted());
    AppendKeyValue(g, row, "Total Events:", std::to_string(doc_->TotalEvents()));
    AppendKeyValue(g, row, "Threads:",      std::to_string(doc_->ActiveThreads().size()));

    AppendSeparator(g, row);
    AppendKeyValue(g, row, "HTTP Requests:", std::to_string(doc_->HttpRequests()));
    AppendKeyValue(g, row, "SQL Queries:",   std::to_string(doc_->SqlQueries()));
    {
        const int errs = doc_->ErrorCount();
        AppendKeyValue(g, row, "Errors:", std::to_string(errs),
                       errs > 0 ? "#f7768e" : nullptr);
    }

    const double durSec = doc_->DurationSeconds();
    if (durSec > 0) {
        AppendSeparator(g, row);
        // macOS divides HTTP by 2 (request + response are two http events
        // in a typical UB log); we mirror that for a comparable rate.
        const int httpPerSec = static_cast<int>(
            doc_->HttpRequests() / 2 / durSec);
        const int sqlPerSec  = static_cast<int>(
            doc_->SqlQueries() / durSec);
        AppendKeyValue(g, row, "HTTP req/sec:", std::to_string(httpPerSec));
        AppendKeyValue(g, row, "SQL q/sec:",    std::to_string(sqlPerSec));
    }

    // Per-level breakdown.
    GtkWidget* breakdownTitle = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(breakdownTitle),
                         "<b>Events by Level</b>");
    gtk_widget_set_halign(breakdownTitle, GTK_ALIGN_START);
    gtk_widget_set_margin_top(breakdownTitle, 8);
    gtk_widget_set_margin_bottom(breakdownTitle, 4);

    GtkWidget* levelsGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(levelsGrid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(levelsGrid), 2);
    {
        const int* perLevel = doc_->PerLevelCount();
        int lr = 0;
        for (int i = 0; i < kLogLevelCount; ++i) {
            if (perLevel[i] <= 0) continue;
            const auto& info = GetLogLevelInfo(static_cast<LogLevel>(i));
            const LogLevel level = static_cast<LogLevel>(i);

            GtkWidget* swatch = MakeColorSwatch(LevelHexColor(level));

            GtkWidget* labelLbl = gtk_label_new(nullptr);
            char* markup = g_markup_printf_escaped(
                "<span font_family=\"monospace\">%s</span>", info.label);
            gtk_label_set_markup(GTK_LABEL(labelLbl), markup);
            g_free(markup);
            gtk_widget_set_halign(labelLbl, GTK_ALIGN_START);

            GtkWidget* countLbl = gtk_label_new(nullptr);
            char* cmarkup = g_markup_printf_escaped(
                "<span font_family=\"monospace\">%d</span>", perLevel[i]);
            gtk_label_set_markup(GTK_LABEL(countLbl), cmarkup);
            g_free(cmarkup);
            gtk_widget_set_halign(countLbl, GTK_ALIGN_END);

            gtk_grid_attach(GTK_GRID(levelsGrid), swatch,   0, lr, 1, 1);
            gtk_grid_attach(GTK_GRID(levelsGrid), labelLbl, 1, lr, 1, 1);
            gtk_grid_attach(GTK_GRID(levelsGrid), countLbl, 2, lr, 1, 1);
            ++lr;
        }
    }

    // Top container: scrollable.
    GtkWidget* contentBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(contentBox,  20);
    gtk_widget_set_margin_end(contentBox,    20);
    gtk_widget_set_margin_top(contentBox,    20);
    gtk_widget_set_margin_bottom(contentBox, 20);
    gtk_box_append(GTK_BOX(contentBox), grid);
    gtk_box_append(GTK_BOX(contentBox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(contentBox), breakdownTitle);
    gtk_box_append(GTK_BOX(contentBox), levelsGrid);

    GtkWidget* scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), contentBox);

    // Footer: site link + Close.
    GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_start(footer,  20);
    gtk_widget_set_margin_end(footer,    20);
    gtk_widget_set_margin_top(footer,    8);
    gtk_widget_set_margin_bottom(footer, 12);
    GtkWidget* link = gtk_link_button_new_with_label(
        "https://logram.perek.rest", "logram.perek.rest");
    gtk_widget_add_css_class(link, "flat");
    gtk_widget_set_hexpand(link, TRUE);
    gtk_widget_set_halign(link, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(footer), link);

    GtkWidget* closeBtn = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(closeBtn, "clicked",
                             G_CALLBACK(gtk_window_close), window_);
    gtk_box_append(GTK_BOX(footer), closeBtn);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(root), scrolled);
    gtk_box_append(GTK_BOX(root), footer);
    gtk_window_set_child(GTK_WINDOW(window_), root);

    // Self-destruct on close.
    g_signal_connect_swapped(window_, "close-request",
        G_CALLBACK(+[](gpointer self) -> gboolean {
            delete static_cast<StatsDialog*>(self);
            return FALSE;
        }), this);

    gtk_window_present(GTK_WINDOW(window_));
}