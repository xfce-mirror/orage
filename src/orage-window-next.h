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

#ifndef ORAGE_WINDOW_NEXT_H
#define ORAGE_WINDOW_NEXT_H 1

#include "orage-window.h"
#include "orage-application.h"
#include <glib.h>

G_BEGIN_DECLS

#define ORAGE_WINDOW_NEXT_TYPE (orage_window_next_get_type ())

G_DECLARE_FINAL_TYPE (OrageWindowNext, orage_window_next, ORAGE, WINDOW_NEXT, GtkApplicationWindow)

/** Creates a new next generation OrageWindow
 *  @return a newly created OrageWindow
 */
GtkWidget *orage_window_next_new (OrageApplication *application);

void orage_window_next_mark_appointments (OrageWindow *window);

/** This routine is called from ical-code xfical_alarm_build_list_internal and
 *  ical files are already open at that time. So make sure ical files are opened
 *  before and closed after this call.
 */
void orage_window_next_build_info (OrageWindow *window);
void orage_window_next_build_events (OrageWindow *window);
void orage_window_next_build_todo (OrageWindow *window);
void orage_window_next_initial_load (OrageWindow *window);
void orage_window_next_select_today (OrageWindow *window);
void orage_window_next_select_date (OrageWindow *window, GDateTime *gdt);
void orage_window_next_show_menubar (OrageWindow *window, gboolean show);
void orage_window_next_hide_todo (OrageWindow *window);
void orage_window_next_hide_event (OrageWindow *window);
GtkCalendar *orage_window_next_get_calendar (OrageWindow *window);
void orage_window_next_raise (OrageWindow *window);
void orage_window_next_set_calendar_options (OrageWindow *window, guint options);

G_END_DECLS

#endif
