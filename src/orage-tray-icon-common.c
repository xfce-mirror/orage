/*
 * Copyright (c) 2023 Erkki Moorits
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 *     Free Software Foundation
 *     51 Franklin Street, 5th Floor
 *     Boston, MA 02110-1301 USA
 */

#include <config.h>

#include "orage-tray-icon-common.h"

#include "parameters.h"
#include "orage-i18n.h"
#include "orage-window.h"
#include "orage-i18n.h"
#include "orage-appointment-window.h"
#include "functions.h"
#include "event-list.h"
#include <gtk/gtk.h>
#include <glib.h>

#ifdef HAVE_LIBXFCE4UI
#include <libxfce4ui/libxfce4ui.h>
#endif

static GtkWidget *orage_image_menu_item (const gchar *label,
                                         const gchar *icon_name)
{
#ifdef HAVE_LIBXFCE4UI
    return xfce_gtk_image_menu_item_new_from_icon_name (
            label, NULL, NULL, NULL, NULL, icon_name, NULL);
#else
    (void)icon_name;

    return gtk_menu_item_new_with_mnemonic (label);
#endif
}

static void on_today_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                               gpointer user_data)
{
    GDateTime *gdt;

    gdt = g_date_time_new_now_local ();
    orage_select_date (orage_window_get_calendar (ORAGE_WINDOW (user_data)),
                       gdt);
    g_date_time_unref (gdt);
    (void)create_el_win (NULL);
}

static void on_new_appointment_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                         G_GNUC_UNUSED gpointer user_data)
{
    GDateTime *gdt;
    GtkWidget *appointment_window;

    gdt = g_date_time_new_now_local ();
    appointment_window = orage_appointment_window_new (gdt);
    gtk_window_present (GTK_WINDOW (appointment_window));
    g_date_time_unref (gdt);
}

static void on_preferences_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                     G_GNUC_UNUSED gpointer user_data)
{
    show_parameters ();
}

static void on_orage_quit_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                    gpointer orage_app)
{
    g_application_quit (G_APPLICATION (orage_app));
}

GtkWidget *orage_tray_icon_create_menu (void)
{
    OrageApplication *app;
    GtkWidget *menu;
    GtkWidget *menu_item;

    app = ORAGE_APPLICATION (g_application_get_default ());
    menu = gtk_menu_new ();

    menu_item = orage_image_menu_item (_("Today"), "go-home");
    g_signal_connect (menu_item, "activate",
                      G_CALLBACK (on_today_activate),
                      orage_application_get_window (app));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    gtk_widget_show (menu_item);

    menu_item = orage_image_menu_item (_("New appointment"), "document-new");
    g_signal_connect (menu_item, "activate",
                      G_CALLBACK (on_new_appointment_activate), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    gtk_widget_show (menu_item);

    menu_item = orage_image_menu_item (_("Preferences"), "preferences-system");
    g_signal_connect (menu_item, "activate",
                      G_CALLBACK (on_preferences_activate), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    gtk_widget_show (menu_item);

    menu_item = orage_image_menu_item (_("Quit"), "application-exit");
    g_signal_connect (menu_item, "activate",
                      G_CALLBACK (on_orage_quit_activate), app);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    gtk_widget_show (menu_item);

    return menu;
}
