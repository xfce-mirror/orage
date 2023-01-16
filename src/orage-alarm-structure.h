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

#ifndef ORAGE_ALARM_STRUCTURE_H
#define ORAGE_ALARM_STRUCTURE_H 1

#include <gtk/gtk.h>
#include <glib.h>

typedef struct _active_alarm_struct
{
    /** Sound is currently being played. */
    gboolean sound_active;
    GtkWidget *stop_noise_reminder;

    /** This is NotifyNotification, but it may not be linked in, so need to be
     *  done like this.
     */
    gpointer active_notify;
    gboolean notify_stop_noise_action;
} active_alarm_struct;

typedef struct _orage_ddmmhh_hbox
{
    GtkWidget *time_hbox
       , *spin_dd, *spin_dd_label
       , *spin_hh, *spin_hh_label
       , *spin_mm, *spin_mm_label;
    GtkWidget *dialog;
} orage_ddmmhh_hbox_struct;

typedef struct _alarm_struct
{
    GDateTime *alarm_time;

    /** Alarm is based on this time. */
    gchar   *action_time;
    gchar   *uid;
    gchar   *title;
    gchar   *description;
    gboolean persistent;

    /** Alarm, which is not stored in ical file. */
    gboolean temporary;

    gboolean display_orage;
    gboolean display_notify;
    gint     notify_timeout;

    gboolean audio;
    gboolean audio_orig;
    gchar   *sound;

    /** Contains the whole command to play. */
    gchar   *sound_cmd;
    gint     repeat_cnt;
    gint     repeat_delay;

    gboolean procedure;
    gchar   *cmd;
    
#if 0
    gboolean email;
#endif

    /** This is used to control active alarms. */
    active_alarm_struct *active_alarm;

    /** Pointer to special data needed for orage window alarm. */
    gpointer orage_display_data;

    gint ref_count;
} alarm_struct;

alarm_struct *orage_alarm_new (void);
alarm_struct *orage_alarm_ref (alarm_struct *alarm);
void orage_alarm_unref (alarm_struct *alarm);
gint orage_alarm_order (gconstpointer a, gconstpointer b);
alarm_struct *orage_alarm_copy (const alarm_struct *l_alarm);

#endif
