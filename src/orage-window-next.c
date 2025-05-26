/*
 * Copyright (c) 2025 Erkki Moorits
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

#include "orage-window-next.h"

#include "orage-month-view.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>

struct _OrageWindowNext
{
    GtkApplicationWindow __parent__;
    
    GtkWidget *main_box;
    GtkWidget *menubar;
    GtkWidget *file_menu;
    GtkWidget *file_menu_label;
    GtkWidget *file_menu_new;
    GtkWidget *file_menu_close;
    GtkWidget *file_menu_quit;
    GtkWidget *main_view;
    GtkWidget *stack_views;
    GtkWidget *month_view;
    GtkWidget *year_view;
    GDateTime *date;
};

typedef enum
{
    OCAL_COS
} OcalWindowAction;

static void orage_window_next_interface_init (OrageWindowInterface *interface);

G_DEFINE_TYPE_WITH_CODE (OrageWindowNext, orage_window_next, GTK_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (ORAGE_WINDOW_TYPE, orage_window_next_interface_init))
#if 0
void orage_window_next_open (G_GNUC_UNUSED OrageWindow *self,
                             G_GNUC_UNUSED GFile *file)
{
}
#endif

static void orage_window_next_init (OrageWindowNext *self)
{
    self->date = g_date_time_new_now_utc ();
    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add (GTK_CONTAINER (self), self->main_box);

    /* build the menubar */
    self->menubar = gtk_menu_bar_new ();
    self->file_menu = gtk_menu_new ();
    
    self->file_menu_label = gtk_menu_item_new_with_label ("File");

    self->file_menu_new = gtk_menu_item_new_with_label ("New");
    self->file_menu_close = gtk_menu_item_new_with_label ("Close");
    self->file_menu_quit = gtk_menu_item_new_with_label ("Quit");
    
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (self->file_menu_label),
    self->file_menu);
    gtk_menu_shell_append (GTK_MENU_SHELL (self->file_menu),
    self->file_menu_new);
    gtk_menu_shell_append (GTK_MENU_SHELL (self->file_menu),
    self->file_menu_close);
    gtk_menu_shell_append (GTK_MENU_SHELL (self->file_menu),
    self->file_menu_quit);
    gtk_menu_shell_append (GTK_MENU_SHELL (self->menubar),
    self->file_menu_label);
    
    gtk_box_pack_start (GTK_BOX (self->main_box), self->menubar, FALSE, FALSE, 0);
    self->stack_views = gtk_stack_new ();
    self->month_view = orage_month_view_new ();
    gtk_box_pack_start (GTK_BOX (self->main_box), self->month_view, FALSE, FALSE, 0);
    
    gtk_widget_show_all (self->main_box);
}

static void orage_window_next_class_init (G_GNUC_UNUSED OrageWindowNextClass *klass)
{
}

static void orage_window_next_interface_init (OrageWindowInterface *iface)
{
    iface->mark_appointments = orage_window_next_mark_appointments;
    iface->build_info = orage_window_next_build_info;
    iface->build_events = orage_window_next_build_events;
    iface->build_todo = orage_window_next_build_todo;
    iface->month_changed = orage_window_next_month_changed;
    iface->show_menubar = orage_window_next_show_menubar;
    iface->hide_todo = orage_window_next_hide_todo;
    iface->hide_event = orage_window_next_hide_event;
    iface->get_calendar = orage_window_next_get_calendar;
    iface->raise = orage_window_next_raise;
}

GtkWidget *orage_window_next_new (OrageApplication *application)
{
    return g_object_new (ORAGE_WINDOW_NEXT_TYPE,
                         "application", GTK_APPLICATION (application),
                         NULL);
}

void orage_window_next_build_events (OrageWindow *window)
{
#if 0
    if (!xfical_file_open(TRUE))
        return;

    build_mainbox_event_info (window);
    xfical_file_close(TRUE);
#else
    (void)window;
#endif
}

void orage_window_next_build_info (OrageWindow *window)
{
#if 0
    build_mainbox_todo_info (window);
    build_mainbox_event_info (window);
#else
    (void)window;
#endif
}

void orage_window_next_build_todo (OrageWindow *window)
{
#if 0
    if (!xfical_file_open (TRUE))
        return;
    build_mainbox_todo_info (window);
    xfical_file_close (TRUE);
#else
    (void)window;
#endif
}

GtkCalendar *orage_window_next_get_calendar (OrageWindow *window)
{
#if 0
    return GTK_CALENDAR (ORAGE_WINDOW_CLASSIC (window)->mCalendar);
#else
    (void)window;
    return GTK_CALENDAR (gtk_calendar_new ());
#endif
}

void orage_window_next_hide_event (OrageWindow *window)
{
#if 0
    gtk_widget_hide (ORAGE_WINDOW_CLASSIC (window)->mEvent_vbox);
#else
    (void)window;
#endif
}

void orage_window_next_hide_todo (OrageWindow *window)
{
#if 0
    gtk_widget_hide (ORAGE_WINDOW_CLASSIC (window)->mTodo_vbox);
#else
    (void)window;
#endif
}

void orage_window_next_mark_appointments (OrageWindow *window)
{
#if 0
    if (window == NULL)
        return;

    if (!xfical_file_open (TRUE))
        return;

    xfical_mark_calendar (orage_window_classic_get_calendar (window));
    xfical_file_close (TRUE);
#else
    (void)window;
#endif
}

void orage_window_next_month_changed (OrageWindow *window)
{
#if 0
    mCalendar_month_changed_cb (orage_window_classic_get_calendar (window),
                                window);
#else
    (void)window;
#endif
}

void orage_window_next_raise (OrageWindow *window)
{
#if 0
    GtkWindow *gtk_window = GTK_WINDOW (window);

    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move (gtk_window, g_par.pos_x, g_par.pos_y);

    if (g_par.select_always_today)
        orage_select_today (orage_window_classic_get_calendar (window));

    if (g_par.set_stick)
        gtk_window_stick (gtk_window);

    gtk_window_set_keep_above (gtk_window, g_par.set_ontop);
    gtk_window_present (gtk_window);
#else
    (void)window;
#endif
}

void orage_window_next_show_menubar (OrageWindow *window, const gboolean show)
{
#if 0
    OrageWindowClassic *clwindow = ORAGE_WINDOW_CLASSIC (window);

    if (show)
        gtk_widget_show (clwindow->mMenubar);
    else
        gtk_widget_hide (clwindow->mMenubar);
#else
    (void)window;
    (void)show;
#endif
}
