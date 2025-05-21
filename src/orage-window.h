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

/** Common interface for old and new window. */

#ifndef ORAGE_WINDOW_H
#define ORAGE_WINDOW_H 1

#include "orage-application.h"

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ORAGE_WINDOW_TYPE (orage_window_get_type())
G_DECLARE_INTERFACE (OrageWindow, orage_window, ORAGE, WINDOW, GtkApplicationWindow)

struct _OrageWindowInterface
{
    GTypeInterface parent_iface;

    void (*mark_appointments) (OrageWindow *window);
    void (*build_info) (OrageWindow *window);
    void (*build_events) (OrageWindow *window);
    void (*build_todo) (OrageWindow *window);
    void (*month_changed) (OrageWindow *window);
    void (*show_menubar) (OrageWindow *window, gboolean show);
    void (*hide_todo) (OrageWindow *window);
    void (*hide_event) (OrageWindow *window);
    GtkCalendar *(*get_calendar) (OrageWindow *window);
    void (*raise)(OrageWindow *window);
};

GtkWidget *orage_window_create (OrageApplication *app, gboolean use_new_ui);

void orage_window_mark_appointments (OrageWindow *window);

/** This routine is called from ical-code xfical_alarm_build_list_internal and
 *  ical files are already open at that time. So make sure ical files are opened
 *  before and closed after this call.
 */
void orage_window_build_info (OrageWindow *window);
void orage_window_build_events (OrageWindow *window);
void orage_window_build_todo (OrageWindow *window);
void orage_window_month_changed (OrageWindow *window);

void orage_window_show_menubar (OrageWindow *window, gboolean show);
void orage_window_hide_todo (OrageWindow *window);
void orage_window_hide_event (OrageWindow *window);
GtkCalendar *orage_window_get_calendar (OrageWindow *window);
void orage_window_raise (OrageWindow *window);

G_END_DECLS

#endif

