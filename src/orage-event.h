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


#ifndef ORAGE_EVENT_H
#define ORAGE_EVENT_H 1

#include <gtk/gtk.h>
#include <glib.h>

G_BEGIN_DECLS

#define ORAGE_EVENT_TYPE (orage_event_get_type ())

G_DECLARE_FINAL_TYPE (OrageEvent, orage_event, ORAGE, EVENT, GObject)

/**
 * orage_event_new:
 *
 * Creates a new #OrageEvent object. The returned event has no associated
 * calendar or component data.
 *
 * Returns: (transfer full): a newly created #OrageEvent
 */
OrageEvent *orage_event_new (void);

/**
 * orage_event_set_date_start:
 * @self: a #OrageEvent
 * @gdt: a #GDateTime
 *
 * Sets the start date of @self to @gdt.
 */
void orage_event_set_date_start (OrageEvent *self, GDateTime *gdt);

/**
 * orage_event_get_date_start:
 * @self: a #OrageEvent
 *
 * Retrieves the start date of @self.
 *
 * Returns: (transfer none): a #GDateTime.
 */
GDateTime *orage_event_get_date_start (OrageEvent *self);

/**
 * orage_event_set_date_end:
 * @self: a #OrageEvent
 * @gdt: a #GDateTime
 *
 * Sets the end date of @self to @gdt.
 */
void orage_event_set_date_end (OrageEvent *self, GDateTime *gdt);

/**
 * orage_event_get_date_end:
 * @self: a #OrageEvent
 *
 * Retrieves the end date of @self.
 *
 * Returns: (transfer none): a #GDateTime.
 */
GDateTime *orage_event_get_date_end (OrageEvent *self);

/**
 * orage_event_get_description:
 * @self: a #OrageEvent
 *
 * Retrieves the description of @self.
 *
 * Returns: (transfer none): the description of the event,
 *          or an empty string if none is set.
 */
const gchar *orage_event_get_description (OrageEvent *self);

/**
 * orage_event_set_description:
 * @self: a #OrageEvent
 * @description: (nullable): the new description text
 *
 * Sets the description of @self to a copy of @description.
 * If a previous description exists, it will be freed.
 * If @description is %NULL, the description will be cleared.
 */
void orage_event_set_description (OrageEvent *self, const gchar *description);

/**
 * orage_event_get_uid:
 * @self: a #OrageEvent
 *
 * Gets the unique identifier (UID) of @self.
 *
 * Returns: (transfer none): the UID string of @self, or an empty string if no
 *          UID has been set. The returned string is owned by @self and  must
 *          not be freed or modified.
 */
const gchar *orage_event_get_uid (OrageEvent *self);

/**
 * orage_event_set_uid:
 * @self: a #OrageEvent
 * @uid: (nullable): a unique identifier string
 *
 * Sets the unique identifier (UID) of @self to a copy of @uid.
 * If a previous UID exists, it will be freed.
 * If @uid is %NULL, the description will be cleared.
 */
void orage_event_set_uid (OrageEvent *self, const gchar *uid);

/**
 * orage_event_equal_by_id:
 * @event1: a #OrageEvent
 * @event2: a #OrageEvent
 *
 * Compares two events for equality based on their unique IDs (UIDs).
 * This function is suitable for use with g_list_find_custom() or
 * other operations where you need to check if an event with the
 * same UID already exists.
 *
 * Returns: 0 if the UIDs are equal, a non-zero integer otherwise.
 *          %NULL UIDs are treated as empty strings.
 */
gint orage_event_equal_by_id (gconstpointer event1, gconstpointer event2);

/**
 * orage_event_compare:
 * @event1: a #OrageEvent
 * @event2: a #OrageEvent
 *
 * Compares two events for ordering based on their start times.
 * This function is suitable for use with g_list_sort() or
 * other sorting routines. Events with %NULL pointers are
 * considered greater than non-NULL events.
 *
 * Returns: a negative integer if @event1 starts before @event2,
 *          0 if they have the same start time or either is invalid,
 *          or a positive integer if @event1 starts after @event2.
 */
gint orage_event_compare (gconstpointer event1, gconstpointer event2);

G_END_DECLS

#endif
