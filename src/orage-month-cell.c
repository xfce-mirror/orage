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
#include "functions.h"

struct _OrageMonthCell
{
    /* Add GtkOverlay as a main container for this widgets */
    GtkBox parent;

    GtkWidget *main_box;
    GtkWidget *day_label;
    GDateTime *date;
    gboolean different_month;
};

G_DEFINE_TYPE (OrageMonthCell, orage_month_cell, GTK_TYPE_BOX)

static void orage_month_cell_class_init (
    G_GNUC_UNUSED OrageMonthCellClass *klass)
{
}

static void orage_month_cell_init (OrageMonthCell *self)
{
    gtk_widget_set_name (GTK_WIDGET (self), "orage-month-cell");

    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    self->day_label = gtk_label_new (NULL);
    self->different_month = TRUE;

    gtk_box_pack_start (GTK_BOX (self->main_box), self->day_label, FALSE, FALSE,
                        0);

    gtk_box_pack_start (GTK_BOX (&self->parent), self->main_box, FALSE, FALSE,
                        0);
}

GtkWidget *orage_month_cell_new (void)
{
    return g_object_new (ORAGE_MONTH_CELL_TYPE, NULL);
}

void orage_month_cell_clear (OrageMonthCell *self)
{
    GtkStyleContext *context;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    orage_gdatetime_unref (self->date);
    self->date = NULL;
    self->different_month = TRUE;

    context = gtk_widget_get_style_context (GTK_WIDGET (self));
    gtk_style_context_remove_class (context, "today");
    gtk_style_context_remove_class (context, "highlighted");

    gtk_label_set_text (GTK_LABEL (self->day_label), NULL);
}

void orage_month_cell_set_date (OrageMonthCell *self, GDateTime *date)
{
    GtkStyleContext *context;
    GDateTime *today;
    gchar *text;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    if (self->date &&
        date &&
        orage_gdatetime_days_between (self->date, date) == 0)
    {
        return;
    }

    orage_gdatetime_unref (self->date);
    self->date = g_date_time_ref (date);

    text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));

    gtk_label_set_text (GTK_LABEL (self->day_label), text);
    g_free (text);

    context = gtk_widget_get_style_context (GTK_WIDGET (self));
    today = g_date_time_new_now_local ();
    if (orage_gdatetime_days_between (self->date, today) == 0)
        gtk_style_context_add_class (context, "today");
    else
        gtk_style_context_remove_class (context, "today");

    g_date_time_unref (today);
}

GDateTime *orage_month_cell_get_date (OrageMonthCell *self)
{
    g_return_val_if_fail (ORAGE_IS_MONTH_CELL (self), NULL);

    return g_date_time_ref (self->date);
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

    self->different_month = different;
}

void orage_month_cell_set_highlight (OrageMonthCell *self,
                                     const gboolean highlighted)
{
    GtkStyleContext *context;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    if (self->different_month)
        return;

    context = gtk_widget_get_style_context (GTK_WIDGET (self));

    if (highlighted)
        gtk_style_context_add_class (context, "highlighted");
    else
        gtk_style_context_remove_class (context, "highlighted");
}
