#pragma once

#include <gtk/gtk.h>

class MainWindow {
public:
    static GtkWidget* Create(GtkApplication* app);
};