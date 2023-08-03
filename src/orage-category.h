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

#ifndef ORAGE_CATEGORY_H
#define ORAGE_CATEGORY_H 1

#include "orage-rc-file.h"
#include <glib.h>
#include <gdk/gdk.h>

typedef struct _orage_category
{
    gchar *category;
    GdkRGBA color;
} orage_category_struct;

G_BEGIN_DECLS

OrageRc *orage_category_file_open (const gboolean read_only);
GdkRGBA *orage_category_list_contains (const gchar *categories);

/** Load category list to static variable and return pointer of this variable.
 *  @return pointer to static category list
 */
GList *orage_category_get_list (void);
void orage_category_write_entry (const gchar *category, const GdkRGBA *color);
void orage_category_remove_entry (const gchar *category);

G_END_DECLS

#endif
