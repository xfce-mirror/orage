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

#include "orage-event.h"

struct _OrageEvent
{
    GObject parent;

    gchar *uid;
    gchar *description;

    GDateTime *dt_start;
};

G_DEFINE_TYPE (OrageEvent, orage_event, G_TYPE_OBJECT)

static void orage_event_finalize (GObject *object)
{
    OrageEvent *self = (OrageEvent *)object;

    g_clear_pointer (&self->description, g_free);
    g_clear_pointer (&self->uid, g_free);

    G_OBJECT_CLASS (orage_event_parent_class)->finalize (object);
}

static void orage_event_init (OrageEvent *self)
{
}

static void orage_event_class_init (OrageEventClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orage_event_finalize;
}

OrageEvent *orage_event_new (void)
{
    return g_object_new (ORAGE_EVENT_TYPE, NULL);
}

const gchar *orage_event_get_description (OrageEvent *self)
{
    g_return_val_if_fail (ORAGE_IS_EVENT (self), NULL);

    return self->description ? self->description : "";
}

void orage_event_set_description (OrageEvent *self, const gchar *description)
{
    g_return_if_fail (ORAGE_IS_EVENT (self));

    g_clear_pointer (&self->description, g_free);
    self->description = g_strdup (description);
}

void orage_event_set_uid (OrageEvent *self, const gchar *uid)
{
    g_return_if_fail (ORAGE_IS_EVENT (self));

    g_clear_pointer (&self->uid, g_free);
    self->uid = g_strdup (uid);
}

gint orage_event_equal_by_id (gconstpointer event1, gconstpointer event2)
{
    OrageEvent *e1 = (OrageEvent *)event1;
    OrageEvent *e2 = (OrageEvent *)event2;

    g_return_val_if_fail (ORAGE_IS_EVENT (e1), -1);
    g_return_val_if_fail (ORAGE_IS_EVENT (e2), -1);

    return g_strcmp0 (e1->uid, e2->uid);
}

gint orage_event_compare (gconstpointer event1, gconstpointer event2)
{
    OrageEvent *e1 = (OrageEvent *)event1;
    OrageEvent *e2 = (OrageEvent *)event2;

    if ((e1 == NULL) && (e2 == NULL))
        return 0;
    if (e1 == NULL)
        return 1;
    else if (e2 == NULL)
        return -1;

    g_return_val_if_fail (ORAGE_IS_EVENT (e1), 0);
    g_return_val_if_fail (ORAGE_IS_EVENT (e2), 0);

    return g_date_time_compare (e1->dt_start, e2->dt_start);
}