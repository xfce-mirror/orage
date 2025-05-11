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
#include "functions.h"
#include "orage-appointment-window.h"
#include <libxfce4ui/libxfce4ui.h>
#include <gtk/gtk.h>
#include <glib-object.h>
#include <glib.h>

#define IMPORT_WINDOW_EVENTS "import-window-events"

static void orage_import_window_set_property (GObject *object,
                                              guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec);
static void orage_import_window_get_property (GObject *object,
                                              guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec);
static void orage_import_window_constructed (GObject *object);
static void orage_import_window_finalize (GObject *object);

struct _OrageImportWindow
{
    XfceTitledDialog __parent__;

    GList *events;
    GtkWidget *notebook;
};

enum
{
    PROP_EVENT_LIST = 1
};

static const gchar *event_type_to_string (const xfical_type type)
{
    switch (type)
    {
        case XFICAL_TYPE_EVENT:
            return _("Event");

        case XFICAL_TYPE_TODO:
            return _("Todo");

        case XFICAL_TYPE_JOURNAL:
            return _("Journal");

        default:
            g_critical ("%s: Unsupported Type", G_STRFUNC);
            return "Unsupported event type";
    }
}

static GtkWidget *create_time_widget (GDateTime *gdt)
{
    GtkWidget *data_label;
    gchar *label_text;

    label_text = orage_gdatetime_to_i18_time_with_zone (gdt);
    data_label = gtk_label_new (label_text);
    g_free (label_text);

    return data_label;
}

G_DEFINE_TYPE (OrageImportWindow, orage_import_window, XFCE_TYPE_TITLED_DIALOG)
static GtkWidget *orage_import_window_create_event_preview_from_cal_comp (
    OrageCalendarComponent *cal_comp)
{
    const gchar *text;
    gint row = 0;
    GtkWidget *name_label;
    GtkWidget *data_label;
    GtkGrid *grid = GTK_GRID (gtk_grid_new ());

    gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

    /* Task summary */
    text = o_cal_component_get_summary (cal_comp);
    name_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (name_label), _("<b>Summary</b>"));
    gtk_widget_set_halign (name_label, GTK_ALIGN_END);
    data_label = gtk_label_new (text ? text : NULL);
    gtk_widget_set_halign (data_label, GTK_ALIGN_START);
    gtk_grid_attach (grid, name_label, 0, row, 1, 1);
    gtk_grid_attach (grid, data_label, 1, row++, 1, 1);
    gtk_widget_show (name_label);
    gtk_widget_show (data_label);

    /* Type */
    name_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (name_label), _("<b>Type</b>"));
    gtk_widget_set_halign (name_label, GTK_ALIGN_END);
    data_label = gtk_label_new (
            event_type_to_string (o_cal_component_get_type (cal_comp)));
    gtk_widget_set_halign (data_label, GTK_ALIGN_START);
    gtk_grid_attach (grid, name_label, 0, row, 1, 1);
    gtk_grid_attach (grid, data_label, 1, row++, 1, 1);
    gtk_widget_show (name_label);
    gtk_widget_show (data_label);

    /* Location */
    text = o_cal_component_get_location (cal_comp);
    if (text)
    {
        name_label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (name_label), _("<b>Location</b>"));
        gtk_widget_set_halign (name_label, GTK_ALIGN_END);
        data_label = gtk_label_new (text);
        gtk_widget_set_halign (data_label, GTK_ALIGN_START);
        gtk_grid_attach (grid, name_label, 0, row, 1, 1);
        gtk_grid_attach (grid, data_label, 1, row++, 1, 1);
        gtk_widget_show (name_label);
        gtk_widget_show (data_label);
    }

    /* URL */
    text = o_cal_component_get_url (cal_comp);
    if (text)
    {
        name_label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (name_label), _("<b>URL</b>"));
        gtk_widget_set_halign (name_label, GTK_ALIGN_END);
        data_label = gtk_entry_new ();
        gtk_entry_set_text (GTK_ENTRY (data_label), text);
        gtk_editable_set_editable (GTK_EDITABLE (data_label), FALSE);
        gtk_entry_set_has_frame (GTK_ENTRY (data_label), FALSE);
        gtk_widget_set_can_focus (data_label, FALSE);

        gtk_widget_set_halign (data_label, GTK_ALIGN_START);
        gtk_grid_attach (grid, name_label, 0, row, 1, 1);
        gtk_grid_attach (grid, data_label, 1, row++, 1, 1);
        gtk_widget_show (name_label);
        gtk_widget_show (data_label);
    }

    /* Duration */
    if (o_cal_component_is_all_day_event (cal_comp))
    {
        name_label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (name_label), _("<b>All-day</b>"));
        gtk_widget_set_halign (name_label, GTK_ALIGN_END);
        data_label = gtk_check_button_new ();
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data_label), TRUE);
        gtk_widget_set_sensitive (data_label, FALSE);
        gtk_widget_set_halign (data_label, GTK_ALIGN_START);
        gtk_grid_attach (grid, name_label, 0, row, 1, 1);
        gtk_grid_attach (grid, data_label, 1, row++, 1, 1);
        gtk_widget_show (name_label);
        gtk_widget_show (data_label);
    }
    else
    {
        GDateTime *gdt;
        name_label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (name_label), _("<b>From</b>"));
        gtk_widget_set_halign (name_label, GTK_ALIGN_END);
        gdt = o_cal_component_get_dtstart (cal_comp);
        data_label = create_time_widget (gdt);
        g_date_time_unref (gdt);
        gtk_widget_set_halign (data_label, GTK_ALIGN_START);
        gtk_grid_attach (grid, name_label, 0, row, 1, 1);
        gtk_grid_attach (grid, data_label, 1, row++, 1, 1);
        gtk_widget_show (name_label);
        gtk_widget_show (data_label);

        gdt = o_cal_component_get_dtend (cal_comp);
        if (gdt)
        {
            name_label = gtk_label_new (NULL);
            gtk_label_set_markup (GTK_LABEL (name_label), _("<b>To</b>"));
            gtk_widget_set_halign (name_label, GTK_ALIGN_END);
            data_label = create_time_widget (gdt);
            g_date_time_unref (gdt);
            gtk_widget_set_halign (data_label, GTK_ALIGN_START);
            gtk_grid_attach (grid, name_label, 0, row, 1, 1);
            gtk_grid_attach (grid, data_label, 1, row++, 1, 1);
            gtk_widget_show (name_label);
            gtk_widget_show (data_label);
        }
    }

    /* Repeat */
    name_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (name_label), _("<b>Recurring</b>"));
    gtk_widget_set_halign (name_label, GTK_ALIGN_END);
    data_label = gtk_label_new (
            o_cal_component_is_recurring (cal_comp) ? _("Yes") : _("No"));
    gtk_widget_set_halign (data_label, GTK_ALIGN_START);
    gtk_grid_attach (grid, name_label, 0, row, 1, 1);
    gtk_grid_attach (grid, data_label, 1, row++, 1, 1);
    gtk_widget_show (name_label);
    gtk_widget_show (data_label);

    return GTK_WIDGET (grid);
}

static void orage_import_window_class_init (OrageImportWindowClass *klass)
{
    GParamSpec *param_specs;
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->constructed = orage_import_window_constructed;
    object_class->finalize = orage_import_window_finalize;
    object_class->get_property = orage_import_window_get_property;
    object_class->set_property = orage_import_window_set_property;

    param_specs = g_param_spec_pointer (IMPORT_WINDOW_EVENTS,
                                        IMPORT_WINDOW_EVENTS,
                                        "Calendar event list",
                                        G_PARAM_READWRITE |
                                        G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_property (object_class,
                                     PROP_EVENT_LIST,
                                     param_specs);
}

static void orage_import_window_init (G_GNUC_UNUSED OrageImportWindow *self)
{
}

static void orage_import_window_constructed (GObject *object)
{
    OrageImportWindow *self = (OrageImportWindow *)object;
    GtkWidget *button;
    GtkWidget *page;
    GtkWidget *label;
    GList *tmp_list;
    OrageCalendarComponent *cal_comp;
    guint nr_items;

    G_OBJECT_CLASS (orage_import_window_parent_class)->constructed (object);

    gtk_widget_set_name (GTK_WIDGET (self), "OrageImportWindow");
    gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
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

    nr_items = g_list_length (self->events);
    if (nr_items == 1)
    {
        tmp_list = g_list_first (self->events);
        cal_comp = ORAGE_CALENDAR_COMPONENT (tmp_list->data);
        page = orage_import_window_create_event_preview_from_cal_comp (cal_comp);

        gtk_box_pack_start (
            GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))), page,
            TRUE, TRUE, 0);

        gtk_widget_show (page);
    }
    else if (nr_items > 1)
    {
        self->notebook = gtk_notebook_new ();
        gtk_notebook_set_scrollable (GTK_NOTEBOOK (self->notebook), TRUE);
        gtk_container_set_border_width (GTK_CONTAINER (self->notebook), 6);
        gtk_box_pack_start (
                GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))),
                         self->notebook, TRUE, TRUE, 0);

        for (tmp_list = g_list_first (self->events);
             tmp_list != NULL;
             tmp_list = g_list_next (tmp_list))
        {
            cal_comp = ORAGE_CALENDAR_COMPONENT (tmp_list->data);
            page = orage_import_window_create_event_preview_from_cal_comp (cal_comp);
            label = gtk_label_new (o_cal_component_get_summary (cal_comp));

            gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook), page, label);
            gtk_widget_show (page);
        }

        gtk_widget_show (self->notebook);
    }
    else
        g_assert_not_reached ();
}

static void orage_import_window_finalize (GObject *object)
{
    G_OBJECT_CLASS (orage_import_window_parent_class)->finalize (object);
}

static void orage_import_window_get_property (GObject *object,
                                              const guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec)
{
    const OrageImportWindow *self = ORAGE_IMPORT_WINDOW (object);

    switch (prop_id)
    {
        case PROP_EVENT_LIST:
            g_assert (self->events == NULL);
            g_value_set_pointer (value, self->events);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void orage_import_window_set_property (GObject *object,
                                              const guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec)
{
    OrageImportWindow *self = ORAGE_IMPORT_WINDOW (object);

    switch (prop_id)
    {
        case PROP_EVENT_LIST:
            self->events = g_value_get_pointer (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

GtkWidget *orage_import_window_new (GList *events)
{
    OrageImportWindow *window;

    g_return_val_if_fail (events != NULL, NULL);

    window = g_object_new (ORAGE_IMPORT_WINDOW_TYPE,
                           "type", GTK_WINDOW_TOPLEVEL,
                           IMPORT_WINDOW_EVENTS, events,
                           NULL);

    return GTK_WIDGET (window);
}
