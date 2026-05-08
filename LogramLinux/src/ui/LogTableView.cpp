#include "ui/LogTableView.h"
#include "core/LogDocument.h"
#include "core/LogLevel.h"
#include "core/LogLine.h"

#include <cstdio>
#include <string>
#include <string_view>

// ---------------- LogRow: GObject wrapping (doc*, lineId) ----------------

struct _LogRow {
    GObject parent_instance;
    LogDocument* doc;
    guint lineId;
};

#define LOGRAM_TYPE_ROW (log_row_get_type())
G_DECLARE_FINAL_TYPE(LogRow, log_row, LOGRAM, ROW, GObject)
G_DEFINE_TYPE(LogRow, log_row, G_TYPE_OBJECT)

static void log_row_init(LogRow* /*self*/) {}
static void log_row_class_init(LogRowClass* /*klass*/) {}

static LogRow* log_row_new(LogDocument* doc, guint lineId) {
    LogRow* row = LOGRAM_ROW(g_object_new(LOGRAM_TYPE_ROW, nullptr));
    row->doc = doc;
    row->lineId = lineId;
    return row;
}

// -------------- LogRowModel: GListModel backed by LogDocument --------------

struct _LogRowModel {
    GObject parent_instance;
    LogDocument* doc;
};

#define LOGRAM_TYPE_ROW_MODEL (log_row_model_get_type())
G_DECLARE_FINAL_TYPE(LogRowModel, log_row_model, LOGRAM, ROW_MODEL, GObject)

static void log_row_model_list_model_init(GListModelInterface* iface);

G_DEFINE_TYPE_WITH_CODE(LogRowModel, log_row_model, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, log_row_model_list_model_init))

static void log_row_model_init(LogRowModel* /*self*/) {}
static void log_row_model_class_init(LogRowModelClass* /*klass*/) {}

static GType lrm_get_item_type(GListModel* /*model*/) {
    return LOGRAM_TYPE_ROW;
}

static guint lrm_get_n_items(GListModel* model) {
    auto* self = LOGRAM_ROW_MODEL(model);
    if (!self->doc) return 0;
    return static_cast<guint>(self->doc->FilteredIndices().size());
}

static gpointer lrm_get_item(GListModel* model, guint position) {
    auto* self = LOGRAM_ROW_MODEL(model);
    if (!self->doc) return nullptr;
    const auto& filtered = self->doc->FilteredIndices();
    if (position >= filtered.size()) return nullptr;
    const uint32_t lineId = filtered[position];
    return log_row_new(self->doc, lineId);
}

static void log_row_model_list_model_init(GListModelInterface* iface) {
    iface->get_item_type = lrm_get_item_type;
    iface->get_n_items   = lrm_get_n_items;
    iface->get_item      = lrm_get_item;
}

static LogRowModel* log_row_model_new() {
    return LOGRAM_ROW_MODEL(g_object_new(LOGRAM_TYPE_ROW_MODEL, nullptr));
}

// ---------------- Cell formatting helpers ----------------

namespace {

// Each callback formats one column for a given (doc, lineId) into the cell label.
using CellFormatter = void (*)(GtkLabel* label, LogDocument* doc,
                               guint lineId, guint position);

void FormatLineNoCell(GtkLabel* label, LogDocument*, guint, guint position) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u", position + 1);
    gtk_label_set_text(label, buf);
}

void FormatThreadCell(GtkLabel* label, LogDocument* doc, guint lineId, guint) {
    const auto& line = doc->AllLines()[lineId];
    if (line.thread < 0) {
        gtk_label_set_text(label, "");
    } else {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", line.thread);
        gtk_label_set_text(label, buf);
    }
}

void FormatLevelCell(GtkLabel* label, LogDocument* doc, guint lineId, guint) {
    const auto& line = doc->AllLines()[lineId];
    const auto& info = GetLogLevelInfo(static_cast<LogLevel>(line.level));
    gtk_label_set_text(label, info.label);
}

void FormatTimeCell(GtkLabel* label, LogDocument* doc, guint lineId, guint) {
    const auto& line = doc->AllLines()[lineId];
    const std::string text = ::FormatTime(line.epochCS);
    gtk_label_set_text(label, text.c_str());
}

void FormatMessageCell(GtkLabel* label, LogDocument* doc, guint lineId, guint) {
    const auto& line = doc->AllLines()[lineId];
    const std::string_view msg = GetMessage(doc->MappedBase(), line);
    gtk_label_set_text(label, std::string(msg).c_str());
}

void OnFactorySetup(GtkSignalListItemFactory* /*factory*/,
                    GtkListItem* listItem, gpointer /*user_data*/) {
    GtkWidget* label = gtk_label_new(nullptr);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_list_item_set_child(listItem, label);
}

void OnFactoryBind(GtkSignalListItemFactory* /*factory*/,
                   GtkListItem* listItem, gpointer user_data) {
    auto formatter = reinterpret_cast<CellFormatter>(user_data);
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(listItem));
    LogRow* row = LOGRAM_ROW(gtk_list_item_get_item(listItem));
    if (!label || !row || !row->doc) return;
    formatter(label, row->doc, row->lineId, gtk_list_item_get_position(listItem));
}

GtkColumnViewColumn* MakeColumn(const char* title, CellFormatter formatter,
                                int fixedWidth, bool expand) {
    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(OnFactorySetup), nullptr);
    g_signal_connect(factory, "bind",  G_CALLBACK(OnFactoryBind),
                     reinterpret_cast<gpointer>(formatter));

    GtkColumnViewColumn* col = gtk_column_view_column_new(title, factory);
    if (fixedWidth > 0) {
        gtk_column_view_column_set_fixed_width(col, fixedWidth);
    }
    gtk_column_view_column_set_expand(col, expand ? TRUE : FALSE);
    gtk_column_view_column_set_resizable(col, TRUE);
    return col;
}

} // namespace

// ---------------- LogTableView ----------------

namespace {
void OnSelectionPropertyChanged(GObject* selection, GParamSpec* /*pspec*/,
                                gpointer user_data) {
    auto* self = static_cast<LogTableView*>(user_data);
    const guint pos = gtk_single_selection_get_selected(
        GTK_SINGLE_SELECTION(selection));
    self->OnSelectionChanged(pos);
}
} // namespace

LogTableView::LogTableView() {
    auto* rowModel = log_row_model_new();
    model_ = G_LIST_MODEL(rowModel);

    selection_ = gtk_single_selection_new(model_);
    // gtk_single_selection_new takes ownership of model_ ref; keep the pointer.
    g_signal_connect(selection_, "notify::selected",
                     G_CALLBACK(OnSelectionPropertyChanged), this);

    columnView_ = gtk_column_view_new(GTK_SELECTION_MODEL(selection_));
    gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(columnView_), FALSE);
    gtk_column_view_set_show_column_separators(GTK_COLUMN_VIEW(columnView_), FALSE);

    GtkColumnView* cv = GTK_COLUMN_VIEW(columnView_);
    gtk_column_view_append_column(cv, MakeColumn("#",       FormatLineNoCell,  72, false));
    gtk_column_view_append_column(cv, MakeColumn("Thread",  FormatThreadCell,  72, false));
    gtk_column_view_append_column(cv, MakeColumn("Level",   FormatLevelCell,   80, false));
    gtk_column_view_append_column(cv, MakeColumn("Time",    FormatTimeCell,   110, false));
    gtk_column_view_append_column(cv, MakeColumn("Message", FormatMessageCell, 0, true));

    scroller_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller_),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller_), columnView_);
    gtk_widget_set_hexpand(scroller_, TRUE);
    gtk_widget_set_vexpand(scroller_, TRUE);
}

LogTableView::~LogTableView() {
    // GtkWidget tree owns itself once attached to a window; nothing to free here.
}

void LogTableView::SetDocument(LogDocument* doc) {
    auto* rm = LOGRAM_ROW_MODEL(model_);
    rm->doc = doc;
    doc_ = doc;
    Refresh();
}

void LogTableView::Refresh() {
    const guint newCount = doc_ ? static_cast<guint>(doc_->FilteredIndices().size()) : 0;
    g_list_model_items_changed(model_, 0, lastCount_, newCount);
    lastCount_ = newCount;
}

void LogTableView::ScrollToPosition(unsigned position) {
    if (!doc_ || position >= doc_->FilteredIndices().size()) return;
    gtk_single_selection_set_selected(selection_, position);
    // Focusing the column view brings the focused (= selected) row into view.
    gtk_widget_grab_focus(columnView_);
}

void LogTableView::OnSelectionChanged(unsigned position) {
    if (!onSelection_) return;
    if (!doc_ || position == GTK_INVALID_LIST_POSITION) {
        onSelection_(-1);
        return;
    }
    const auto& filtered = doc_->FilteredIndices();
    if (position >= filtered.size()) {
        onSelection_(-1);
        return;
    }
    onSelection_(static_cast<int>(filtered[position]));
}