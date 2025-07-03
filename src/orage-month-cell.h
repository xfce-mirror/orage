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

#ifndef ORAGE_MONTH_CELL_H
#define ORAGE_MONTH_CELL_H 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ORAGE_MONTH_CELL_TYPE (orage_month_cell_get_type ())

G_DECLARE_FINAL_TYPE (OrageMonthCell, orage_month_cell, ORAGE, MONTH_CELL, GtkBox)

GtkWidget *orage_month_cell_new (void);

void orage_month_cell_set_date (OrageMonthCell *self, GDateTime *date);
void orage_month_cell_set_different_month (OrageMonthCell *self,
                                           gboolean different);

G_END_DECLS

#endif
