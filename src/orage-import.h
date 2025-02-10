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

#ifndef ORAGE_IMPORT_H
#define ORAGE_IMPORT_H 1

#include <libxfce4ui/libxfce4ui.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib.h>

G_BEGIN_DECLS

#define ORAGE_IMPORT_WINDOW_TYPE (orage_import_window_get_type ())
#ifndef glib_autoptr_clear_XfceTitledDialog
G_DEFINE_AUTOPTR_CLEANUP_FUNC (XfceTitledDialog, g_object_unref)
#endif
G_DECLARE_FINAL_TYPE (OrageImportWindow, orage_import_window, ORAGE, IMPORT_WINDOW, XfceTitledDialog)

OrageImportWindow *orage_import_window_new (GList *);

G_END_DECLS

#endif
