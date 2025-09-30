/*
 * Copyright (c) 2023-2025 Erkki Moorits
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

#include "orage-alarm-structure.h"
#include "orage-time-utils.h"
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

    alarm->active_alarm = g_new0 (active_alarm_struct, 1);
    alarm->orage_display_data = g_new0 (orage_ddmmhh_hbox_struct, 1);
    alarm->ref_count = 1;

    return alarm;
}

alarm_struct *orage_alarm_ref (alarm_struct *alarm)
{
    g_return_val_if_fail (alarm != NULL, NULL);
    g_return_val_if_fail (alarm->ref_count > 0, NULL);

    g_atomic_int_inc (&alarm->ref_count);

    return alarm;
}

void orage_alarm_unref (alarm_struct *alarm)
{
    g_return_if_fail (alarm != NULL);
    g_return_if_fail (alarm->ref_count > 0);

    if (g_atomic_int_dec_and_test (&alarm->ref_count))
        orage_alarm_free (alarm);
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

alarm_struct *orage_alarm_copy (const alarm_struct *l_alarm)
{
    alarm_struct *n_alarm;

    g_return_val_if_fail (l_alarm != NULL, NULL);
    g_return_val_if_fail (l_alarm->ref_count > 0, NULL);

    n_alarm = orage_alarm_new ();

    /* first l_alarm values which are not modified */
    if (l_alarm->alarm_time != NULL)
        n_alarm->alarm_time = g_date_time_ref (l_alarm->alarm_time);

    if (l_alarm->action_time != NULL)
        n_alarm->action_time = g_strdup (l_alarm->action_time);

    if (l_alarm->uid != NULL)
        n_alarm->uid = g_strdup (l_alarm->uid);

    if (l_alarm->title != NULL)
        n_alarm->title = orage_process_text_commands (l_alarm->title);

    if (l_alarm->description != NULL)
    {
        n_alarm->description =
                orage_process_text_commands (l_alarm->description);
    }

    n_alarm->persistent = l_alarm->persistent;
    n_alarm->temporary = l_alarm->temporary;
    n_alarm->notify_timeout = l_alarm->notify_timeout;

    if (l_alarm->sound != NULL)
        n_alarm->sound = g_strdup (l_alarm->sound);

    if (l_alarm->sound_cmd != NULL)
        n_alarm->sound_cmd = g_strdup (l_alarm->sound_cmd);

    n_alarm->repeat_delay = l_alarm->repeat_delay;
    n_alarm->procedure = l_alarm->procedure;

    if (l_alarm->cmd != NULL)
        n_alarm->cmd = g_strdup (l_alarm->cmd);

    n_alarm->display_orage = l_alarm->display_orage;
    n_alarm->display_notify = l_alarm->display_notify;
    n_alarm->audio = l_alarm->audio;
    n_alarm->repeat_cnt = l_alarm->repeat_cnt;

    return n_alarm;
}
