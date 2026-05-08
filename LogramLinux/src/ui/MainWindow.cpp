#include "ui/MainWindow.h"

GtkWidget* MainWindow::Create(GtkApplication* app) {
    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Logram");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 800);

    GtkWidget* header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    GtkWidget* placeholder = gtk_label_new("Logram — Linux build (UI scaffold)");
    gtk_widget_set_halign(placeholder, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(placeholder, GTK_ALIGN_CENTER);
    gtk_window_set_child(GTK_WINDOW(window), placeholder);

    return window;
}