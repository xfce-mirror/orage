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

#include "orage-import.h"

#include "orage-i18n.h"
#include "ical-code.h"
#include "orage-appointment-window.h"
#include <libxfce4ui/libxfce4ui.h>
#include <gtk/gtk.h>
#include <glib.h>

static void orage_import_window_response (GtkDialog *dialog, gint response_id);

struct _OrageImportWindow
{
    XfceTitledDialog __parent__;

    GList *events;
    GtkWidget *notebook;
};

G_DEFINE_TYPE (OrageImportWindow, orage_import_window, XFCE_TYPE_TITLED_DIALOG)

static void orage_import_window_finalize (GObject *object)
{
    OrageImportWindow *window = ORAGE_IMPORT_WINDOW (object);

    G_OBJECT_CLASS (orage_import_window_parent_class)->finalize (object);
}

static void orage_import_window_class_init (OrageImportWindowClass *klass)
{
    GObjectClass *object_class;
    GtkDialogClass *gtkdialog_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orage_import_window_finalize;

    gtkdialog_class = GTK_DIALOG_CLASS (klass);
    gtkdialog_class->response = orage_import_window_response;

#if 0
    object_class = G_OBJECT_CLASS (klass);
    object_class->constructed = orage_week_window_constructed;
    object_class->finalize = orage_week_window_finalize;
    object_class->get_property = orage_week_window_get_property;
    object_class->set_property = orage_week_window_set_property;

    properties[PROP_START_DATE] =
            g_param_spec_boxed (ORAGE_WEEK_WINDOW_START_DATE_PROPERTY,
                                ORAGE_WEEK_WINDOW_START_DATE_PROPERTY,
                                "An GDateTime for week window start date",
                                G_TYPE_DATE_TIME,
                                G_PARAM_STATIC_STRINGS |
                                G_PARAM_READWRITE |
                                G_PARAM_CONSTRUCT_ONLY |
                                G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, N_PROPS, properties);
#endif
}

static void orage_import_window_init (OrageImportWindow *self)
{
    GtkWidget *button;
    GtkWidget *page;
    GtkWidget *label;
    GList *tmp_list;
    OrageCalendarComponent *cal_comp;

    gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
    gtk_widget_set_name (GTK_WIDGET (self), "OrageImportWindow");
    gtk_window_set_title (GTK_WINDOW (self), _("Import calendar file"));

    button = gtk_button_new_with_mnemonic (_("_Import"));
    xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (self),
                                          button, GTK_RESPONSE_ACCEPT);
    xfce_titled_dialog_set_default_response (XFCE_TITLED_DIALOG (self),
                                             GTK_RESPONSE_ACCEPT);
    gtk_widget_set_can_default (button, TRUE);
    gtk_widget_grab_default (button);
    gtk_widget_show (button);

    button = gtk_button_new_with_mnemonic (_("_Cancel"));
    xfce_titled_dialog_add_action_widget (XFCE_TITLED_DIALOG (self),
                                          button, GTK_RESPONSE_CANCEL);
    gtk_widget_show (button);

    self->notebook = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (self->notebook), 6);
    gtk_box_pack_start (
            GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))),
            self->notebook, TRUE, TRUE, 0);

    g_debug ("read list: %p", self->events);

    for (tmp_list = g_list_first (self->events);
         tmp_list != NULL;
         tmp_list = g_list_next (tmp_list))
    {
        g_debug ("append");
        cal_comp = ORAGE_CALENDAR_COMPONENT (tmp_list->data);
        page = orage_event_preview_new_from_cal_comp (cal_comp);
        label = gtk_label_new (o_cal_component_get_event_name (cal_comp));

        gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook), page, label);
    }

    gtk_widget_show (self->notebook);
}

static void orage_import_window_response (GtkDialog *gtk_dialog,
                                          const gint response_id)
{
    OrageImportWindow *dialog = ORAGE_IMPORT_WINDOW (gtk_dialog);

    g_return_if_fail (ORAGE_IS_IMPORT_WINDOW (dialog));

    if (response_id == GTK_RESPONSE_ACCEPT)
    {
        g_debug ("GTK_RESPONSE_ACCEPT");
    }
    else if (response_id == GTK_RESPONSE_CANCEL)
    {
        g_debug ("GTK_RESPONSE_CANCEL");
        gtk_widget_destroy (GTK_WIDGET (gtk_dialog));
    }
}

GtkWidget *orage_import_window_new (GList *events)
{
    OrageImportWindow *window;

    g_return_val_if_fail (events != NULL, NULL);

    window = g_object_new (ORAGE_IMPORT_WINDOW_TYPE,
                           "type", GTK_WINDOW_TOPLEVEL,
                           NULL);

    window->events = events;

    return GTK_WIDGET (window);
}
