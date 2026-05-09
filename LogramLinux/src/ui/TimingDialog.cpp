#include "ui/TimingDialog.h"
#include "ui/Theme.h"
#include "core/LogDocument.h"
#include "core/LogLine.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

// ---- TimingRow GObject (one entry in the dialog's list model) -----------

struct _TimingRow {
    GObject parent_instance;
    int lineId;
    int thread;
    double durationMS;
    char* method;
};

#define LOGRAM_TYPE_TIMING_ROW (timing_row_get_type())
G_DECLARE_FINAL_TYPE(TimingRow, timing_row, LOGRAM, TIMING_ROW, GObject)
G_DEFINE_TYPE(TimingRow, timing_row, G_TYPE_OBJECT)

static void timing_row_finalize(GObject* obj) {
    TimingRow* row = LOGRAM_TIMING_ROW(obj);
    g_free(row->method);
    G_OBJECT_CLASS(timing_row_parent_class)->finalize(obj);
}

static void timing_row_init(TimingRow* /*self*/) {}
static void timing_row_class_init(TimingRowClass* klass) {
    G_OBJECT_CLASS(klass)->finalize = timing_row_finalize;
}

static TimingRow* timing_row_new(int lineId, int thread, double durMS,
                                 const char* method) {
    auto* row = LOGRAM_TIMING_ROW(g_object_new(LOGRAM_TYPE_TIMING_ROW, nullptr));
    row->lineId = lineId;
    row->thread = thread;
    row->durationMS = durMS;
    row->method = g_strdup(method ? method : "");
    return row;
}

// ---- Cell formatters -----------------------------------------------------

namespace {

using CellFn = void (*)(GtkLabel* label, TimingRow* row);

void FormatLineCell(GtkLabel* l, TimingRow* r) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", r->lineId + 1);
    gtk_label_set_text(l, buf);
}
void FormatThreadCell(GtkLabel* l, TimingRow* r) {
    if (r->thread < 0) { gtk_label_set_text(l, ""); return; }
    const char ch[2] = { static_cast<char>(0x21 + r->thread), '\0' };
    char* markup = g_markup_printf_escaped(
        "<span foreground=\"%s\" weight=\"bold\">%s</span>",
        ThreadHexColor(r->thread), ch);
    gtk_label_set_markup(l, markup);
    g_free(markup);
}
void FormatDurationCell(GtkLabel* l, TimingRow* r) {
    const int64_t us = static_cast<int64_t>(r->durationMS * 1000.0);
    const std::string text = FormatDuration(us);
    const char* color = "#a9b1d6";
    if      (us >= 10'000'000) color = "#f7768e";
    else if (us >=  1'000'000) color = "#e0af68";
    else if (us >=    100'000) color = "#7aa2f7";
    char* markup = g_markup_printf_escaped(
        "<span foreground=\"%s\">%s</span>", color, text.c_str());
    gtk_label_set_markup(l, markup);
    g_free(markup);
}
void FormatMethodCell(GtkLabel* l, TimingRow* r) {
    gtk_label_set_text(l, r->method);
}

void OnFactorySetup(GtkSignalListItemFactory*, GtkListItem* item, gpointer) {
    GtkWidget* label = gtk_label_new(nullptr);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_list_item_set_child(item, label);
}

void OnFactoryBind(GtkSignalListItemFactory*, GtkListItem* item, gpointer ud) {
    auto fn = reinterpret_cast<CellFn>(ud);
    GtkLabel* lbl = GTK_LABEL(gtk_list_item_get_child(item));
    TimingRow* row = LOGRAM_TIMING_ROW(gtk_list_item_get_item(item));
    if (lbl && row) fn(lbl, row);
}

GtkColumnViewColumn* MakeColumn(const char* title, CellFn fn,
                                int fixedWidth, bool expand) {
    GtkListItemFactory* f = gtk_signal_list_item_factory_new();
    g_signal_connect(f, "setup", G_CALLBACK(OnFactorySetup), nullptr);
    g_signal_connect(f, "bind",  G_CALLBACK(OnFactoryBind),
                     reinterpret_cast<gpointer>(fn));
    GtkColumnViewColumn* col = gtk_column_view_column_new(title, f);
    if (fixedWidth > 0) gtk_column_view_column_set_fixed_width(col, fixedWidth);
    gtk_column_view_column_set_expand(col, expand ? TRUE : FALSE);
    gtk_column_view_column_set_resizable(col, TRUE);
    return col;
}

void OnRowActivated(GtkColumnView* /*cv*/, guint position, gpointer user_data) {
    static_cast<TimingDialog*>(user_data)->ActivateRow(position);
}

} // namespace

// ---- TimingDialog --------------------------------------------------------

TimingDialog::TimingDialog(GtkWindow* parent, LogDocument* doc,
                           std::function<void(int)> onGoTo)
    : parent_(parent), doc_(doc), onGoTo_(std::move(onGoTo)) {}

void TimingDialog::Show() {
    if (!doc_) return;
    if (doc_->Timings().empty()) doc_->BuildMethodTimings();

    // Sort by duration descending and take the top 1000.
    std::vector<MethodTiming> sorted = doc_->Timings();
    std::sort(sorted.begin(), sorted.end(),
              [](const MethodTiming& a, const MethodTiming& b) {
                  return a.durationMS > b.durationMS;
              });
    if (sorted.size() > 1000) sorted.resize(1000);

    store_ = g_list_store_new(LOGRAM_TYPE_TIMING_ROW);
    for (const auto& t : sorted) {
        TimingRow* r = timing_row_new(static_cast<int>(t.lineId),
                                      t.thread, t.durationMS,
                                      t.method.c_str());
        g_list_store_append(store_, r);
        g_object_unref(r);
    }

    GtkSelectionModel* sel =
        GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(store_)));
    GtkWidget* cv = gtk_column_view_new(sel);
    gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(cv), FALSE);
    gtk_column_view_set_show_column_separators(GTK_COLUMN_VIEW(cv), FALSE);

    GtkColumnView* C = GTK_COLUMN_VIEW(cv);
    gtk_column_view_append_column(C, MakeColumn("Line",     FormatLineCell,      96, false));
    gtk_column_view_append_column(C, MakeColumn("Thread",   FormatThreadCell,    48, false));
    gtk_column_view_append_column(C, MakeColumn("Duration", FormatDurationCell,  96, false));
    gtk_column_view_append_column(C, MakeColumn("Method",   FormatMethodCell,     0, true));

    g_signal_connect(cv, "activate", G_CALLBACK(OnRowActivated), this);

    GtkWidget* scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), cv);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_widget_set_hexpand(scroller, TRUE);

    window_ = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window_), "Method Timing — top 1000 slowest");
    gtk_window_set_default_size(GTK_WINDOW(window_), 900, 600);
    gtk_window_set_transient_for(GTK_WINDOW(window_), parent_);
    gtk_window_set_modal(GTK_WINDOW(window_), FALSE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(window_), TRUE);

    GtkWidget* hb = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window_), hb);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(vbox), scroller);

    char hint[128];
    std::snprintf(hint, sizeof(hint),
                  "%zu pair%s · double-click a row to jump to its line",
                  sorted.size(), sorted.size() == 1 ? "" : "s");
    GtkWidget* hintLbl = gtk_label_new(hint);
    gtk_widget_set_halign(hintLbl, GTK_ALIGN_START);
    gtk_widget_set_margin_start(hintLbl, 8);
    gtk_widget_set_margin_end(hintLbl,   8);
    gtk_widget_set_margin_top(hintLbl,   4);
    gtk_widget_set_margin_bottom(hintLbl, 4);
    gtk_widget_add_css_class(hintLbl, "dim-label");
    gtk_box_append(GTK_BOX(vbox), hintLbl);

    gtk_window_set_child(GTK_WINDOW(window_), vbox);

    // Self-destruct: free the wrapping object when the window closes.
    g_signal_connect_swapped(window_, "close-request",
        G_CALLBACK(+[](gpointer self) -> gboolean {
            delete static_cast<TimingDialog*>(self);
            return FALSE; // let GTK proceed with destroying the window
        }), this);

    gtk_window_present(GTK_WINDOW(window_));
}

void TimingDialog::ActivateRow(unsigned position) {
    if (!store_) return;
    auto* row = LOGRAM_TIMING_ROW(g_list_model_get_item(
        G_LIST_MODEL(store_), position));
    if (!row) return;
    if (onGoTo_) onGoTo_(row->lineId);
    g_object_unref(row);
}