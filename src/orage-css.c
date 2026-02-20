/*
 * Copyright (c) 2021-2026 Erkki Moorits
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

#include "orage-css.h"
#include <libxfce4util/libxfce4util.h>
#include <gtk/gtk.h>
#include <glib.h>

#define ORAGE_CSS_VERSION "orage-4.0"
#define ORAGE_CSS_NAME "gtk.css"
#define ORAGE_DEFAULT_THEME "Default/" ORAGE_CSS_VERSION "/" ORAGE_CSS_NAME

static void append_error_value (GString *string,
                                GType    enum_type,
                                guint    value)
{
    GEnumClass *enum_class;
    GEnumValue *enum_value;

    enum_class = g_type_class_ref (enum_type);
    enum_value = g_enum_get_value (enum_class, value);

    g_string_append (string, enum_value->value_name);

    g_type_class_unref (enum_class);
}

static void parsing_error_cb (G_GNUC_UNUSED GtkCssProvider *provider,
                              GtkCssSection *section,
                              const GError *error,
                              GString *errors)
{
    char *path;

    path = g_file_get_path (gtk_css_section_get_file (section));
    g_string_append_printf (errors, "%s:%u - error: ",
                            path, gtk_css_section_get_end_line (section) + 1);
    g_free (path);

    if (error->domain == GTK_CSS_PROVIDER_ERROR)
        append_error_value (errors, GTK_TYPE_CSS_PROVIDER_ERROR, error->code);
    else
    {
        g_string_append_printf (errors, "%s %u",
                                g_quark_to_string (error->domain),
                                error->code);
    }
}

void orage_css_set_theme (void)
{
    gchar *file;
    gchar **files;
    GtkCssProvider *provider;
    GdkDisplay *display;
    GdkScreen *screen;
    GString *errors;

    provider = gtk_css_provider_new ();
    display = gdk_display_get_default ();
    screen = gdk_display_get_default_screen (display);

    gtk_style_context_add_provider_for_screen (
            screen,
            GTK_STYLE_PROVIDER (provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    file = g_build_filename (xfce_get_homedir (), ".themes",
                             ORAGE_CSS_VERSION, ORAGE_CSS_NAME, NULL);

    g_debug ("loading CSS theme from '%s'", file);

    if (g_file_test (file, G_FILE_TEST_EXISTS) == FALSE)
    {
        g_free (file);
        files = xfce_resource_lookup_all (XFCE_RESOURCE_THEMES,
                                          ORAGE_DEFAULT_THEME);

        if ((files == NULL) || (files[0] == NULL))
        {
            g_warning ("CSS theme '" ORAGE_CSS_VERSION "/" ORAGE_CSS_NAME
                       "' not found in themes directories");
            return;
        }

        file = g_strdup (files[0]);
        g_strfreev (files);
    }

    errors = g_string_new ("");
    g_signal_connect (provider, "parsing-error",
                      G_CALLBACK (parsing_error_cb), errors);

    gtk_css_provider_load_from_path (provider, file, NULL);

    if (errors->str[0])
        g_warning ("failed to parse CSS file '%s': %s", file, errors->str);
    else
        g_debug ("CSS theme loaded sucsessfully from '%s'", file);

    g_string_free (errors, TRUE);
    g_free (file);
    g_object_unref (provider);
}
