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

#ifndef ORAGE_SYNC_EDIT_DIALOG_H
#define ORAGE_SYNC_EDIT_DIALOG_H 1

#include <gtk/gtk.h>
#include <glib.h>

#define ORAGE_SYNC_EDIT_DIALOG_TYPE (orage_sync_edit_dialog_get_type ())
G_DECLARE_FINAL_TYPE (OrageSyncEditDialog, orage_sync_edit_dialog, ORAGE, SYNC_EDIT_DIALOG, GtkDialog)

GtkWidget *orage_sync_edit_dialog_new (void);

GtkWidget *orage_sync_edit_dialog_new_with_defaults (const gchar *description,
                                                     const gchar *command,
                                                     guint period);

/** Gets the current text of the syncronization description.
 *  @param dialog OrageSyncEditDialog instance
 *  @return text, owned by dialog
 */
const gchar *orage_sync_edit_dialog_get_description (OrageSyncEditDialog *dialog);

/** Gets the current text of the command.
 *  @param dialog OrageSyncEditDialog instance
 *  @return text, owned by dialog
 */
const gchar *orage_sync_edit_dialog_get_command (OrageSyncEditDialog *dialog);

/** Gets the current period value.
 *  @param dialog OrageSyncEditDialog instance
 *  @return integer between 1 to 60
 */
guint orage_sync_edit_dialog_get_period (OrageSyncEditDialog *dialog);

/** Gets the dialog changed status
 *  @param dialog OrageSyncEditDialog instance
 *  @return TRUE when dialog has been changed, FLASE if not changed
 */
gboolean orage_sync_edit_dialog_is_changed (OrageSyncEditDialog *dialog);

#endif
