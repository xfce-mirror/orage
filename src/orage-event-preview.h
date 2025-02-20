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

#ifndef ORAGE_EVENT_PREVIEW_H
#define ORAGE_EVENT_PREVIEW_H 1

#include "ical-code.h"
#include <gtk/gtk.h>
#include <glib.h>

G_BEGIN_DECLS

#define ORAGE_TYPE_EVENT_PREVIEW (orage_event_preview_get_type ())

G_DECLARE_FINAL_TYPE (OrageEventPreview, orage_event_preview, ORAGE,
                      EVENT_PREVIEW, GtkWidget)

/** Create new event preview widget from calendar component.
 *  @param cal_comp Orage calendar component
 *  @return event preview widget
 */
GtkWidget *orage_event_preview_new_from_cal_comp (
    OrageCalendarComponent *cal_comp);

G_END_DECLS

#endif
