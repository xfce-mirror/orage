/*
 * Copyright (c) 2022-2025 Erkki Moorits
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

#ifndef ORAGE_APPLICATION_H
#define ORAGE_APPLICATION_H 1

#include "orage-task-runner.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ORAGE_APPLICATION_TYPE (orage_application_get_type ())
G_DECLARE_FINAL_TYPE (OrageApplication, orage_application, ORAGE, APPLICATION, GtkApplication)

OrageApplication *orage_application_new (void);
OrageTaskRunner *orage_application_get_sync (OrageApplication *self);
GtkWidget *orage_application_get_window (OrageApplication *self);
void orage_application_close (OrageApplication *self);

/**
 * orage_application_open_path:
 * @self: (type OrageApplication): an Orage application instance.
 * @filename: (type filename): path to the file to open.
 *
 * Opens a file specified by a local filesystem path and shows the appointment
 * preview.
 *
 * Returns: %TRUE if the file was successfully opened, or %FALSE otherwise.
 */
gboolean orage_application_open_path (OrageApplication *self,
                                      const gchar *filename);

/**
 * orage_application_import_file:
 * @self: an #OrageApplication instance.
 * @file: a #GFile representing the file to import.
 *
 * Imports the given calendar or appointment file into Orage.
 *
 * Returns: %TRUE if the import succeeded, otherwise %FALSE.
 */
gboolean orage_application_import_path (OrageApplication *self,
                                        const gchar *filename);

/**
 * orage_application_export_path:
 * @self: an #OrageApplication instance.
 * @filename: a path to the file where calendar or appointment data will be
 *            exported.
 * @uids: (nullable): a comma-separated list of appointment UIDs to export, or
 *        %NULL to export all.
 *
 * Exports calendar or appointment data from Orage into the specified file.
 *
 * If @uids is %NULL, all appointments are exported. Otherwise, only the
 * appointments whose unique identifiers (UIDs) are listed in @uids are
 * exported.
 *
 * Returns: %TRUE if the export succeeded, otherwise %FALSE.
 */
gboolean orage_application_export_path (OrageApplication *self,
                                        const gchar *filename,
                                        const gchar *uids);
#if 0
gboolean orage_application_export_file (OrageApplication *self,
                                        const gchar *filename);
#endif
gboolean orage_application_add_foreign_file (OrageApplication *self,
                                             const gchar *filename);
gboolean orage_application_remove_foreign_file (OrageApplication *self,
                                                const gchar *filename);

G_END_DECLS

#endif
