/*
 * Copyright (c) 2021-2024 Erkki Moorits
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

#ifndef ORAGE_WEEK_WINDOW_H
#define ORAGE_WEEK_WINDOW_H 1

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ORAGE_WEEK_WINDOW_TYPE (orage_week_window_get_type ())
G_DECLARE_FINAL_TYPE (OrageWeekWindow, orage_week_window, ORAGE, WEEK_WINDOW,
                      GtkWindow)

/** Create new week window for specified start date.
 *  @param date window start date
 */
OrageWeekWindow *orage_week_window_new (GDateTime *date);

/** Build new week window. This function create new week window and connect all
 *  necceary signals to close window.
 *  @param date window start date
 */
void orage_week_window_build (GDateTime *date);

void orage_week_window_remove_appointment_window (OrageWeekWindow *self,
                                                  gconstpointer apptw);

void orage_week_window_refresh (OrageWeekWindow *window);

G_END_DECLS

#endif
