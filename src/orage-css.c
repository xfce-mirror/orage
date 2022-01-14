/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2021 Erkki Moorits  (erkki.moorits at mail.ee)
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
       Free Software Foundation
       51 Franklin Street, 5th Floor
       Boston, MA 02110-1301 USA

 */

#include "orage-css.h"
#include <gtk/gtk.h>
#include <glib.h>

static const gchar orage_css_style[] =
"orage_tray_icon" \
"{\n" \
"   background-color: rgb(239, 235, 230);\n" \
"   border-color: black;\n" \
"   border-radius: 16px;\n" \
"   border-top-width: 2px;\n" \
"   border-right-width: 5px;\n" \
"   border-bottom-width: 5px;\n" \
"   border-left-width: 2px;\n" \
"   border-style:solid;\n" \
"   font-size: 85px;\n" \
"}" \
"orage_tray_icon > row1" \
"{\n" \
"   font-family: Ariel;\n" \
"   font-size: 41%;\n" \
"   color: blue;\n" \
"}" \
"orage_tray_icon > row2" \
"{\n" \
"   font-family: Sans;\n" \
"   font-weight: bold;\n" \
"   font-size: 100%;\n" \
"   color: red;\n" \
"}\n" \
"orage_tray_icon > row3" \
"{\n" \
"   font-family: Ariel;\n" \
"   font-weight: bold;\n" \
"   font-size: 44%;\n" \
"   color: blue;\n" \
"}\n" \
"#" ORAGE_DAY_VIEW_TODAY \
"{\n" \
"   background: gold;\n" \
"   font-weight: bold;\n" \
"   border-width: 1px;\n" \
"}\n"\
"#" ORAGE_DAY_VIEW_SUNDAY \
"{\n" \
"   color: red;\n" \
"}"\
"#" ORAGE_DAY_VIEW_ALL_DAY_EVENT \
"{\n" \
"   background: lightgray;\n" \
"}"\
"#" ORAGE_DAY_VIEW_ODD_HOURS \
"{\n" \
"   background: lightgray;\n" \
"   padding: 4px\n" \
"}" \
"#" ORAGE_DAY_VIEW_EVEN_HOURS \
"{\n" \
"   background: rgb(239, 235, 230);\n" \
"   padding: 4px\n" \
"}" \
"#" ORAGE_DAY_VIEW_SEPARATOR_BAR \
"{\n" \
"   background: white;\n" \
"   min-width: 3px;\n" \
"}\n" \
"#" ORAGE_DAY_VIEW_OCCUPIED_HOUR_LINE \
"{\n" \
"   background: black;\n" \
"   min-width: 3px;\n" \
"}\n" \
"#" ORAGE_DAY_VIEW_TASK_SEPARATOR \
"{\n" \
"   background: black;\n" \
"   min-width: 1px;\n" \
"}\n" \
"#" ORAGE_MAINBOX_RED \
"{\n" \
"   color: red;\n" \
"}\n" \
"#" ORAGE_MAINBOX_BLUE \
"{\n" \
"   color: blue;\n" \
"}";

static gboolean css_registered = FALSE;

void register_css_provider (void)
{
    GtkCssProvider *provider;
    GdkDisplay *display;
    GdkScreen *screen;

    if (css_registered == TRUE)
        return;

    provider = gtk_css_provider_new ();
    display = gdk_display_get_default ();
    screen = gdk_display_get_default_screen (display);

    gtk_style_context_add_provider_for_screen (
            screen,
            GTK_STYLE_PROVIDER (provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                     orage_css_style, -1, NULL);

    g_object_unref (provider);
    css_registered = TRUE;
}
