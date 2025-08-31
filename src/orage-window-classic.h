/*
 * Copyright (c) 2021-2025 Erkki Moorits
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

#ifndef ORAGE_WINDOW_CLASSIC_H
#define ORAGE_WINDOW_CLASSIC_H 1

#include "orage-window.h"
#include "orage-application.h"
#include <glib.h>

G_BEGIN_DECLS

#define ORAGE_WINDOW_CLASSIC_TYPE (orage_window_classic_get_type ())

G_DECLARE_FINAL_TYPE (OrageWindowClassic, orage_window_classic, ORAGE, WINDOW_CLASSIC, GtkApplicationWindow)

/** Creates a new OrageWindow
 *  @return a newly created OrageWindow
 */
GtkWidget *orage_window_classic_new (OrageApplication *application);

void orage_window_classic_mark_appointments (OrageWindow *window);

/** This routine is called from ical-code xfical_alarm_build_list_internal and
 *  ical files are already open at that time. So make sure ical files are opened
 *  before and closed after this call.
 */
void orage_window_classic_build_info (OrageWindow *window);
void orage_window_classic_build_events (OrageWindow *window);
void orage_window_classic_build_todo (OrageWindow *window);
void orage_window_classic_initial_load (OrageWindow *window);
void orage_window_classic_select_today (OrageWindow *window);
void orage_window_classic_select_date (OrageWindow *window, GDateTime *gdt);
GDateTime *orage_window_classic_get_selected_date (OrageWindow *window);
void orage_window_classic_show_menubar (OrageWindow *window, gboolean show);
void orage_window_classic_hide_todo (OrageWindow *window);
void orage_window_classic_hide_event (OrageWindow *window);
void orage_window_classic_raise (OrageWindow *window);
void orage_window_classic_set_calendar_options (OrageWindow *window, guint options);
void orage_window_classic_get_size_and_position (OrageWindow *window,
                                                 gint *size_x, gint *size_y,
                                                 gint *pos_x, gint *pos_y);

G_END_DECLS

#endif
