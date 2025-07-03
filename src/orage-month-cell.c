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

#include "orage-month-cell.h"
#include "orage-time-util.h"

struct _OrageMonthCell
{
    /* Add GtkOverlay as a main container for this widgets */
    GtkBox parent;

    GtkWidget *main_box;
    GtkWidget *day_label;
    GDateTime *date;
};

G_DEFINE_TYPE (OrageMonthCell, orage_month_cell, GTK_TYPE_BOX)

static void load_css (void)
{
    GtkCssProvider *provider = gtk_css_provider_new ();
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);

    gtk_style_context_add_provider_for_screen (screen,
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_USER);

    gtk_css_provider_load_from_data (provider,
                                    ".out-of-month {\n"
                                    "  background-color: rgba(220, 220, 220, 0.4);\n"
                                    "}\n",
                                    -1, NULL);

    g_object_unref (provider);
}

static void orage_month_cell_class_init (OrageMonthCellClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    load_css ();

    gtk_widget_class_set_css_name (widget_class, "monthcell");
}

static void orage_month_cell_init (OrageMonthCell *self)
{
    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    self->day_label = gtk_label_new (NULL);

    gtk_box_pack_start (GTK_BOX (self->main_box), self->day_label, FALSE, FALSE,
                        0);

    gtk_box_pack_start (GTK_BOX (&self->parent), self->main_box, FALSE, FALSE,
                        0);
}

GtkWidget *orage_month_cell_new (void)
{
    return g_object_new (ORAGE_MONTH_CELL_TYPE, NULL);
}

void orage_month_cell_set_date (OrageMonthCell *self, GDateTime *date)
{
    gchar *text;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    if (self->date &&
        date &&
        orage_date_time_compare_date (self->date, date) == 0)
    {
        return;
    }

    orage_clear_date_time (&self->date);
    self->date = g_date_time_ref (date);

    text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));

    gtk_label_set_text (GTK_LABEL (self->day_label), text);

    g_free (text);
}

void orage_month_cell_set_different_month (OrageMonthCell *self,
                                           const gboolean different)
{
    GtkStyleContext *context;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    context = gtk_widget_get_style_context (GTK_WIDGET (self));

    if (different)
        gtk_style_context_add_class (context, "out-of-month");
    else
        gtk_style_context_remove_class (context, "out-of-month");
}
