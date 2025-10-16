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

#ifndef ORAGE_MONTH_VIEW_H
#define ORAGE_MONTH_VIEW_H 1

#include "orage-event.h"
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
    MONDAY,
    SUNDAY,
    BY_LOCALE
} FirstDayOfWeek;

#define ORAGE_MONTH_VIEW_TYPE (orage_month_view_get_type ())

G_DECLARE_FINAL_TYPE (OrageMonthView, orage_month_view, ORAGE, MONTH_VIEW, GtkBox)

GtkWidget *orage_month_view_new (FirstDayOfWeek first_day);

/**
 * orage_month_view_reset_month_cells:
 * @self: (nullable): an #OrageMonthView instance
 *
 * Resets all month cells within the month view to match the current month.
 *
 * This function iterates over all cells in the month grid (6 rows × 7 columns),
 * clears each cell, assigns a new date based on the current month and the
 * first visible weekday, and marks whether the cell belongs to a different
 * month.
 *
 * It does not create or destroy cell widgets; existing cells are reused.
 * The function only resets their state and date information.
 *
 * This function does not emit the "reload-requested" signal. It is typically
 * used together with orage_month_view_set_event(), which populates the month
 * view with event data after all cells have been reset and initialized.
 *
 * The resulting grid represents the complete month view layout that
 * corresponds to the date returned by
 * orage_month_view_get_first_day_of_month().
 */
void orage_month_view_reset_month_cells (OrageMonthView *self);

void orage_month_view_set_month (OrageMonthView *self, GDateTime *gdt);
void orage_month_view_mark_date (OrageMonthView *self, GDateTime *gdt);
GDateTime *orage_month_view_get_first_date (OrageMonthView *self);
GDateTime *orage_month_view_get_last_date (OrageMonthView *self);
void orage_month_view_set_event (OrageMonthView *self, OrageEvent *event);

G_END_DECLS

#endif
