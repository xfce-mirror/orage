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

void orage_alarm_free (alarm_struct *alarm)
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
