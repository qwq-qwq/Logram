#include "ui/App.h"
#include "ui/MainWindow.h"

#include <gtk/gtk.h>

namespace {

void OnActivate(GtkApplication* app, gpointer /*user_data*/) {
    GtkWidget* window = MainWindow::Create(app);
    gtk_window_present(GTK_WINDOW(window));
}

} // namespace

int App::Run(int argc, char** argv) {
    GtkApplication* app = gtk_application_new("com.unitybase.Logram",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(OnActivate), nullptr);
    const int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}