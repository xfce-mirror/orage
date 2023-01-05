/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2023 Erkki Moorits
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
       Free Software Foundation
       51 Franklin Street, 5th Floor
       Boston, MA 02110-1301 USA

 */

#include "orage-alarm-structure.h"
#include "functions.h"
#include <glib.h>


static void orage_alarm_free (alarm_struct *alarm)
{
    orage_gdatetime_unref (alarm->alarm_time);
    g_free (alarm->action_time);
    g_free (alarm->uid);
    g_free (alarm->title);
    g_free (alarm->description);
    g_free (alarm->sound);
    g_free (alarm->sound_cmd);
    g_free (alarm->cmd);
    g_free (alarm->active_alarm);
    g_free (alarm->orage_display_data);
    g_free (alarm);
}

alarm_struct *orage_alarm_new (void)
{
    alarm_struct *alarm = g_new0 (alarm_struct, 1);

    alarm->ref_count = 1;

    g_debug ("%s: %p", G_STRFUNC, (void *)alarm);

    return alarm;
}

alarm_struct *orage_alarm_ref (alarm_struct *alarm)
{
    g_return_val_if_fail (alarm != NULL, NULL);
    g_return_val_if_fail (alarm->ref_count > 0, NULL);

    g_atomic_int_inc (&alarm->ref_count);
    g_debug ("%s: %p, refcount=%d", G_STRFUNC, (void *)alarm, alarm->ref_count);

    return alarm;
}

void orage_alarm_unref (alarm_struct *alarm)
{
    g_return_if_fail (alarm != NULL);
    g_return_if_fail (alarm->ref_count > 0);

    g_debug ("%s: %p, refcount=%d", G_STRFUNC, (void *)alarm, alarm->ref_count);

    if (g_atomic_int_dec_and_test (&alarm->ref_count))
    {
        g_debug ("%s: free %p", G_STRFUNC, (void *)alarm);
        orage_alarm_free (alarm);
    }
}

gint orage_alarm_order (gconstpointer a, gconstpointer b)
{
    const alarm_struct *alarm_a = (const alarm_struct *)a;
    const alarm_struct *alarm_b = (const alarm_struct *)b;

    if (alarm_a->alarm_time == NULL)
        return 1;
    else if (alarm_b->alarm_time == NULL)
        return -1;

    return g_date_time_compare (alarm_a->alarm_time, alarm_b->alarm_time);
}
