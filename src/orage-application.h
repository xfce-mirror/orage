/*
 * Copyright (c) 2022-2023 Erkki Moorits
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
OrageTaskRunner *orage_application_get_sync (OrageApplication *application);
GtkWidget *orage_application_get_window (OrageApplication *application);
void orage_application_close (OrageApplication *application);

gboolean orage_application_open_path (OrageApplication *application,
                                      const gchar *filename);
gboolean orage_application_import_file (OrageApplication *application,
                                        const gchar *filename);
gboolean orage_application_export_file (OrageApplication *application,
                                        const gchar *filename);
gboolean orage_application_add_foreign_file (OrageApplication *application,
                                             const gchar *filename);
gboolean orage_application_remove_foreign_file (OrageApplication *application,
                                                const gchar *filename);

G_END_DECLS

#endif
