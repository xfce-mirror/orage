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

#include "orage-event-preview.h"

#include "ical-code.h"
#include <gtk/gtk.h>
#include <glib.h>

#define EVENT_PREVIEW_CALENDAR_COMPONENT "event-preview-cal-component"

struct _OrageEventPreview
{
    GtkWidget __parent__;

    OrageCalendarComponent *cal_comp;
};

enum
{
    PROP_CALENDAR_COMPONENT = 1
};

G_DEFINE_TYPE (OrageEventPreview, orage_event_preview, GTK_TYPE_WIDGET)

static void orage_event_preview_set_property (GObject *object,
                                              const guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec)
{
    OrageEventPreview *self = ORAGE_EVENT_PREVIEW (object);

    switch (prop_id)
    {
        case PROP_CALENDAR_COMPONENT:
            g_assert (self->cal_comp == NULL);
            self->cal_comp = (OrageCalendarComponent *)g_value_dup_object (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void orage_event_preview_get_property (GObject *object,
                                              const guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec)
{
    const OrageEventPreview *self = ORAGE_EVENT_PREVIEW (object);

    switch (prop_id)
    {
        case PROP_CALENDAR_COMPONENT:
            g_value_set_object (value, self->cal_comp);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void orage_event_preview_finalize (GObject *object)
{
    OrageEventPreview *self = (OrageEventPreview *)object;

    g_object_unref (self->cal_comp);
    G_OBJECT_CLASS (orage_event_preview_parent_class)->finalize (object);
}

static void orage_event_preview_constructed (GObject *object)
{
    /* TODO: create widget*/
    G_OBJECT_CLASS (orage_event_preview_parent_class)->constructed (object);
}

static void orage_event_preview_class_init (OrageEventPreviewClass *klass)
{
    GParamSpec *param_specs;
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->constructed = orage_event_preview_constructed;
    object_class->finalize = orage_event_preview_finalize;
    object_class->get_property = orage_event_preview_get_property;
    object_class->set_property = orage_event_preview_set_property;

    param_specs = g_param_spec_object (EVENT_PREVIEW_CALENDAR_COMPONENT,
                                       EVENT_PREVIEW_CALENDAR_COMPONENT,
                                       "Calendar compoenent",
                                       ORAGE_EVENT_PREVIEW_TYPE,
                                       G_PARAM_READWRITE |
                                       G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_property (object_class,
                                     PROP_CALENDAR_COMPONENT,
                                     param_specs);
}

static void orage_event_preview_init (OrageEventPreview *self)
{
}

GtkWidget *orage_event_preview_new_from_cal_comp (
    OrageCalendarComponent *cal_comp)
{
    return g_object_new (ORAGE_EVENT_PREVIEW_TYPE,
                         EVENT_PREVIEW_CALENDAR_COMPONENT, cal_comp,
                         NULL);
}
