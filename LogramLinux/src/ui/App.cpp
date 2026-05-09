#include "ui/App.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <gtk/gtk.h>

namespace {

void OnWindowDestroy(GtkWidget* /*window*/, gpointer user_data) {
    delete static_cast<MainWindow*>(user_data);
}

MainWindow* CreateWindow(GtkApplication* app) {
    Theme::Apply();
    auto* mw = new MainWindow(app);
    g_signal_connect(mw->Widget(), "destroy", G_CALLBACK(OnWindowDestroy), mw);
    return mw;
}

void OnActivate(GtkApplication* app, gpointer /*user_data*/) {
    MainWindow* mw = CreateWindow(app);
    gtk_window_present(GTK_WINDOW(mw->Widget()));
}

void OnOpen(GApplication* app, GFile** files, gint n_files,
            const gchar* /*hint*/, gpointer /*user_data*/) {
    MainWindow* mw = CreateWindow(GTK_APPLICATION(app));
    gtk_window_present(GTK_WINDOW(mw->Widget()));
    if (n_files > 0 && files[0]) {
        char* path = g_file_get_path(files[0]);
        if (path) {
            mw->LoadFile(path);
            g_free(path);
        }
    }
}

} // namespace

int App::Run(int argc, char** argv) {
    GtkApplication* app = gtk_application_new(
        "com.unitybase.Logram",
        static_cast<GApplicationFlags>(G_APPLICATION_DEFAULT_FLAGS |
                                       G_APPLICATION_HANDLES_OPEN));
    g_signal_connect(app, "activate", G_CALLBACK(OnActivate), nullptr);
    g_signal_connect(app, "open",     G_CALLBACK(OnOpen),     nullptr);
    const int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}