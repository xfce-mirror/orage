/*
 * Copyright (c) 2021-2023 Erkki Moorits
 * Copyright (c) 2006-2013 Juha Kautto  (juha at xfce.org)
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

#ifndef __ORAGE_PARAMETERS_H__
#define __ORAGE_PARAMETERS_H__

#include "orage-task-runner.h"
#include "orage-application.h"
#include <glib.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct _foreign_file
{
    char *file;
    gboolean read_only;
    time_t latest_file_change;
    char *name;
} foreign_file;

typedef struct _parameters
{
    /* main window settings */
    gboolean select_always_today;
    gboolean show_menu;
    gboolean show_borders;
    gboolean show_heading;
    gboolean show_day_names;
    gboolean show_weeks;
    gboolean show_todos;
    gint     show_event_days;
    gboolean show_pager;
#ifdef HAVE_X11_TRAY_ICON
    gboolean show_systray;
#endif
    gboolean show_taskbar;
    gboolean start_visible;
    gboolean start_minimized;
    gboolean set_stick;
    gboolean set_ontop;
    
    /* ical week start day (0 = Monday, 1 = Tuesday,... 6 = Sunday) */
    gint ical_weekstartday;

    /* timezone handling */
    char *local_timezone;
    gboolean local_timezone_utc;

    /* archiving */
    int archive_limit;
    char *archive_file;

    /* foreign files */
    gint foreign_count;
    foreign_file foreign_data[10];
    gboolean use_foreign_display_alarm_notify; /* changes alarm to notify */

    /* other */
    gchar *orage_file;
    time_t latest_file_change;
    char *sound_application;

    /* List of active alarms */
    GList *alarm_list;

    /* alarm timer id and timeout in millisecs */
    guint alarm_timer; /* monitors next alarm */
    guint day_timer;   /* fires when day changes = every 24 hours */
    guint tooltip_timer; /* keeps tooltips upto date */
    guint wakeup_timer;  /* controls wakeup after suspend/hibernate */

    /* main window */
    void *xfcal;     /* this is main calendar CalWin * */
    gint pos_x, pos_y;
    gint size_x, size_y;

    /* tray icon */
    void *trayIcon; /* this is GtkStatusIcon * */

    char *own_icon_row1_data;
    char *own_icon_row2_data;
    char *own_icon_row3_data;
    gint own_icon_row3_y;

    /* event-list window */
    gint el_pos_x, el_pos_y;
    gint el_size_x, el_size_y;
    gint el_days;
    gboolean el_only_first;

    /* day view window */
    gint dw_pos_x, dw_pos_y;
    gint dw_size_x, dw_size_y;
    gboolean dw_week_mode;

    /* show days window from main calendar */
    gboolean show_days; /* true=show days false=show events */

    /* Controls which appointment priorities are shown in daylist */
    gint priority_list_limit;

    /* some systems are not able to wake up properly from suspend/hibernate
       and we need to monitor the status ourselves */
    gboolean use_wakeup_timer;

    /* always quit instead of going to background when asked to close */
    gboolean close_means_quit;

    /* delayed close length in seconds. 0 = close immediately */
    gint file_close_delay;

    /** Number of sync sources. */
    gint sync_source_count;

    /** Sync configurations. */
    orage_task_runner_conf sync_conf[10];
} global_parameters; /* global parameters */

extern global_parameters g_par;

void show_parameters(void);
void write_parameters(void);

/** Read parameters from config file.
 *  @param appl parent application
 */
void read_parameters (OrageApplication *appl);

void set_parameters(void);

#endif /* !__ORAGE_PARAMETERS_H__ */
