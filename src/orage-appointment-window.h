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

#ifndef ORAGE_APPOINTMENT_WINDOW_H
#define ORAGE_APPOINTMENT_WINDOW_H 1

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#define ORAGE_TYPE_APPOINTMENT_WINDOW (orage_appointment_window_get_type ())

G_DECLARE_FINAL_TYPE (OrageAppointmentWindow, orage_appointment_window, ORAGE, APPOINTMENT_WINDOW, GtkWindow)

GtkWidget *orage_appointment_window_new (GDateTime *gdt);
GtkWidget *orage_appointment_window_new_copy (const gchar *uid);
GtkWidget *orage_appointment_window_new_update (const gchar *uid);

void orage_appointment_window_set_event_list (OrageAppointmentWindow *apptw,
                                              void *el);

void orage_appointment_window_set_day_window (OrageAppointmentWindow *apptw,
                                              void *dw);

GdkRGBA *orage_category_list_contains(char *categories);
void orage_category_get_list (void);

#endif