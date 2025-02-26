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

#include "orage-sync-edit-dialog.h"
#include "orage-application.h"
#include "orage-i18n.h"
#include "functions.h"
#include <gtk/gtk.h>

#define SPACING 4

struct _OrageSyncEditDialog
{
    GtkDialog parent;

    GtkEntry *description_entry;
    GtkEntry *command_entry;
    GtkSpinButton *period_entry;

    gboolean changed;
};

G_DEFINE_TYPE (OrageSyncEditDialog, orage_sync_edit_dialog, GTK_TYPE_DIALOG)

static GtkWidget *create_label (const gchar *str)
{
    GtkWidget *label;

    label = gtk_label_new (str);
    g_object_set (label, "vexpand", FALSE,
                         "valign", GTK_ALIGN_FILL,
                         "hexpand", FALSE,
                         "halign", GTK_ALIGN_END,
                         NULL);
    return label;
}

static void on_entry_changed_cb (G_GNUC_UNUSED GtkEditable *self,
                                 gpointer user_data)
{
    OrageSyncEditDialog *dialog = ORAGE_SYNC_EDIT_DIALOG (user_data);

    dialog->changed = TRUE;
}

static void on_spinner_changed_cb (G_GNUC_UNUSED GtkSpinButton *self,
                                   gpointer user_data)
{
    OrageSyncEditDialog *dialog = ORAGE_SYNC_EDIT_DIALOG (user_data);

    dialog->changed = TRUE;
}

static GtkEntry *create_entry (OrageSyncEditDialog *self)
{
    GtkWidget *entry;

    entry = gtk_entry_new ();
    g_object_set (entry, "vexpand", FALSE,
                         "hexpand", TRUE,
                         NULL);

    g_signal_connect (entry, "changed", G_CALLBACK (on_entry_changed_cb), self);

    return GTK_ENTRY (entry);
}

static GtkSpinButton *create_spinner (OrageSyncEditDialog *self)
{
    GtkWidget *spinner;

    spinner = gtk_spin_button_new_with_range (1, 60, 1);
    g_object_set (spinner, "vexpand", FALSE,
                           "hexpand", FALSE,
                           NULL);

    g_signal_connect (spinner, "value-changed",
                      G_CALLBACK (on_spinner_changed_cb), self);

    return GTK_SPIN_BUTTON (spinner);
}

static void orage_sync_edit_dialog_finalize (GObject *object)
{
    g_return_if_fail (ORAGE_IS_SYNC_EDIT_DIALOG (object));

    G_OBJECT_CLASS (orage_sync_edit_dialog_parent_class)->finalize (object);
}

static void orage_sync_edit_dialog_constructed (GObject *object)
{
    OrageSyncEditDialog *dialog = ORAGE_SYNC_EDIT_DIALOG (object);
    GtkWidget *content_area;
    GtkGrid *grid;
    GtkWidget *description_label;
    GtkWidget *command_label;
    GtkWidget *period_label;
    GtkBox *minute_box;
    GtkWidget *minute_label;
    GtkWidget *close_button;

    G_OBJECT_CLASS (orage_sync_edit_dialog_parent_class)->constructed (object);

    grid = GTK_GRID (gtk_grid_new ());
    minute_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, SPACING));
    gtk_grid_set_column_spacing (grid, SPACING);
    gtk_grid_set_row_spacing (grid, SPACING);

    g_object_set (minute_box, "vexpand", FALSE,
                              "valign", GTK_ALIGN_FILL,
                              "hexpand", FALSE,
                              "halign", GTK_ALIGN_START,
                              NULL);

    description_label = create_label (_("Description:"));
    dialog->description_entry = create_entry (dialog);
    command_label = create_label (_("Command:"));
    dialog->command_entry = create_entry (dialog);
    period_label = create_label (_("Period:"));
    dialog->period_entry = create_spinner (dialog);
    minute_label = create_label (_("minutes"));

    gtk_widget_set_tooltip_text (GTK_WIDGET (dialog->description_entry),
        _("The name or description of this synchronization task"));

    gtk_widget_set_tooltip_text (GTK_WIDGET (dialog->command_entry),
        _("Command for synchronization"));

    gtk_widget_set_tooltip_text (GTK_WIDGET (dialog->period_entry),
        _("Synchronization period in minutes"));

    gtk_box_pack_start (minute_box, GTK_WIDGET (dialog->period_entry),
                        FALSE, FALSE, 0);
    gtk_box_pack_end (minute_box, minute_label,
                      TRUE, TRUE, SPACING);

    gtk_grid_attach (grid, description_label,                      0, 0, 1, 1);
    gtk_grid_attach (grid, GTK_WIDGET (dialog->description_entry), 1, 0, 1, 1);
    gtk_grid_attach (grid, command_label,                          0, 1, 1, 1);
    gtk_grid_attach (grid, GTK_WIDGET (dialog->command_entry),     1, 1, 1, 1);
    gtk_grid_attach (grid, period_label,                           0, 2, 1, 1);
    gtk_grid_attach (grid, GTK_WIDGET (minute_box),                1, 2, 1, 1);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_box_pack_start (GTK_BOX (content_area), GTK_WIDGET (grid),
                        TRUE, TRUE, SPACING);
    gtk_widget_show_all (content_area);

    close_button = orage_util_image_button ("window-close", _("_Close"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), close_button,
                                  GTK_RESPONSE_OK);
    gtk_widget_set_can_default (close_button, TRUE);
    gtk_widget_show (close_button);
}

static void orage_sync_edit_dialog_class_init (OrageSyncEditDialogClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = orage_sync_edit_dialog_finalize;
    gobject_class->constructed = orage_sync_edit_dialog_constructed;
}

static void orage_sync_edit_dialog_init (OrageSyncEditDialog *self)
{
    self->changed = FALSE;
}

GtkWidget *orage_sync_edit_dialog_new (void)
{
    return g_object_new (ORAGE_SYNC_EDIT_DIALOG_TYPE, NULL);
}

GtkWidget *orage_sync_edit_dialog_new_with_defaults (const gchar *description,
                                                     const gchar *command,
                                                     guint period)
{
    OrageSyncEditDialog *dialog;

    dialog = ORAGE_SYNC_EDIT_DIALOG (g_object_new (ORAGE_SYNC_EDIT_DIALOG_TYPE,
                                                   NULL));

    if (description)
        gtk_entry_set_text (dialog->description_entry, description);

    if (command)
        gtk_entry_set_text (dialog->command_entry, command);

    gtk_spin_button_set_value (dialog->period_entry, period);

    dialog->changed = FALSE;

    return GTK_WIDGET (dialog);
}

const gchar *orage_sync_edit_dialog_get_description (OrageSyncEditDialog *dialog)
{
    g_return_val_if_fail (ORAGE_IS_SYNC_EDIT_DIALOG (dialog), NULL);

    return gtk_entry_get_text (dialog->description_entry);
}

const gchar *orage_sync_edit_dialog_get_command (OrageSyncEditDialog *dialog)
{
    g_return_val_if_fail (ORAGE_IS_SYNC_EDIT_DIALOG (dialog), NULL);

    return gtk_entry_get_text (dialog->command_entry);
}

guint orage_sync_edit_dialog_get_period (OrageSyncEditDialog *dialog)
{
    g_return_val_if_fail (ORAGE_IS_SYNC_EDIT_DIALOG (dialog), 10);

    return gtk_spin_button_get_value_as_int (dialog->period_entry);
}

gboolean orage_sync_edit_dialog_is_changed (OrageSyncEditDialog *dialog)
{
    g_return_val_if_fail (ORAGE_IS_SYNC_EDIT_DIALOG (dialog), FALSE);

    return dialog->changed;
}
