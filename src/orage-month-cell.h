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

/** Clear all cell data.
 *  @param self a valid OrageMonthCell instance
 */
void orage_month_cell_clear (OrageMonthCell *self);

/** Sets the date associated with this month cell.
 *  @param self a valid OrageMonthCell instance
 *  @param date a GDateTime to assign to the month cell (nullable). The date is
 *         internally referenced and the caller retains ownership.
 */
void orage_month_cell_set_date (OrageMonthCell *self, GDateTime *date);

/** Returns a new reference to the date associated with this month cell. The
 *  returned GDateTime is owned by the caller and must be freed with
 *  g_date_time_unref() when no longer needed.
 *  @param self a valid OrageMonthCell instance
 *  @return a newly referenced #GDateTime (transfer full)
 */
GDateTime *orage_month_cell_get_date (OrageMonthCell *self);

void orage_month_cell_set_different_month (OrageMonthCell *self,
                                           gboolean different);

void orage_month_cell_set_highlight (OrageMonthCell *self,
                                     gboolean highlighted);

void orage_month_cell_set_selected (OrageMonthCell *self,
                                    const gboolean selected);

/** Emits the "clicked" signal on the given "self" instance. This function can
 *  be used to manually trigger a click event on an OrageMonthCell.
 *  @param self a OrageMonthCell
 */
void orage_month_cell_emit_clicked (OrageMonthCell *self);

/** Emits the "double-clicked" signal on the given "self" instance. This
 *  function can be used to manually trigger a click event on an OrageMonthCell.
 *  @param self a OrageMonthCell
 */
void orage_month_cell_emit_double_clicked (OrageMonthCell *self);

G_END_DECLS

#endif
