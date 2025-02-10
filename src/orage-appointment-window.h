/*
 * Copyright (c) 2021-2025q Erkki Moorits
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

#ifndef ORAGE_APPOINTMENT_WINDOW_H
#define ORAGE_APPOINTMENT_WINDOW_H 1

#include "orage-week-window.h"
#include "event-list.h"
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ORAGE_TYPE_APPOINTMENT_WINDOW (orage_appointment_window_get_type ())

G_DECLARE_FINAL_TYPE (OrageAppointmentWindow, orage_appointment_window, ORAGE,
                      APPOINTMENT_WINDOW, GtkWindow)

GtkWidget *orage_appointment_window_new (GDateTime *gdt);

/** Create new appointment window for current time.
 *  @return appointment window
 */
GtkWidget *orage_appointment_window_new_now (void);
GtkWidget *orage_appointment_window_new_copy (const gchar *uid);
GtkWidget *orage_appointment_window_new_update (const gchar *uid);

void orage_appointment_window_set_event_list (OrageAppointmentWindow *apptw,
                                              el_win *el);

void orage_appointment_window_set_day_window (OrageAppointmentWindow *apptw,
                                              gpointer dw);

G_END_DECLS

#endif
