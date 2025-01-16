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

#include "orage-application.h"
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib.h>

#if 1
struct _OrageImportWindow
{
    GtkWindow __parent__;
};
#endif

G_DEFINE_TYPE (OrageImportWindow, orage_import_window, GTK_TYPE_WINDOW)

static void orage_import_window_class_init (OrageImportWindowClass *klass)
{
#if 0
    GObjectClass *object_class;

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
#else
    (void)klass;
#endif
}

static void orage_import_window_init (OrageImportWindow *self)
{
#if 0
    self->scroll_pos = -1; /* not set */
    self->accel_group = gtk_accel_group_new ();
    self->a_day = g_date_time_new_now_local ();
    self->Vbox = gtk_grid_new ();

    gtk_widget_set_name (GTK_WIDGET (self), "OrageWeekWindow");
    gtk_window_set_title (GTK_WINDOW (self), _("Orage - day view"));
    gtk_window_add_accel_group (GTK_WINDOW (self), self->accel_group);
    gtk_container_add (GTK_CONTAINER (self), self->Vbox);
#else
    (void)self;
#endif
}

OrageImportWindow *orage_import_window_new (OrageApplication *app)
{
    OrageImportWindow *window;

    window = g_object_new (ORAGE_IMPORT_WINDOW_TYPE,
                           "type", GTK_WINDOW_TOPLEVEL,
                           NULL);

    (void)app;

    return window;
}

gboolean orage_import_open_file (OrageImportWindow *window, GFile *file)
{
    (void)window;
    (void)file;

    return FALSE;
}
