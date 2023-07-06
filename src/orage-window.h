/*
 * Copyright (c) 2021-2023 Erkki Moorits
 * Copyright (c) 2005-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2004-2006 Mickael Graf (korbinus at xfce.org)
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

#ifndef ORAGE_WINDOW_H
#define ORAGE_WINDOW_H 1

#include "orage-application.h"
#include <glib.h>

G_BEGIN_DECLS

#define ORAGE_TYPE_WINDOW (orage_window_get_type ())

G_DECLARE_FINAL_TYPE (OrageWindow, orage_window, ORAGE, WINDOW, GtkApplicationWindow)

void orage_mark_appointments (void);

/** This routine is called from ical-code xfical_alarm_build_list_internal and
 *  ical files are already open at that time. So make sure ical files are opened
 *  before and closed after this call.
 */
void orage_window_build_info (OrageWindow *window);
void orage_window_build_events (OrageWindow *window);
void orage_window_build_todo (OrageWindow *window);
void orage_window_month_changed (OrageWindow *window);

/** Creates a new OrageWindow
 *  @return a newly created OrageWindow
 */
GtkWidget *orage_window_new (OrageApplication *application);

void orage_window_show_menubar (OrageWindow *window, gboolean show);
void orage_window_hide_todo (OrageWindow *window);
void orage_window_hide_event (OrageWindow *window);
GtkCalendar *orage_window_get_calendar (OrageWindow *window);

G_END_DECLS

#endif
