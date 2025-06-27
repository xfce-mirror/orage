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

#include "orage-month-view.h"
#include "orage-month-cell.h"
#include "orage-time-util.h"
#include "orage-i18n.h"

#include <gtk/gtk.h>

struct _OrageMonthView
{
    GtkBox parent;

    GtkWidget *main_box;
    GtkWidget *month_grid;
    GtkWidget *label_weekday[7];
    GtkWidget *month_cell[6][7];
    GtkWidget *header;

    GDateTime *date;
    gint days_delay;
    gint first_weekday;
};

static const gchar *day_name[] =
{
    NULL, /* Holde for week number */
    _("Sunday"), _("Monday"), _("Tuesday"), _("Wednesday"), _("Thursday"),
    _("Friday"), _("Saturday")
};

static void update_month_cells (OrageMonthView *self);

static void update_days_delay (OrageMonthView *self, gint month_different);

G_DEFINE_TYPE (OrageMonthView, orage_month_view, GTK_TYPE_BOX)

static void update_days_delay (OrageMonthView *self, gint month_different)
{
    gint year = g_date_time_get_year (self->date);
    gint month = g_date_time_get_month (self->date);
    GDate *gd = g_date_new_dmy (1, month, year);

    if (month_different >= 0)
        g_date_add_months (gd, month_different);
    else
        g_date_subtract_months (gd, -1 * month_different);

    self->days_delay = (g_date_get_weekday (gd) - self->first_weekday + 7) % 7;

    g_date_free (gd);
}

static void setup_month_grid (OrageMonthView *self)
{
    guint row, col;

    for (row = 0; row < 6; row++)
    {
        for (col = 1; col < 8; col++)
        {
            GtkWidget *cell;

            cell = orage_month_cell_new ();
            gtk_widget_set_hexpand (cell, TRUE);
            gtk_widget_set_vexpand (cell, TRUE);
            gtk_widget_set_halign (cell, GTK_ALIGN_FILL);
            gtk_widget_set_valign (cell, GTK_ALIGN_FILL);

            gtk_grid_attach (GTK_GRID (self->month_grid), cell, col, (row + 1),
                             1, 1);
            self->month_cell[row][col - 1] = cell;
        }
    }
}

static void update_month_cells (OrageMonthView *self)
{
    guint row, col;
    g_autoptr (GDateTime) dt = NULL;

    dt = g_date_time_new_local (g_date_time_get_year (self->date),
                                g_date_time_get_month (self->date), 1, 0, 0, 0);

    for (row = 0; row < 6; row++)
    {
        for (col = 0; col < 7; col++)
        {
            g_autoptr (GDateTime) cell_date = NULL;
            OrageMonthCell *cell;

            cell = ORAGE_MONTH_CELL (self->month_cell[row][col]);

            cell_date = g_date_time_add_days (dt,
                                              row * 7 + col - self->days_delay);
            orage_month_cell_set_date (cell, cell_date);
        }
    }
}

static void orage_month_view_class_init (OrageMonthViewClass *klass)
{
}

static void orage_month_view_init (OrageMonthView *self)
{
    GtkWidget *day_label;
    guint i;

    self->date = g_date_time_new_now_utc ();
    self->first_weekday = 0;
    update_days_delay (self, -1);

    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    self->month_grid = gtk_grid_new ();
    gtk_widget_set_hexpand (self->month_grid, TRUE);
    gtk_widget_set_vexpand (self->month_grid, TRUE);
    gtk_widget_set_halign (self->month_grid, GTK_ALIGN_FILL);
    gtk_widget_set_valign (self->month_grid, GTK_ALIGN_FILL);

    for (i = 0; i < 8; i++)
    {
        day_label = gtk_label_new_with_mnemonic (day_name[i]);
        gtk_widget_set_hexpand (day_label, TRUE);
        gtk_widget_set_vexpand (day_label, TRUE);
        gtk_widget_set_halign (day_label, GTK_ALIGN_FILL);
        gtk_widget_set_valign (day_label, GTK_ALIGN_FILL);
        gtk_grid_attach (GTK_GRID (self->month_grid), day_label, i, 0, 1, 1);
        self->label_weekday[i] = day_label;
    }

    setup_month_grid (self);
    update_month_cells (self);

    gtk_box_pack_start (GTK_BOX (self->main_box), self->month_grid, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (&self->parent), self->main_box, TRUE, TRUE, 0);
}

GtkWidget *orage_month_view_new (void)
{
    return g_object_new (ORAGE_MONTH_VIEW_TYPE, NULL);
}

void orage_month_view_next_month (OrageMonthView *month_view)
{
    update_days_delay (month_view, 1);

    month_view->date = g_date_time_add_months (month_view->date, 1);
    update_month_cells (month_view);
}

void orage_month_view_previous_month (OrageMonthView *month_view)
{
    update_days_delay (month_view, -1);

    month_view->date = g_date_time_add_months (month_view->date, -1);
    update_month_cells (month_view);
}