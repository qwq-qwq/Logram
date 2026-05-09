#include "ui/FilterSidebar.h"
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

FilterSidebar::~FilterSidebar() = default;

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
        char label[64];
        std::snprintf(label, sizeof(label), "%s (%d)", info.label, perLevel[i]);

        GtkWidget* check = gtk_check_button_new_with_label(label);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check),
            (levelMask >> i) & 1ULL);
        g_object_set_data(G_OBJECT(check), kLevelIdKey, GINT_TO_POINTER(i));
        g_signal_connect(check, "toggled",
                         G_CALLBACK(OnLevelCheckToggled), this);
        gtk_box_append(GTK_BOX(levelsBox_), check);
    }

    for (int t : doc_->ActiveThreads()) {
        if (t < 0 || t >= kMaxThreads) continue;
        char label[32];
        std::snprintf(label, sizeof(label), "Thread %d (%d)", t, perThread[t]);

        GtkWidget* check = gtk_check_button_new_with_label(label);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check),
            (threadMask >> t) & 1ULL);
        g_object_set_data(G_OBJECT(check), kThreadIdKey, GINT_TO_POINTER(t));
        g_signal_connect(check, "toggled",
                         G_CALLBACK(OnThreadCheckToggled), this);
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