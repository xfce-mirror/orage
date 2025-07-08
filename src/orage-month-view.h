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

#include <gtk/gtk.h>
#include <glib.h>

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
void orage_month_view_select_month (OrageMonthView *self, GDateTime *gdt);
GDateTime *orage_month_view_return_first_date (OrageMonthView *self);
GDateTime *orage_month_view_return_last_date (OrageMonthView *self);

G_END_DECLS

#endif
