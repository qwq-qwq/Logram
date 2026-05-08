#include "ui/App.h"
#include "ui/MainWindow.h"

#include <gtk/gtk.h>

namespace {

void OnWindowDestroy(GtkWidget* /*window*/, gpointer user_data) {
    delete static_cast<MainWindow*>(user_data);
}

void OnActivate(GtkApplication* app, gpointer /*user_data*/) {
    auto* mw = new MainWindow(app);
    g_signal_connect(mw->Widget(), "destroy", G_CALLBACK(OnWindowDestroy), mw);
    gtk_window_present(GTK_WINDOW(mw->Widget()));
}

} // namespace

int App::Run(int argc, char** argv) {
    // G_APPLICATION_DEFAULT_FLAGS requires GLib >= 2.74; use FLAGS_NONE for 22.04 compat
    GtkApplication* app = gtk_application_new("com.unitybase.Logram",
                                              G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(OnActivate), nullptr);
    const int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}