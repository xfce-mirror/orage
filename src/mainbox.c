/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2005-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2004-2005 Mickael Graf (korbinus at xfce.org)
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "orage-i18n.h"
#include "orage-css.h"
#include "functions.h"
#include "mainbox.h"
#include "about-xfcalendar.h"
#include "ical-code.h"
#include "event-list.h"
#include "appointment.h"
#include "interface.h"
#include "parameters.h"
#include "tray_icon.h"
#include "day-view.h"

static guint month_change_timer=0;

gboolean orage_mark_appointments(void)
{
    if (!xfical_file_open(TRUE))
        return(FALSE);
    xfical_mark_calendar(GTK_CALENDAR(((CalWin *)g_par.xfcal)->mCalendar));
    xfical_file_close(TRUE);
    return(TRUE);
}

static void mFile_newApp_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                      gpointer user_data)
{
    GDateTime *gdt;
    CalWin *cal = (CalWin *)user_data;
    gchar *cur_date;

    /* cal has always a day selected here, so it is safe to read it */
    gdt = orage_cal_to_gdatetime (GTK_CALENDAR (cal->mCalendar), 1, 1);
    cur_date = orage_gdatetime_to_icaltime (gdt, TRUE);
    create_appt_win("NEW", cur_date, gdt);
    g_date_time_unref (gdt);
    g_free (cur_date);
}

static void mFile_interface_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem
        , gpointer user_data)
{
    CalWin *cal = (CalWin *)user_data;

    orage_external_interface(cal);
}

static void mFile_close_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                     gpointer user_data)
{
    CalWin *cal = (CalWin *)user_data;

    if (g_par.close_means_quit)
        gtk_main_quit();
    else
        gtk_widget_hide(cal->mWindow);
}

static void mFile_quit_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                    G_GNUC_UNUSED gpointer user_data)
{
    gtk_main_quit();
}

static void mEdit_preferences_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                           G_GNUC_UNUSED gpointer user_data)
{
    show_parameters();
}

static void mView_ViewSelectedDate_activate_cb (
    G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
    (void)create_el_win(NULL);
}

static void mView_ViewSelectedWeek_activate_cb (
    G_GNUC_UNUSED GtkMenuItem *menuitem, gpointer user_data)
{
    CalWin *cal = (CalWin *)user_data;
    GDateTime *date;

    date = orage_cal_to_gdatetime (GTK_CALENDAR (cal->mCalendar), 1, 1);
    create_day_win (date);
    g_date_time_unref (date);
}

static void mView_selectToday_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem
        , gpointer user_data)
{
    CalWin *cal = (CalWin *)user_data;

    orage_select_today(GTK_CALENDAR(cal->mCalendar));
}

static void mView_StartGlobaltime_activate_cb (
    G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
    GError *error = NULL;

    if (!orage_exec("globaltime", NULL, &error))
        g_warning ("start of globaltime failed: %s", error->message);
}

static void mHelp_help_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                    G_GNUC_UNUSED gpointer user_data)
{
    const gchar *helpdoc;
    GError *error = NULL;

    helpdoc = "exo-open https://docs.xfce.org/apps/orage/start";
    if (!orage_exec(helpdoc, NULL, &error)) {
        g_message ("%s failed: %s. Trying firefox", helpdoc
                                    , error->message);
        g_clear_error(&error);
        helpdoc = "firefox https://docs.xfce.org/apps/orage/start";
        if (!orage_exec(helpdoc, NULL, &error)) {
            g_warning ("start of %s failed: %s", helpdoc, error->message);
            g_clear_error(&error);
        }
    }
}

static void mHelp_about_activate_cb(GtkMenuItem *menuitem, gpointer user_data)
{
    create_wAbout((GtkWidget *)menuitem, user_data);
}

static void mCalendar_day_selected_double_click_cb(GtkCalendar *calendar
        , G_GNUC_UNUSED gpointer user_data)
{
    GDateTime *date;

    if (g_par.show_days)
    {
        date = orage_cal_to_gdatetime (calendar, 1, 1);
        create_day_win (date);
        g_date_time_unref (date);
    }
    else
        (void)create_el_win(NULL);
}

static gboolean upd_calendar (G_GNUC_UNUSED GtkCalendar *calendar)
{
    orage_mark_appointments();
    month_change_timer = 0;

    return(FALSE); /* we do this only once */
}

void mCalendar_month_changed_cb (GtkCalendar *calendar,
                                 G_GNUC_UNUSED gpointer user_data)
{
    /* orage_mark_appointments is rather heavy (=slow), so doing
     * it here is not a good idea. We can't keep up with the autorepeat
     * speed if we do the whole thing here. Bug 2080 prooves it. So let's
     * throw it to background and do it later. We stop previously 
     * running updates since this new one will overwrite them anyway.
     * Let's clear still the view to fix bug 3913 (only needed 
     * if there are changes in the calendar) */
    if (month_change_timer) {
        g_source_remove(month_change_timer);
    }

    gtk_calendar_clear_marks(calendar);
    month_change_timer = g_timeout_add(400, (GSourceFunc)upd_calendar, calendar);
}

static void build_menu(void)
{
    CalWin *cal = (CalWin *)g_par.xfcal;

    cal->mMenubar = gtk_menu_bar_new();
    gtk_grid_attach_next_to (GTK_GRID(cal->mVbox), cal->mMenubar, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    /* File menu */
    cal->mFile_menu = orage_menu_new(_("_File"), cal->mMenubar);

    cal->mFile_newApp = orage_image_menu_item_new_from_stock("gtk-new"
            , cal->mFile_menu, cal->mAccel_group);

    (void)orage_separator_menu_item_new(cal->mFile_menu);

    cal->mFile_interface = 
            orage_menu_item_new_with_mnemonic(_("_Exchange data")
                    , cal->mFile_menu);

    (void)orage_separator_menu_item_new(cal->mFile_menu);

    cal->mFile_close = orage_image_menu_item_new_from_stock("gtk-close"
            , cal->mFile_menu, cal->mAccel_group);
    cal->mFile_quit = orage_image_menu_item_new_from_stock("gtk-quit"
            , cal->mFile_menu, cal->mAccel_group);

    /* Edit menu */
    cal->mEdit_menu = orage_menu_new(_("_Edit"), cal->mMenubar);

    cal->mEdit_preferences = 
            orage_image_menu_item_new_from_stock("gtk-preferences"
                    , cal->mEdit_menu, cal->mAccel_group);

    /* View menu */
    cal->mView_menu = orage_menu_new(_("_View"), cal->mMenubar);

    cal->mView_ViewSelectedDate = 
            orage_menu_item_new_with_mnemonic(_("View selected _date")
                    , cal->mView_menu);
    cal->mView_ViewSelectedWeek = 
            orage_menu_item_new_with_mnemonic(_("View selected _week")
                    , cal->mView_menu);

    (void)orage_separator_menu_item_new(cal->mView_menu);

    cal->mView_selectToday = 
            orage_menu_item_new_with_mnemonic(_("Select _Today")
                    , cal->mView_menu);

    (void)orage_separator_menu_item_new(cal->mView_menu);

    cal->mView_StartGlobaltime = 
            orage_menu_item_new_with_mnemonic(_("Show _Globaltime")
                    , cal->mView_menu);

    /* Help menu */
    cal->mHelp_menu = orage_menu_new(_("_Help"), cal->mMenubar);
    cal->mHelp_help = orage_image_menu_item_new_from_stock("gtk-help"
            , cal->mHelp_menu, cal->mAccel_group);
    cal->mHelp_about = orage_image_menu_item_new_from_stock("gtk-about"
            , cal->mHelp_menu, cal->mAccel_group);

    gtk_widget_show_all(((CalWin *)g_par.xfcal)->mMenubar);

    /* Signals */
    g_signal_connect((gpointer) cal->mFile_newApp, "activate"
            , G_CALLBACK(mFile_newApp_activate_cb),(gpointer) cal);
    g_signal_connect((gpointer) cal->mFile_interface, "activate"
            , G_CALLBACK(mFile_interface_activate_cb),(gpointer) cal);
    g_signal_connect((gpointer) cal->mFile_close, "activate"
            , G_CALLBACK(mFile_close_activate_cb),(gpointer) cal);
    g_signal_connect((gpointer) cal->mFile_quit, "activate"
            , G_CALLBACK(mFile_quit_activate_cb),(gpointer) cal);
    g_signal_connect((gpointer) cal->mEdit_preferences, "activate"
            , G_CALLBACK(mEdit_preferences_activate_cb), NULL);
    g_signal_connect((gpointer) cal->mView_ViewSelectedDate, "activate"
            , G_CALLBACK(mView_ViewSelectedDate_activate_cb),(gpointer) cal);
    g_signal_connect((gpointer) cal->mView_ViewSelectedWeek, "activate"
            , G_CALLBACK(mView_ViewSelectedWeek_activate_cb),(gpointer) cal);
    g_signal_connect((gpointer) cal->mView_selectToday, "activate"
            , G_CALLBACK(mView_selectToday_activate_cb),(gpointer) cal);
    g_signal_connect((gpointer) cal->mView_StartGlobaltime, "activate"
            , G_CALLBACK(mView_StartGlobaltime_activate_cb),(gpointer) cal);
    g_signal_connect((gpointer) cal->mHelp_help, "activate"
            , G_CALLBACK(mHelp_help_activate_cb), NULL);
    g_signal_connect((gpointer) cal->mHelp_about, "activate"
            , G_CALLBACK(mHelp_about_activate_cb),(gpointer) cal);
}

static void todo_clicked(GtkWidget *widget
        , GdkEventButton *event, G_GNUC_UNUSED gpointer *user_data)
{
    gchar *uid;
    GDateTime *gdt;

    if (event->type==GDK_2BUTTON_PRESS) {
        uid = g_object_get_data(G_OBJECT(widget), "UID");
        gdt = g_date_time_new_now_local ();
        create_appt_win("UPDATE", uid, gdt);
        g_date_time_unref (gdt);
    }
}

static void add_info_row(xfical_appt *appt, GtkGrid *parentBox,
                         const gboolean todo)
{
    GtkWidget *ev, *label;
    CalWin *cal = (CalWin *)g_par.xfcal;
    gchar *tip, *tmp, *tmp_title, *tmp_note;
    gchar *tip_title, *tip_location, *tip_note;
    gchar *format_bold = "<b> %s </b>";
    char  *s_time, *s_timeonly, *e_time, *c_time, *na;
    GDateTime *today;
    GDateTime *gdt_e_time;

    /***** add data into the vbox *****/
    ev = gtk_event_box_new();
    tmp_title = appt->title
            ? orage_process_text_commands(appt->title)
            : g_strdup(_("No title defined"));
    s_time = orage_gdatetime_to_i18_time (appt->starttimecur, appt->allDay);
    today = g_date_time_new_now_local ();
    if (todo) {
        e_time = appt->use_due_time ?
            orage_gdatetime_to_i18_time(appt->endtimecur, appt->allDay) : g_strdup (s_time);
        tmp = g_strdup_printf(" %s  %s", e_time, tmp_title);
        g_free(e_time);
    }
    else {
        s_timeonly = g_date_time_format (appt->starttimecur, "%R");
        if (orage_gdatetime_compare_date (today, appt->starttimecur) == 0)
            tmp = g_strdup_printf(" %s* %s", s_timeonly, tmp_title);
        else {
            if (g_par.show_event_days > 1)
                tmp = g_strdup_printf(" %s  %s", s_time, tmp_title);
            else
                tmp = g_strdup_printf(" %s  %s", s_timeonly, tmp_title);
        }
        g_free(s_timeonly);
    }
    label = gtk_label_new(tmp);

    g_free(tmp);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    g_object_set (label, "xalign", 0.0, "yalign", 0.5,
                         "xpad", 5, "ypad", 0,
                         "hexpand", TRUE,
                         "halign", GTK_ALIGN_FILL,
                         NULL);
    gtk_container_add(GTK_CONTAINER(ev), label);
    gtk_grid_attach_next_to (parentBox, ev, NULL, GTK_POS_BOTTOM, 1, 1);
    g_object_set_data_full(G_OBJECT(ev), "UID", g_strdup(appt->uid), g_free);
    g_signal_connect((gpointer)ev, "button-press-event"
            , G_CALLBACK(todo_clicked), cal);

    /***** set color *****/
    if (todo) {
        if (appt->use_due_time)
            gdt_e_time = g_date_time_ref (appt->endtimecur);
        else
            gdt_e_time = g_date_time_new_local (9999, 12, 31, 23, 59, 59);

        if (g_date_time_compare (gdt_e_time, today) < 0) /* gone */
            gtk_widget_set_name (label, ORAGE_MAINBOX_RED);
        else if (g_date_time_compare (appt->starttimecur, today) <= 0
             &&  g_date_time_compare (gdt_e_time, today) >= 0)
        {
            gtk_widget_set_name (label, ORAGE_MAINBOX_BLUE);
        }

        g_date_time_unref (gdt_e_time);
    }

    /***** set tooltip hint *****/
    tip_title = g_markup_printf_escaped(format_bold, tmp_title);
    if (appt->location) {
        tmp = g_markup_printf_escaped(format_bold, appt->location);
        tip_location = g_strdup_printf(_(" Location: %s\n"), tmp);
        g_free(tmp);
    }
    else {
        tip_location = g_strdup("");
    }
    if (appt->note) {
        tmp_note = orage_process_text_commands(appt->note);
        tmp_note = orage_limit_text(tmp_note, 50, 10);
        tmp = g_markup_escape_text(tmp_note, strlen(tmp_note));
        tip_note = g_strdup_printf(_("\n Note:\n%s"), tmp);
        g_free(tmp);
    }
    else {
        tip_note = g_strdup("");
    }

    if (todo) {
        na = _("Never");
        e_time = appt->use_due_time ?
                 orage_gdatetime_to_i18_time (appt->endtimecur, appt->allDay)
                 : g_strdup (na);
        c_time = appt->completed && appt->completedtime ?
                 orage_gdatetime_to_i18_time (appt->completedtime, appt->allDay)
                 : g_strdup (na);

        tip = g_strdup_printf(_("Title: %s\n%s Start:\t%s\n Due:\t%s\n Done:\t%s%s")
                , tip_title, tip_location, s_time, e_time, c_time, tip_note);

        g_free(c_time);
    }
    else { /* it is event */
        e_time = orage_gdatetime_to_i18_time (appt->endtimecur, appt->allDay);
        tip = g_strdup_printf(_("Title: %s\n%s Start:\t%s\n End:\t%s%s")
                , tip_title, tip_location, s_time, e_time, tip_note);
    }

    gtk_widget_set_tooltip_markup(ev, tip);

    g_date_time_unref (today);
    g_free(tip_title);
    g_free(tip_location);
    g_free(tip_note);
    g_free(tmp_title);
    g_free(s_time);
    g_free(e_time);
    g_free(tip);
}

static void insert_rows (GList **list, GDateTime *gdt, xfical_type ical_type
        , gchar *file_type)
{
    xfical_appt *appt;
    gchar *a_day;

    a_day = orage_gdatetime_to_icaltime (gdt, TRUE);

    for (appt = xfical_appt_get_next_on_day(a_day, TRUE, 0
                , ical_type , file_type);
         appt;
         appt = xfical_appt_get_next_on_day(a_day, FALSE, 0
                , ical_type , file_type)) {
        *list = g_list_prepend(*list, appt);
    }

    g_free (a_day);
}

static gint event_order(gconstpointer a, gconstpointer b)
{
    xfical_appt *appt1, *appt2;

    appt1 = (xfical_appt *)a;
    appt2 = (xfical_appt *)b;

    return g_date_time_compare (appt1->starttimecur, appt2->starttimecur);
}

static gint todo_order(gconstpointer a, gconstpointer b)
{
    xfical_appt *appt1, *appt2;

    appt1 = (xfical_appt *)a;
    appt2 = (xfical_appt *)b;

    if (appt1->use_due_time && !appt2->use_due_time)
        return(-1);
    if (!appt1->use_due_time && appt2->use_due_time)
        return(1);

    return g_date_time_compare (appt1->endtimecur, appt2->endtimecur);
}

static void info_process(gpointer a, gpointer pbox)
{
    xfical_appt *appt = (xfical_appt *)a;
    GtkGrid *box= GTK_GRID (pbox);
    CalWin *cal = (CalWin *)g_par.xfcal;
    gboolean todo;

    todo = (pbox == cal->mTodo_rows_vbox) ? TRUE : FALSE;
    if (appt->priority < g_par.priority_list_limit)
        add_info_row(appt, box, todo);
    xfical_appt_free(appt);
}

static void create_mainbox_todo_info(void)
{
    CalWin *cal = (CalWin *)g_par.xfcal;

    cal->mTodo_vbox = gtk_grid_new ();
    g_object_set (cal->mTodo_vbox, "vexpand", TRUE,
                                   "valign", GTK_ALIGN_FILL,
                                   NULL);
    gtk_grid_attach_next_to (GTK_GRID(cal->mVbox), cal->mTodo_vbox, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    cal->mTodo_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(cal->mTodo_label), _("<b>To do:</b>"));
    gtk_grid_attach_next_to (GTK_GRID(cal->mTodo_vbox), cal->mTodo_label,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    g_object_set (cal->mTodo_label, "xalign", 0.0, "yalign", 0.5, NULL);
    cal->mTodo_scrolledWin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cal->mTodo_scrolledWin)
            , GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(
            cal->mTodo_scrolledWin), GTK_SHADOW_NONE);
    g_object_set (cal->mTodo_scrolledWin, "vexpand", TRUE, NULL);
    gtk_grid_attach_next_to (GTK_GRID(cal->mTodo_vbox), cal->mTodo_scrolledWin,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    cal->mTodo_rows_vbox = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER (cal->mTodo_scrolledWin),
                                      cal->mTodo_rows_vbox);
}

static void create_mainbox_event_info_box(void)
{
    CalWin *cal = (CalWin *)g_par.xfcal;
    gchar *tmp, *tmp2, *tmp3;
    GDateTime *gdt;
    GDateTime *gdt_tmp;

    gdt = orage_cal_to_gdatetime (GTK_CALENDAR (cal->mCalendar), 1, 1);

    cal->mEvent_vbox = gtk_grid_new ();
    g_object_set (cal->mEvent_vbox, "vexpand", TRUE,
                                    "valign", GTK_ALIGN_FILL,
                                    NULL);
    gtk_grid_attach_next_to (GTK_GRID(cal->mVbox), cal->mEvent_vbox, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    cal->mEvent_label = gtk_label_new(NULL);
    if (g_par.show_event_days) {
    /* bug 7836: we call this routine also with 0 = no event data at all */
        if (g_par.show_event_days == 1) {
            tmp2 = g_date_time_format (gdt, "%x");
            tmp = g_strdup_printf(_("<b>Events for %s:</b>"), tmp2);
            g_free(tmp2);
        }
        else {
            tmp2 = g_date_time_format (gdt, "%x");

            gdt_tmp = gdt;
            gdt = g_date_time_add_days (gdt_tmp, g_par.show_event_days - 1);
            g_date_time_unref (gdt_tmp);
            tmp3 = g_date_time_format (gdt, "%x");
            tmp = g_strdup_printf(_("<b>Events for %s - %s:</b>"), tmp2, tmp3);
            g_free(tmp2);
            g_free(tmp3);
        }
        gtk_label_set_markup(GTK_LABEL(cal->mEvent_label), tmp);
        g_free(tmp);
    }

    g_date_time_unref (gdt);
    g_object_set (cal->mEvent_label, "xalign", 0.0, "yalign", 0.5, NULL);
    gtk_grid_attach_next_to (GTK_GRID(cal->mEvent_vbox), cal->mEvent_label,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    cal->mEvent_scrolledWin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cal->mEvent_scrolledWin)
            , GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(
            cal->mEvent_scrolledWin), GTK_SHADOW_NONE);
    g_object_set (cal->mEvent_scrolledWin, "expand", TRUE, NULL);
    gtk_grid_attach_next_to (GTK_GRID(cal->mEvent_vbox),
                             cal->mEvent_scrolledWin, NULL, GTK_POS_BOTTOM,
                             1, 1);
    cal->mEvent_rows_vbox = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER (cal->mEvent_scrolledWin),
                                      cal->mEvent_rows_vbox);
}

static void build_mainbox_todo_info(void)
{
    CalWin *cal = (CalWin *)g_par.xfcal;
    GDateTime *gdt;
    xfical_type ical_type;
    gchar file_type[8];
    gint i;
    GList *todo_list=NULL;

    if (g_par.show_todos) {
        gdt = g_date_time_new_now_local ();
        ical_type = XFICAL_TYPE_TODO;
        /* first search base orage file */
        g_strlcpy (file_type, "O00.", sizeof (file_type));
        insert_rows(&todo_list, gdt, ical_type, file_type);
        /* then process all foreign files */
        for (i = 0; i < g_par.foreign_count; i++) {
            g_snprintf(file_type, sizeof (file_type), "F%02d.", i);
            insert_rows(&todo_list, gdt, ical_type, file_type);
        }

        g_date_time_unref (gdt);
    }
    if (todo_list) {
        gtk_widget_destroy(cal->mTodo_vbox);
        create_mainbox_todo_info();
        todo_list = g_list_sort(todo_list, todo_order);
        g_list_foreach(todo_list, (GFunc)info_process
                , cal->mTodo_rows_vbox);
        g_list_free(todo_list);
        todo_list = NULL;
        gtk_widget_show_all(cal->mTodo_vbox);
    }
    else {
        gtk_widget_hide(cal->mTodo_vbox);
    }
}

static void build_mainbox_event_info(void)
{
    CalWin *cal = (CalWin *)g_par.xfcal;
    xfical_type ical_type;
    gchar file_type[8];
    gint i;
    GList *event_list=NULL;
    GDateTime *gdt;

    if (g_par.show_event_days) {
        gdt = orage_cal_to_gdatetime (GTK_CALENDAR (cal->mCalendar), 1, 1);
        ical_type = XFICAL_TYPE_EVENT;
        g_strlcpy (file_type, "O00.", sizeof (file_type));
#if 0
        insert_rows(&event_list, gdt, ical_type, file_type);
#endif
        xfical_get_each_app_within_time (gdt, g_par.show_event_days
                , ical_type, file_type, &event_list);
        for (i = 0; i < g_par.foreign_count; i++) {
            g_snprintf(file_type, sizeof (file_type), "F%02d.", i);
#if 0
            insert_rows(&event_list, gdt, ical_type, file_type);
#endif
            xfical_get_each_app_within_time (gdt, g_par.show_event_days
                    , ical_type, file_type, &event_list);
        }

        g_date_time_unref (gdt);
    }
    if (event_list) {
        gtk_widget_destroy(cal->mEvent_vbox);
        create_mainbox_event_info_box();
        event_list = g_list_sort(event_list, event_order);
        g_list_foreach(event_list, (GFunc)info_process
                , cal->mEvent_rows_vbox);
        g_list_free(event_list);
        event_list = NULL;
        gtk_widget_show_all(cal->mEvent_vbox);
    }
    else {
        gtk_widget_hide(cal->mEvent_vbox);
    }
}

static void mCalendar_day_selected_cb (G_GNUC_UNUSED GtkCalendar *calendar,
                                       G_GNUC_UNUSED gpointer user_data)
{
    /* rebuild the info for the selected date */
    build_mainbox_event_box();
}

void build_mainbox_event_box(void)
{
    if (!xfical_file_open(TRUE))
        return;
    build_mainbox_event_info();
    xfical_file_close(TRUE);   
}

void build_mainbox_todo_box(void)
{
    if (!xfical_file_open(TRUE))
        return;
    build_mainbox_todo_info();
    xfical_file_close(TRUE);   
}

/**********************************************************************
 * This routine is called from ical-code xfical_alarm_build_list_internal
 * and ical files are already open at that time. So make sure ical files
 * are opened before and closed after this call.
 **********************************************************************/
void build_mainbox_info(void)
{
    build_mainbox_todo_info();
    build_mainbox_event_info();
}

void build_mainWin(void)
{
    CalWin *cal = (CalWin *)g_par.xfcal;

    cal->mAccel_group = gtk_accel_group_new();

    gtk_window_set_title(GTK_WINDOW(cal->mWindow), _("Orage"));
    gtk_window_set_position(GTK_WINDOW(cal->mWindow), GTK_WIN_POS_NONE);
    gtk_window_set_resizable(GTK_WINDOW(cal->mWindow), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(cal->mWindow), TRUE);

    /* Build the vertical box */
    cal->mVbox = gtk_grid_new ();
    gtk_container_add(GTK_CONTAINER(cal->mWindow), cal->mVbox);
    gtk_widget_show(cal->mVbox);

    /* Build the menu */
    build_menu();

    /* Build the calendar */
    cal->mCalendar = gtk_calendar_new();
    g_object_set (cal->mCalendar, "hexpand", TRUE,
                                  "halign", GTK_ALIGN_FILL,
                                  NULL);
    gtk_grid_attach_next_to (GTK_GRID(cal->mVbox), cal->mCalendar, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    /*
    gtk_calendar_set_display_options(GTK_CALENDAR(cal->mCalendar)
            , GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES
            | GTK_CALENDAR_SHOW_WEEK_NUMBERS);
    */
    gtk_widget_show(cal->mCalendar);

    /* Build the Info boxes */
    create_mainbox_todo_info();
    create_mainbox_event_info_box();

    /* Signals */
    g_signal_connect((gpointer) cal->mCalendar, "day_selected_double_click"
            , G_CALLBACK(mCalendar_day_selected_double_click_cb)
            , (gpointer) cal);
    g_signal_connect((gpointer) cal->mCalendar, "day_selected"
            , G_CALLBACK(mCalendar_day_selected_cb)
            , (gpointer) cal);
    g_signal_connect((gpointer) cal->mCalendar, "month-changed"
            , G_CALLBACK(mCalendar_month_changed_cb), (gpointer) cal);

    gtk_window_add_accel_group(GTK_WINDOW(cal->mWindow), cal->mAccel_group);

    if (g_par.size_x || g_par.size_y)
        gtk_window_set_default_size(GTK_WINDOW(cal->mWindow)
                , g_par.size_x, g_par.size_y);
    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move(GTK_WINDOW(cal->mWindow), g_par.pos_x, g_par.pos_y);
    /*
    gtk_window_stick(GTK_WINDOW(cal->mWindow));
    */
}
