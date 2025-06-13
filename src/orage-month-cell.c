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

void orage_month_cell_set_date (OrageMonthCell *self, GDateTime *date)
{
    g_autofree gchar *text = NULL;

    if (self->date && date
        && orage_date_time_compare_date (self->date, date) == 0)
        return;

    orage_clear_date_time (&self->date);
    self->date = g_date_time_ref (date);

    text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));

    gtk_label_set_text (GTK_LABEL (self->day_label), text);
}

static void orage_month_cell_init (OrageMonthCell *self)
{
    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    self->day_label = gtk_label_new ("x");

    gtk_box_pack_start (GTK_BOX (self->main_box), self->day_label, FALSE, FALSE,
                        0);

    gtk_box_pack_start (GTK_BOX (&self->parent), self->main_box, FALSE, FALSE,
                        0);
}

static void orage_month_cell_class_init (OrageMonthCellClass *klass)
{
}

GtkWidget *orage_month_cell_new (void)
{
    return g_object_new (ORAGE_MONTH_CELL_TYPE, NULL);
}
