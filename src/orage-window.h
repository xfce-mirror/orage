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

#ifndef ORAGE_WINDOW_H
#define ORAGE_WINDOW_H 1

#include "orage-application.h"
#include <glib.h>

G_BEGIN_DECLS

typedef struct _CalWin
{
    GtkAccelGroup *mAccel_group;

    GtkWidget *mWindow;
    GtkWidget *mVbox;

    GtkWidget *mMenubar;
    GtkWidget *mFile_menu;
    GtkWidget *mFile_newApp;
#ifdef ENABLE_SYNC
    GtkWidget *mFile_refresh;
#endif
    GtkWidget *mFile_interface;
    GtkWidget *mFile_close;
    GtkWidget *mFile_quit;
    GtkWidget *mEdit_menu;
    GtkWidget *mEdit_preferences;
    GtkWidget *mView_menu;
    GtkWidget *mView_ViewSelectedDate;
    GtkWidget *mView_ViewSelectedWeek;
    GtkWidget *mView_selectToday;
    GtkWidget *mHelp_menu;
    GtkWidget *mHelp_help;
    GtkWidget *mHelp_about;

    GtkWidget *mCalendar;

    GtkWidget *mTodo_vbox;
    GtkWidget *mTodo_label;
    GtkWidget *mTodo_scrolledWin;
    GtkWidget *mTodo_rows_vbox;
    GtkWidget *mEvent_vbox;
    GtkWidget *mEvent_label;
    GtkWidget *mEvent_scrolledWin;
    GtkWidget *mEvent_rows_vbox;
} CalWin;

#define ORAGE_TYPE_WINDOW (orage_window_get_type ())

G_DECLARE_FINAL_TYPE (OrageWindow, orage_window, ORAGE, WINDOW, GtkApplicationWindow)

void build_mainWin (void);
gboolean orage_mark_appointments (void);
void build_mainbox_info (void);
void build_mainbox_event_box (void);
void build_mainbox_todo_box (void);
void mCalendar_month_changed_cb(GtkCalendar *calendar, gpointer user_data);

/** Creates a new OrageWindow
 *  @return a newly created OrageWindow
 */
GtkWidget *orage_window_new (OrageApplication *application);

G_END_DECLS

#endif
