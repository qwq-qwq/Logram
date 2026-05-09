#include "ui/FilterSidebar.h"
#include "ui/Theme.h"
#include "core/LogDocument.h"
#include "core/LogLevel.h"

#include <cstdio>
#include <cstdint>

namespace {

constexpr const char* kLevelIdKey  = "logram-level-id";
constexpr const char* kThreadIdKey = "logram-thread-id";

void OnLevelCheckToggled(GtkCheckButton* btn, gpointer user_data) {
    auto* self = static_cast<FilterSidebar*>(user_data);
    const int levelId = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(btn), kLevelIdKey));
    self->OnLevelToggled(levelId, gtk_check_button_get_active(btn));
}

void OnThreadCheckToggled(GtkCheckButton* btn, gpointer user_data) {
    auto* self = static_cast<FilterSidebar*>(user_data);
    const int threadId = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(btn), kThreadIdKey));
    self->OnThreadToggled(threadId, gtk_check_button_get_active(btn));
}

void OnAllLevelsClicked(GtkButton*, gpointer user_data) {
    static_cast<FilterSidebar*>(user_data)->OnAllLevels(true);
}
void OnNoLevelsClicked(GtkButton*, gpointer user_data) {
    static_cast<FilterSidebar*>(user_data)->OnAllLevels(false);
}
void OnAllThreadsClicked(GtkButton*, gpointer user_data) {
    static_cast<FilterSidebar*>(user_data)->OnAllThreads(true);
}
void OnNoThreadsClicked(GtkButton*, gpointer user_data) {
    static_cast<FilterSidebar*>(user_data)->OnAllThreads(false);
}

void OnRowRightClick(GtkGestureClick* /*g*/, int /*n*/,
                     double x, double y, gpointer user_data) {
    auto* check = GTK_WIDGET(g_object_get_data(G_OBJECT(user_data),
                                               "logram-row-widget"));
    auto* self  = static_cast<FilterSidebar*>(
        g_object_get_data(G_OBJECT(user_data), "logram-row-self"));
    if (self && check) self->ShowRowContextMenu(check, x, y);
}

GtkWidget* MakeSectionHeader(const char* text) {
    GtkWidget* label = gtk_label_new(nullptr);
    char markup[64];
    std::snprintf(markup, sizeof(markup),
                  "<b>%s</b>", text);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(label, 8);
    gtk_widget_set_margin_bottom(label, 4);
    return label;
}

GtkWidget* MakeAllNoneRow(FilterSidebar* self, bool forLevels) {
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* allBtn  = gtk_button_new_with_label("All");
    GtkWidget* noneBtn = gtk_button_new_with_label("None");
    gtk_widget_set_hexpand(allBtn,  TRUE);
    gtk_widget_set_hexpand(noneBtn, TRUE);
    gtk_widget_add_css_class(allBtn,  "flat");
    gtk_widget_add_css_class(noneBtn, "flat");
    g_signal_connect(allBtn,  "clicked",
        G_CALLBACK(forLevels ? OnAllLevelsClicked  : OnAllThreadsClicked), self);
    g_signal_connect(noneBtn, "clicked",
        G_CALLBACK(forLevels ? OnNoLevelsClicked   : OnNoThreadsClicked),  self);
    gtk_box_append(GTK_BOX(hbox), allBtn);
    gtk_box_append(GTK_BOX(hbox), noneBtn);
    return hbox;
}

} // namespace

FilterSidebar::FilterSidebar() {
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(vbox,  8);
    gtk_widget_set_margin_end(vbox,    8);
    gtk_widget_set_margin_top(vbox,    8);
    gtk_widget_set_margin_bottom(vbox, 8);

    gtk_box_append(GTK_BOX(vbox), MakeSectionHeader("Levels"));
    gtk_box_append(GTK_BOX(vbox), MakeAllNoneRow(this, /*forLevels=*/true));
    levelsBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(vbox), levelsBox_);

    gtk_box_append(GTK_BOX(vbox),
                   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    gtk_box_append(GTK_BOX(vbox), MakeSectionHeader("Threads"));
    gtk_box_append(GTK_BOX(vbox), MakeAllNoneRow(this, /*forLevels=*/false));
    threadsBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(vbox), threadsBox_);

    scroller_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller_),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller_), vbox);
    gtk_widget_set_size_request(scroller_, 220, -1);
}

FilterSidebar::~FilterSidebar() {
    if (contextPopover_) {
        gtk_widget_unparent(contextPopover_);
        contextPopover_ = nullptr;
    }
}

void FilterSidebar::ClearBox(GtkWidget* box) {
    GtkWidget* child = gtk_widget_get_first_child(box);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(box), child);
        child = next;
    }
}

void FilterSidebar::SetDocument(LogDocument* doc) {
    doc_ = doc;
    Rebuild();
}

void FilterSidebar::Refresh() {
    Rebuild();
}

void FilterSidebar::Rebuild() {
    ClearBox(levelsBox_);
    ClearBox(threadsBox_);
    if (!doc_) return;

    const uint64_t levelMask  = doc_->EnabledLevelMask();
    const uint64_t threadMask = doc_->EnabledThreadMask();
    const int* perLevel  = doc_->PerLevelCount();
    const int* perThread = doc_->PerThreadCount();

    for (int i = 0; i < kLogLevelCount; ++i) {
        if (perLevel[i] <= 0) continue;
        const auto& info = GetLogLevelInfo(static_cast<LogLevel>(i));
        const LogLevel level = static_cast<LogLevel>(i);

        GtkWidget* check = gtk_check_button_new();
        GtkWidget* label = gtk_label_new(nullptr);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        char* markup = g_markup_printf_escaped(
            "<span foreground=\"%s\">%s</span>"
            " <span foreground=\"#565f89\">(%d)</span>",
            LevelHexColor(level), info.label, perLevel[i]);
        gtk_label_set_markup(GTK_LABEL(label), markup);
        g_free(markup);
        gtk_check_button_set_child(GTK_CHECK_BUTTON(check), label);

        gtk_check_button_set_active(GTK_CHECK_BUTTON(check),
            (levelMask >> i) & 1ULL);
        g_object_set_data(G_OBJECT(check), kLevelIdKey, GINT_TO_POINTER(i));
        g_signal_connect(check, "toggled",
                         G_CALLBACK(OnLevelCheckToggled), this);

        // Right-click context menu on the row.
        GtkGesture* rclick = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rclick),
                                      GDK_BUTTON_SECONDARY);
        g_object_set_data(G_OBJECT(rclick), "logram-row-widget", check);
        g_object_set_data(G_OBJECT(rclick), "logram-row-self",   this);
        g_signal_connect(rclick, "pressed",
                         G_CALLBACK(OnRowRightClick), rclick);
        gtk_widget_add_controller(check, GTK_EVENT_CONTROLLER(rclick));

        gtk_box_append(GTK_BOX(levelsBox_), check);
    }

    for (int t : doc_->ActiveThreads()) {
        if (t < 0 || t >= kMaxThreads) continue;

        GtkWidget* check = gtk_check_button_new();
        GtkWidget* label = gtk_label_new(nullptr);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        const char ch[2] = { static_cast<char>(0x21 + t), '\0' };
        char* markup = g_markup_printf_escaped(
            "<span foreground=\"%s\" weight=\"bold\">%s</span>"
            "  <span foreground=\"#a9b1d6\">Thread %d</span>"
            " <span foreground=\"#565f89\">(%d)</span>",
            ThreadHexColor(t), ch, t, perThread[t]);
        gtk_label_set_markup(GTK_LABEL(label), markup);
        g_free(markup);
        gtk_check_button_set_child(GTK_CHECK_BUTTON(check), label);

        gtk_check_button_set_active(GTK_CHECK_BUTTON(check),
            (threadMask >> t) & 1ULL);
        g_object_set_data(G_OBJECT(check), kThreadIdKey, GINT_TO_POINTER(t));
        g_signal_connect(check, "toggled",
                         G_CALLBACK(OnThreadCheckToggled), this);

        GtkGesture* rclick = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rclick),
                                      GDK_BUTTON_SECONDARY);
        g_object_set_data(G_OBJECT(rclick), "logram-row-widget", check);
        g_object_set_data(G_OBJECT(rclick), "logram-row-self",   this);
        g_signal_connect(rclick, "pressed",
                         G_CALLBACK(OnRowRightClick), rclick);
        gtk_widget_add_controller(check, GTK_EVENT_CONTROLLER(rclick));

        gtk_box_append(GTK_BOX(threadsBox_), check);
    }
}

void FilterSidebar::NotifyChanged() {
    if (!doc_) return;
    doc_->ApplyFilters();
    if (onChanged_) onChanged_();
}

void FilterSidebar::OnLevelToggled(int levelId, bool active) {
    if (!doc_) return;
    uint64_t mask = doc_->EnabledLevelMask();
    if (active) mask |=  (1ULL << levelId);
    else        mask &= ~(1ULL << levelId);
    doc_->SetEnabledLevelMask(mask);
    NotifyChanged();
}

void FilterSidebar::OnThreadToggled(int threadId, bool active) {
    if (!doc_) return;
    uint64_t mask = doc_->EnabledThreadMask();
    if (active) mask |=  (1ULL << threadId);
    else        mask &= ~(1ULL << threadId);
    doc_->SetEnabledThreadMask(mask);
    NotifyChanged();
}

void FilterSidebar::OnAllLevels(bool enable) {
    if (!doc_) return;
    doc_->SetEnabledLevelMask(enable ? ~uint64_t(0) : 0);
    Rebuild();
    NotifyChanged();
}

void FilterSidebar::OnAllThreads(bool enable) {
    if (!doc_) return;
    doc_->SetEnabledThreadMask(enable ? ~uint64_t(0) : 0);
    Rebuild();
    NotifyChanged();
}

void FilterSidebar::OnLevelOnlyThis(int levelId) {
    if (!doc_) return;
    doc_->SetEnabledLevelMask(uint64_t(1) << levelId);
    Rebuild();
    NotifyChanged();
}

void FilterSidebar::OnThreadOnlyThis(int threadId) {
    if (!doc_) return;
    doc_->SetEnabledThreadMask(uint64_t(1) << threadId);
    Rebuild();
    NotifyChanged();
}

void FilterSidebar::ShowRowContextMenu(GtkWidget* row, double x, double y) {
    if (!row || !scroller_) return;

    // Determine whether this row belongs to Levels or Threads.
    const bool isLevel =
        g_object_get_qdata(G_OBJECT(row),
                           g_quark_from_static_string(kLevelIdKey)) != nullptr;
    const int id = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(row),
                          isLevel ? kLevelIdKey : kThreadIdKey));

    // The popover is parented once to the stable scroller_. Parenting it to
    // the row instead would tie its lifetime to that row — and the moment the
    // user picks any menu item we Rebuild() the rows, destroying the popover
    // mid-callback and leaving contextPopover_ dangling.
    if (!contextPopover_) {
        contextPopover_ = gtk_popover_new();
        gtk_popover_set_has_arrow(GTK_POPOVER(contextPopover_), FALSE);
        gtk_widget_set_parent(contextPopover_, scroller_);
    }

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 4);
    gtk_widget_set_margin_end(box,   4);
    gtk_widget_set_margin_top(box,   4);
    gtk_widget_set_margin_bottom(box,4);

    auto makeBtn = [&](const char* label, std::function<void()> action) {
        GtkWidget* b = gtk_button_new_with_label(label);
        gtk_widget_add_css_class(b, "flat");
        gtk_widget_set_halign(b, GTK_ALIGN_FILL);
        auto* heap = new std::function<void()>(std::move(action));
        g_signal_connect_data(b, "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer ud) {
                auto* fn = static_cast<std::function<void()>*>(ud);
                (*fn)();
            }),
            heap,
            +[](gpointer ud, GClosure*) {
                delete static_cast<std::function<void()>*>(ud);
            },
            G_CONNECT_DEFAULT);
        gtk_box_append(GTK_BOX(box), b);
        return b;
    };

    // Hide before invoking the action so the popover is not asked to popdown
    // after Rebuild() may have already replaced its children.
    if (isLevel) {
        makeBtn("All",       [this]{
            gtk_popover_popdown(GTK_POPOVER(contextPopover_));
            OnAllLevels(true);
        });
        makeBtn("None",      [this]{
            gtk_popover_popdown(GTK_POPOVER(contextPopover_));
            OnAllLevels(false);
        });
        makeBtn("Only this", [this, id]{
            gtk_popover_popdown(GTK_POPOVER(contextPopover_));
            OnLevelOnlyThis(id);
        });
    } else {
        makeBtn("All",       [this]{
            gtk_popover_popdown(GTK_POPOVER(contextPopover_));
            OnAllThreads(true);
        });
        makeBtn("None",      [this]{
            gtk_popover_popdown(GTK_POPOVER(contextPopover_));
            OnAllThreads(false);
        });
        makeBtn("Only this", [this, id]{
            gtk_popover_popdown(GTK_POPOVER(contextPopover_));
            OnThreadOnlyThis(id);
        });
    }

    gtk_popover_set_child(GTK_POPOVER(contextPopover_), box);

    // Translate gesture coords from the row's space into scroller_ space so
    // the popover anchors near the actual click instead of the scroller's
    // origin.
    double sx = 0, sy = 0;
    graphene_point_t in;
    in.x = static_cast<float>(x);
    in.y = static_cast<float>(y);
    graphene_point_t out;
    if (gtk_widget_compute_point(row, scroller_, &in, &out)) {
        sx = out.x;
        sy = out.y;
    }
    const GdkRectangle rect = {static_cast<int>(sx), static_cast<int>(sy), 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(contextPopover_), &rect);
    gtk_popover_popup(GTK_POPOVER(contextPopover_));
}