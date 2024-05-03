/*
 * Copyright (c) 2021-2023 Erkki Moorits
 * Copyright (c) 2005-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2004-2006 Mickael Graf (korbinus at xfce.org)
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

#ifndef __DAY_VIEW_H__
#define __DAY_VIEW_H__

#define MAX_DAYS 40

typedef struct _day_win
{
    GtkAccelGroup *accel_group;

    GtkWidget *Window;
    GtkWidget *Vbox;

    GtkWidget *Menubar;
    GtkWidget *File_menu;
    GtkWidget *File_menu_new;
    GtkWidget *File_menu_close;
    GtkWidget *View_menu;
    GtkWidget *View_menu_refresh;
    GtkWidget *Go_menu;
    GtkWidget *Go_menu_today;
    GtkWidget *Go_menu_prev_day;
    GtkWidget *Go_menu_prev_week;
    GtkWidget *Go_menu_next_day;
    GtkWidget *Go_menu_next_week;

    GtkWidget *Toolbar;
    GtkWidget *Create_toolbutton;
    GtkWidget *Previous_week_toolbutton;
    GtkWidget *Previous_day_toolbutton;
    GtkWidget *Today_toolbutton;
    GtkWidget *Next_day_toolbutton;
    GtkWidget *Next_week_toolbutton;
    GtkWidget *Refresh_toolbutton;
    GtkWidget *Close_toolbutton;

    GtkWidget *StartDate_button;
    GtkRequisition StartDate_button_req;
    GtkWidget *day_spin;

    GtkWidget *scroll_win;
    GtkWidget *dtable;   /* day table */

    GtkWidget *header[MAX_DAYS];
    GtkWidget *element[24][MAX_DAYS];
    GtkWidget *line[24][MAX_DAYS];

    guint upd_timer;
    gdouble scroll_pos; /* remember the scroll position */

    GList *apptw_list; /* keep track of appointments being updated */

    GDateTime *a_day; /* start date */
    gint days;      /* how many days to show */
} day_win;

void refresh_day_win(day_win *dw);

#define ORAGE_WEEK_WINDOW_TYPE (orage_week_window_get_type ())
G_DECLARE_FINAL_TYPE (OrageWeekWindow, orage_week_window, ORAGE, WEEK_WINDOW,
                      GtkWindow)

/** Create new week window for specified start date.
 *  @param date window start date
 */
OrageWeekWindow *orage_week_window_new (GDateTime *date);

#endif
