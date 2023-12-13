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

#ifndef ORAGE_APPINDICATOR_H
#define ORAGE_APPINDICATOR_H 1

#include <gtk/gtk.h>
#include <glib.h>

G_BEGIN_DECLS

void *orage_appindicator_create (void);
void orage_appindicator_refresh (void);
void orage_appindicator_set_visible (void *tray_icon, gboolean show_systray);
void orage_appindicator_set_title (void *appindicator, const gchar *title);
void orage_appindicator_set_description (void *appindicator, const gchar *desc);

G_END_DECLS

#endif
