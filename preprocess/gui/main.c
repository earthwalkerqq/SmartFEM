#include <gtk/gtk.h>

#include "defines.h"

void on_submit(GtkWidget *widget, gpointer data) {
    GtkEntry **entries = (GtkEntry **)data;
    const char *e_str = gtk_entry_get_text(entries[0]);
    const char *nu_str = gtk_entry_get_text(entries[1]);

    const double E = atof(e_str);
    const double nu = atof(nu_str);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Параметры материала");
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);

    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);

    GtkWidget *label_e = gtk_label_new("Модуль упругости E:");
    GtkWidget *entry_e = gtk_entry_new();
    GtkWidget *label_nu = gtk_label_new("Коэффициент Пуассона nu:");
    GtkWidget *entry_nu = gtk_entry_new();

    GtkWidget *button = gtk_button_new_with_label("Применить");

    gtk_grid_attach(GTK_GRID(grid), label_e, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_e, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_nu, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_nu, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), button, 0, 2, 2, 1);

    GtkEntry *entries[2] = {GTK_ENTRY(entry_e), GTK_ENTRY(entry_nu)};
    g_signal_connect(button, "clicked", G_CALLBACK(on_submit), entries);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);

    gtk_main();
    return 0;
}
