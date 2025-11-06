/*
 * Copyright (c) 2021-2023 Erkki Moorits
 * Copyright (c) 2005-2011 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2004-2005 Mickael Graf (korbinus at xfce.org)
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

#ifndef ICAL_EXPIMP_H
#define ICAL_EXPIMP_H 1

#include <glib.h>

gboolean xfical_import_file (GFile *file);
gboolean xfical_import_by_path (const gchar *file_name);
gboolean xfical_export_by_path (const gchar *file_name,
                                gint type,
                                const gchar *uids);

#endif
