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

#define _XOPEN_SOURCE /* glibc2 needs this */
#define _XOPEN_SOURCE_EXTENDED 1 /* strptime needs this in posix systems */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include "orage-i18n.h"
#include "functions.h"
#include "mainbox.h"
#include "ical-code.h"
#include "timezone_selection.h"
#include "event-list.h"
#include "day-view.h"
#include "appointment.h"
#include "parameters.h"
#include "reminder.h"

/* FIXME: Remove USE_GLIB_258 after switching required GLib to >= 2.58. */
#define USE_GLIB_258 0
#define BORDER_SIZE 20
#define FILETYPE_SIZE 38

#define ORAGE_RC_COLOUR "Color"
#define CATEGORIES_SPACING 10

typedef struct _orage_category_win
{
    GtkWidget *window;
    GtkWidget *vbox;

    GtkWidget *new_frame;
    GtkWidget *new_entry;
    GtkWidget *new_color_button;
    GtkWidget *new_add_button;

    GtkWidget *cur_frame;
    GtkWidget *cur_frame_vbox;

    GtkAccelGroup *accelgroup;

    gpointer *apptw;
} category_win_struct;

static void refresh_categories(category_win_struct *catw);
static void read_default_alarm(xfical_appt *appt);

/*  
 *  these are the main functions in this file:
 */ 
static gboolean fill_appt_window(appt_win *apptw, const gchar *action,
                                 const gchar *par, GDateTime *gdt_par);
static gboolean fill_appt_from_apptw(xfical_appt *appt, appt_win *apptw);

static GtkWidget *datetime_hbox_new(GtkWidget *date_button
        , GtkWidget *spin_hh, GtkWidget *spin_mm
        , GtkWidget *timezone_button)
{
    GtkWidget *grid, *space_label;

    grid = gtk_grid_new ();

    gtk_grid_attach (GTK_GRID (grid), date_button, 0, 0, 1, 1);

    space_label = gtk_label_new("  ");
    gtk_grid_attach_next_to (GTK_GRID (grid), space_label, NULL,
                             GTK_POS_RIGHT, 1, 1);

    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_hh), TRUE);
    gtk_grid_attach_next_to (GTK_GRID (grid), spin_hh, NULL,
                             GTK_POS_RIGHT, 1, 1);

    space_label = gtk_label_new(":");
    gtk_grid_attach_next_to (GTK_GRID (grid), space_label, NULL,
                             GTK_POS_RIGHT, 1, 1);

    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_mm), TRUE);
    gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_mm), 5, 10);
    gtk_grid_attach_next_to (GTK_GRID (grid), spin_mm, NULL,
                             GTK_POS_RIGHT, 1, 1);

    if (timezone_button) {
        space_label = gtk_label_new("  ");
        g_object_set (timezone_button, "hexpand", TRUE,
                                       "halign", GTK_ALIGN_FILL, NULL);
        gtk_grid_attach_next_to (GTK_GRID (grid), space_label, NULL,
                                 GTK_POS_RIGHT, 1, 1);
        gtk_grid_attach_next_to (GTK_GRID (grid), timezone_button, NULL,
                                 GTK_POS_RIGHT, 1, 1);
    }

    return grid;
}

static void mark_appointment_changed(appt_win *apptw)
{
    if (!apptw->appointment_changed) {
        apptw->appointment_changed = TRUE;
        gtk_widget_set_sensitive(apptw->Revert, TRUE);
        gtk_widget_set_sensitive(apptw->File_menu_revert, TRUE);
    }
}

static void mark_appointment_unchanged(appt_win *apptw)
{
    if (apptw->appointment_changed) {
        apptw->appointment_changed = FALSE;
        gtk_widget_set_sensitive(apptw->Revert, FALSE);
        gtk_widget_set_sensitive(apptw->File_menu_revert, FALSE);
    }
}

static void set_time_sensitivity(appt_win *apptw)
{
    gboolean dur_act, allDay_act, comp_act, due_act;

    dur_act = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Dur_checkbutton));
    allDay_act = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->AllDay_checkbutton));
    comp_act = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Completed_checkbutton));
        /* todo can be without due date, but event always has end date.
         * Journal has none, but we show it similarly than event */
    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Type_todo_rb)))
        due_act = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->End_checkbutton));
    else 
        due_act = TRUE;

    gtk_widget_set_sensitive(apptw->Dur_checkbutton, due_act);
    if (allDay_act) {
        gtk_widget_set_sensitive(apptw->StartTime_spin_hh, FALSE);
        gtk_widget_set_sensitive(apptw->StartTime_spin_mm, FALSE);
        gtk_widget_set_sensitive(apptw->StartTimezone_button, FALSE);
        gtk_widget_set_sensitive(apptw->EndTime_spin_hh, FALSE);
        gtk_widget_set_sensitive(apptw->EndTime_spin_mm, FALSE);
        gtk_widget_set_sensitive(apptw->EndTimezone_button, FALSE);
        gtk_widget_set_sensitive(apptw->Dur_spin_hh, FALSE);
        gtk_widget_set_sensitive(apptw->Dur_spin_hh_label, FALSE);
        gtk_widget_set_sensitive(apptw->Dur_spin_mm, FALSE);
        gtk_widget_set_sensitive(apptw->Dur_spin_mm_label, FALSE);
        if (dur_act) {
            gtk_widget_set_sensitive(apptw->EndDate_button, FALSE);
            gtk_widget_set_sensitive(apptw->Dur_spin_dd, due_act);
            gtk_widget_set_sensitive(apptw->Dur_spin_dd_label, due_act);
        }
        else {
            gtk_widget_set_sensitive(apptw->EndDate_button, due_act);
            gtk_widget_set_sensitive(apptw->Dur_spin_dd, FALSE);
            gtk_widget_set_sensitive(apptw->Dur_spin_dd_label, FALSE);
        }
    }
    else {
        gtk_widget_set_sensitive(apptw->StartTime_spin_hh, TRUE);
        gtk_widget_set_sensitive(apptw->StartTime_spin_mm, TRUE);
        gtk_widget_set_sensitive(apptw->StartTimezone_button, TRUE);
        if (dur_act) {
            gtk_widget_set_sensitive(apptw->EndDate_button, FALSE);
            gtk_widget_set_sensitive(apptw->EndTime_spin_hh, FALSE);
            gtk_widget_set_sensitive(apptw->EndTime_spin_mm, FALSE);
            gtk_widget_set_sensitive(apptw->EndTimezone_button, FALSE);
            gtk_widget_set_sensitive(apptw->Dur_spin_dd, due_act);
            gtk_widget_set_sensitive(apptw->Dur_spin_dd_label, due_act);
            gtk_widget_set_sensitive(apptw->Dur_spin_hh, due_act);
            gtk_widget_set_sensitive(apptw->Dur_spin_hh_label, due_act);
            gtk_widget_set_sensitive(apptw->Dur_spin_mm, due_act);
            gtk_widget_set_sensitive(apptw->Dur_spin_mm_label, due_act);
        }
        else {
            gtk_widget_set_sensitive(apptw->EndDate_button, due_act);
            gtk_widget_set_sensitive(apptw->EndTime_spin_hh, due_act);
            gtk_widget_set_sensitive(apptw->EndTime_spin_mm, due_act);
            gtk_widget_set_sensitive(apptw->EndTimezone_button, due_act);
            gtk_widget_set_sensitive(apptw->Dur_spin_dd, FALSE);
            gtk_widget_set_sensitive(apptw->Dur_spin_dd_label, FALSE);
            gtk_widget_set_sensitive(apptw->Dur_spin_hh, FALSE);
            gtk_widget_set_sensitive(apptw->Dur_spin_hh_label, FALSE);
            gtk_widget_set_sensitive(apptw->Dur_spin_mm, FALSE);
            gtk_widget_set_sensitive(apptw->Dur_spin_mm_label, FALSE);
        }
    }
    gtk_widget_set_sensitive(apptw->CompletedDate_button, comp_act);
    gtk_widget_set_sensitive(apptw->CompletedTime_spin_hh, comp_act);
    gtk_widget_set_sensitive(apptw->CompletedTime_spin_mm, comp_act);
    gtk_widget_set_sensitive(apptw->CompletedTimezone_button, comp_act);
}

static void set_repeat_sensitivity(appt_win *apptw)
{
    gint freq, i;
    
    freq = gtk_combo_box_get_active(GTK_COMBO_BOX(apptw->Recur_freq_cb));
    if (freq == XFICAL_FREQ_NONE) {
        gtk_widget_set_sensitive(apptw->Recur_limit_rb, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_count_rb, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_count_spin, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_count_label, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_until_rb, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_until_button, FALSE);
        for (i=0; i <= 6; i++) {
            gtk_widget_set_sensitive(apptw->Recur_byday_cb[i], FALSE);
            gtk_widget_set_sensitive(apptw->Recur_byday_spin[i], FALSE);
        }
        gtk_widget_set_sensitive(apptw->Recur_int_spin, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_int_spin_label1, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_int_spin_label2, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_todo_base_hbox, FALSE);
        /*
        gtk_widget_set_sensitive(apptw->Recur_exception_hbox, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_calendar_hbox, FALSE);
        */
    }
    else {
        gtk_widget_set_sensitive(apptw->Recur_limit_rb, TRUE);
        gtk_widget_set_sensitive(apptw->Recur_count_rb, TRUE);
        if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_count_rb))) {
            gtk_widget_set_sensitive(apptw->Recur_count_spin, TRUE);
            gtk_widget_set_sensitive(apptw->Recur_count_label, TRUE);
        }
        else {
            gtk_widget_set_sensitive(apptw->Recur_count_spin, FALSE);
            gtk_widget_set_sensitive(apptw->Recur_count_label, FALSE);
        }
        gtk_widget_set_sensitive(apptw->Recur_until_rb, TRUE);
        if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_until_rb))) {
            gtk_widget_set_sensitive(apptw->Recur_until_button, TRUE);
        }
        else {
            gtk_widget_set_sensitive(apptw->Recur_until_button, FALSE);
        }
        for (i=0; i <= 6; i++) {
            gtk_widget_set_sensitive(apptw->Recur_byday_cb[i], TRUE);
        }
        if (freq == XFICAL_FREQ_MONTHLY || freq == XFICAL_FREQ_YEARLY) {
            for (i=0; i <= 6; i++) {
                gtk_widget_set_sensitive(apptw->Recur_byday_spin[i], TRUE);
            }
        }
        else {
            for (i=0; i <= 6; i++) {
                gtk_widget_set_sensitive(apptw->Recur_byday_spin[i], FALSE);
            }
        }
        gtk_widget_set_sensitive(apptw->Recur_int_spin, TRUE);
        gtk_widget_set_sensitive(apptw->Recur_int_spin_label1, TRUE);
        gtk_widget_set_sensitive(apptw->Recur_int_spin_label2, TRUE);
        gtk_widget_set_sensitive(apptw->Recur_todo_base_hbox, TRUE);
        /*
        gtk_widget_set_sensitive(apptw->Recur_exception_hbox, TRUE);
        gtk_widget_set_sensitive(apptw->Recur_calendar_hbox, TRUE);
        */
    }
}

static void recur_hide_show(appt_win *apptw)
{
    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Recur_feature_normal_rb))) {
        gtk_widget_hide(apptw->Recur_byday_label);
        gtk_widget_hide(apptw->Recur_byday_hbox);
        gtk_widget_hide(apptw->Recur_byday_spin_label);
        gtk_widget_hide(apptw->Recur_byday_spin_hbox);
    }
    else {
        gtk_widget_show(apptw->Recur_byday_label);
        gtk_widget_show(apptw->Recur_byday_hbox);
        gtk_widget_show(apptw->Recur_byday_spin_label);
        gtk_widget_show(apptw->Recur_byday_spin_hbox);
    }
}

static void type_hide_show(appt_win *apptw)
{
    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Type_event_rb))) {
        gtk_label_set_text(GTK_LABEL(apptw->End_label), _("End"));
        gtk_widget_show(apptw->Availability_label);
        gtk_widget_show(apptw->Availability_cb);
        gtk_widget_hide(apptw->End_checkbutton);
        gtk_widget_hide(apptw->Completed_label);
        gtk_widget_hide(apptw->Completed_hbox);
        gtk_widget_set_sensitive(apptw->Alarm_notebook_page, TRUE);
        gtk_widget_set_sensitive(apptw->Alarm_tab_label, TRUE);
        gtk_widget_set_sensitive(apptw->Recur_notebook_page, TRUE);
        gtk_widget_set_sensitive(apptw->Recur_tab_label, TRUE);
        gtk_widget_set_sensitive(apptw->Location_label, TRUE);
        gtk_widget_set_sensitive(apptw->Location_entry, TRUE);
        gtk_widget_set_sensitive(apptw->End_label, TRUE);
        gtk_widget_set_sensitive(apptw->EndTime_hbox, TRUE);
        gtk_widget_set_sensitive(apptw->Dur_hbox, TRUE);
        gtk_widget_hide(apptw->Recur_todo_base_label);
        gtk_widget_hide(apptw->Recur_todo_base_hbox);
    }
    else if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Type_todo_rb))) {
        gtk_label_set_text(GTK_LABEL(apptw->End_label), _("Due"));
        gtk_widget_hide(apptw->Availability_label);
        gtk_widget_hide(apptw->Availability_cb);
        gtk_widget_show(apptw->End_checkbutton);
        gtk_widget_show(apptw->Completed_label);
        gtk_widget_show(apptw->Completed_hbox);
        gtk_widget_set_sensitive(apptw->Alarm_notebook_page, TRUE);
        gtk_widget_set_sensitive(apptw->Alarm_tab_label, TRUE);
        gtk_widget_set_sensitive(apptw->Recur_notebook_page, TRUE);
        gtk_widget_set_sensitive(apptw->Recur_tab_label, TRUE);
        gtk_widget_set_sensitive(apptw->Location_label, TRUE);
        gtk_widget_set_sensitive(apptw->Location_entry, TRUE);
        gtk_widget_set_sensitive(apptw->End_label, TRUE);
        gtk_widget_set_sensitive(apptw->EndTime_hbox, TRUE);
        gtk_widget_set_sensitive(apptw->Dur_hbox, TRUE);
        gtk_widget_show(apptw->Recur_todo_base_label);
        gtk_widget_show(apptw->Recur_todo_base_hbox);
    }
    else /* if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Type_journal_rb))) */ {
        gtk_label_set_text(GTK_LABEL(apptw->End_label), _("End"));
        gtk_widget_hide(apptw->Availability_label);
        gtk_widget_hide(apptw->Availability_cb);
        gtk_widget_hide(apptw->End_checkbutton);
        gtk_widget_hide(apptw->Completed_label);
        gtk_widget_hide(apptw->Completed_hbox);
        gtk_widget_set_sensitive(apptw->Alarm_notebook_page, FALSE);
        gtk_widget_set_sensitive(apptw->Alarm_tab_label, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_notebook_page, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_tab_label, FALSE);
        gtk_widget_set_sensitive(apptw->Location_label, FALSE);
        gtk_widget_set_sensitive(apptw->Location_entry, FALSE);
        gtk_widget_set_sensitive(apptw->End_label, FALSE);
        gtk_widget_set_sensitive(apptw->EndTime_hbox, FALSE);
        gtk_widget_set_sensitive(apptw->Dur_hbox, FALSE);
        gtk_widget_hide(apptw->Recur_todo_base_label);
        gtk_widget_hide(apptw->Recur_todo_base_hbox);
    }
    set_time_sensitivity(apptw); /* todo has different settings */
}

static void readonly_hide_show(appt_win *apptw)
{
    if (((xfical_appt *)apptw->xf_appt)->readonly) {
        gtk_widget_set_sensitive(apptw->General_notebook_page, FALSE);
        gtk_widget_set_sensitive(apptw->General_tab_label, FALSE);
        gtk_widget_set_sensitive(apptw->Alarm_notebook_page, FALSE);
        gtk_widget_set_sensitive(apptw->Alarm_tab_label, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_notebook_page, FALSE);
        gtk_widget_set_sensitive(apptw->Recur_tab_label, FALSE);

        gtk_widget_set_sensitive(apptw->Save, FALSE);
        gtk_widget_set_sensitive(apptw->File_menu_save, FALSE);
        gtk_widget_set_sensitive(apptw->SaveClose, FALSE);
        gtk_widget_set_sensitive(apptw->File_menu_saveclose, FALSE);
        gtk_widget_set_sensitive(apptw->Revert, FALSE);
        gtk_widget_set_sensitive(apptw->File_menu_revert, FALSE);
        gtk_widget_set_sensitive(apptw->Delete, FALSE);
        gtk_widget_set_sensitive(apptw->File_menu_delete, FALSE);
    }
}

static void set_sound_sensitivity(appt_win *apptw)
{
    gboolean sound_act, repeat_act;

    sound_act = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(apptw->Sound_checkbutton));
    repeat_act = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(apptw->SoundRepeat_checkbutton));

    if (sound_act) {
        gtk_widget_set_sensitive(apptw->Sound_entry, TRUE);
        gtk_widget_set_sensitive(apptw->Sound_button, TRUE);
        gtk_widget_set_sensitive(apptw->SoundRepeat_hbox, TRUE);
        gtk_widget_set_sensitive(apptw->SoundRepeat_checkbutton, TRUE);
        if (repeat_act) {
            gtk_widget_set_sensitive(apptw->SoundRepeat_spin_cnt, TRUE);
            gtk_widget_set_sensitive(apptw->SoundRepeat_spin_cnt_label, TRUE);
            gtk_widget_set_sensitive(apptw->SoundRepeat_spin_len, TRUE);
            gtk_widget_set_sensitive(apptw->SoundRepeat_spin_len_label, TRUE);
        }
        else {
            gtk_widget_set_sensitive(apptw->SoundRepeat_spin_cnt, FALSE);
            gtk_widget_set_sensitive(apptw->SoundRepeat_spin_cnt_label, FALSE);
            gtk_widget_set_sensitive(apptw->SoundRepeat_spin_len, FALSE);
            gtk_widget_set_sensitive(apptw->SoundRepeat_spin_len_label, FALSE);
        }
    }
    else {
        gtk_widget_set_sensitive(apptw->Sound_entry, FALSE);
        gtk_widget_set_sensitive(apptw->Sound_button, FALSE);
        gtk_widget_set_sensitive(apptw->SoundRepeat_hbox, FALSE); /* enough */
    }
}

static void set_notify_sensitivity(appt_win *apptw)
{
#ifdef HAVE_NOTIFY
    gboolean notify_act, expire_act;

    notify_act = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(apptw->Display_checkbutton_notify));
    expire_act = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(apptw->Display_checkbutton_expire_notify));

    if (notify_act) {
        gtk_widget_set_sensitive(apptw->Display_checkbutton_expire_notify
                , TRUE);
        if (expire_act) {
            gtk_widget_set_sensitive(apptw->Display_spin_expire_notify, TRUE);
            gtk_widget_set_sensitive(apptw->Display_spin_expire_notify_label
                    , TRUE);
        }
        else {
            gtk_widget_set_sensitive(apptw->Display_spin_expire_notify, FALSE);
            gtk_widget_set_sensitive(apptw->Display_spin_expire_notify_label
                    , FALSE);
        }
    }
    else {
        gtk_widget_set_sensitive(apptw->Display_checkbutton_expire_notify
                , FALSE);
        gtk_widget_set_sensitive(apptw->Display_spin_expire_notify, FALSE);
        gtk_widget_set_sensitive(apptw->Display_spin_expire_notify_label
                , FALSE);
    }
#endif
}

static void set_proc_sensitivity(appt_win *apptw)
{
    gboolean proc_act;

    proc_act = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(apptw->Proc_checkbutton));

    if (proc_act) {
        gtk_widget_set_sensitive(apptw->Proc_entry, TRUE);
    }
    else {
        gtk_widget_set_sensitive(apptw->Proc_entry, FALSE);
    }
}

static void app_type_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb
        , gpointer user_data)
{
    type_hide_show((appt_win *)user_data);
    mark_appointment_changed((appt_win *)user_data);
}

static void on_appTitle_entry_changed_cb (G_GNUC_UNUSED GtkEditable *entry
        , gpointer user_data)
{
    gchar *title, *application_name;
    const gchar *appointment_name;
    appt_win *apptw = (appt_win *)user_data;

    appointment_name = gtk_entry_get_text((GtkEntry *)apptw->Title_entry);
    application_name = _("Orage");

    if (ORAGE_STR_EXISTS(appointment_name))
        title = g_strdup_printf("%s - %s", appointment_name, application_name);
    else
        title = g_strdup_printf("%s", application_name);

    gtk_window_set_title(GTK_WINDOW(apptw->Window), (const gchar *)title);

    g_free(title);
    mark_appointment_changed((appt_win *)user_data);
}

static void app_time_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb
        , gpointer user_data)
{
    set_time_sensitivity((appt_win *)user_data);
    mark_appointment_changed((appt_win *)user_data);
}

static void refresh_recur_calendars(appt_win *apptw)
{
    xfical_appt *appt;

    appt = (xfical_appt *)apptw->xf_appt;
    if (apptw->appointment_changed) {
        fill_appt_from_apptw(appt, apptw);
    }
    xfical_mark_calendar_recur(GTK_CALENDAR(apptw->Recur_calendar1), appt);
    xfical_mark_calendar_recur(GTK_CALENDAR(apptw->Recur_calendar2), appt);
    xfical_mark_calendar_recur(GTK_CALENDAR(apptw->Recur_calendar3), appt);
}

static void on_notebook_page_switch (G_GNUC_UNUSED GtkNotebook *notebook,
                                     G_GNUC_UNUSED GtkWidget *page,
                                     guint page_num, gpointer user_data)
{
    if (page_num == 2)
        refresh_recur_calendars((appt_win *)user_data);
}

static void app_recur_checkbutton_clicked_cb (
    G_GNUC_UNUSED GtkCheckButton *checkbutton, gpointer user_data)
{
    set_repeat_sensitivity((appt_win *)user_data);
    mark_appointment_changed((appt_win *)user_data);
    refresh_recur_calendars((appt_win *)user_data);
}

static void app_recur_feature_checkbutton_clicked_cb (
    G_GNUC_UNUSED GtkCheckButton *cb, gpointer user_data)
{
    recur_hide_show((appt_win *)user_data);
    mark_appointment_changed((appt_win *)user_data);
}

static void app_sound_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb
        , gpointer user_data)
{
    set_sound_sensitivity((appt_win *)user_data);
    mark_appointment_changed((appt_win *)user_data);
}

#ifdef HAVE_NOTIFY
static void app_notify_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb
        , gpointer user_data)
{
    set_notify_sensitivity((appt_win *)user_data);
    mark_appointment_changed((appt_win *)user_data);
}
#endif

static void app_proc_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb
        , gpointer user_data)
{
    set_proc_sensitivity((appt_win *)user_data);
    mark_appointment_changed((appt_win *)user_data);
}

static void app_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb,
                                        gpointer user_data)
{
    mark_appointment_changed((appt_win *)user_data);
}

static void refresh_dependent_data(appt_win *apptw)
{
    if (apptw->el != NULL)
        refresh_el_win((el_win *)apptw->el);
    if (apptw->dw != NULL)
        refresh_day_win((day_win *)apptw->dw);
    orage_mark_appointments();
}

static void on_appNote_buffer_changed_cb (G_GNUC_UNUSED GtkTextBuffer *b,
                                          gpointer user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    GtkTextIter start, end, match_start, match_end;
    GtkTextBuffer *tb;
    GDateTime *gdt;
    gchar *time_str;
    const gchar *fmt;

    tb = apptw->Note_buffer;
    gtk_text_buffer_get_bounds(tb, &start, &end);

    if (gtk_text_iter_forward_search(&start, "<D>"
                , GTK_TEXT_SEARCH_TEXT_ONLY
                , &match_start, &match_end, &end)) { /* found it */
        fmt = "%x";
    }
    else if (gtk_text_iter_forward_search(&start, "<T>"
                , GTK_TEXT_SEARCH_TEXT_ONLY
                , &match_start, &match_end, &end)) { /* found it */
        fmt = "%H:%M";
    }
    else if (gtk_text_iter_forward_search(&start, "<DT>"
                , GTK_TEXT_SEARCH_TEXT_ONLY
                , &match_start, &match_end, &end)) { /* found it */
        fmt = "%x %H:%M";
    }
    else
        fmt = NULL;

    if (fmt)
    {
        gdt = g_date_time_new_now_local ();
        time_str = g_date_time_format (gdt, fmt);
        g_date_time_unref (gdt);

        gtk_text_buffer_delete(tb, &match_start, &match_end);
        gtk_text_buffer_insert(tb, &match_start, time_str, -1);
        g_free (time_str);
    }
   
    mark_appointment_changed((appt_win *)user_data);
}

static void on_app_entry_changed_cb (G_GNUC_UNUSED GtkEditable *entry,
                                     gpointer user_data)
{
    mark_appointment_changed((appt_win *)user_data);
}

static void on_freq_combobox_changed_cb (G_GNUC_UNUSED GtkComboBox *cb,
                                         gpointer user_data)
{
    set_repeat_sensitivity((appt_win *)user_data);
    mark_appointment_changed((appt_win *)user_data);
    refresh_recur_calendars((appt_win *)user_data);
}

static void on_app_combobox_changed_cb (G_GNUC_UNUSED GtkComboBox *cb,
                                        gpointer user_data)
{
    mark_appointment_changed((appt_win *)user_data);
}

static void on_app_spin_button_changed_cb (G_GNUC_UNUSED GtkSpinButton *sb,
                                           gpointer user_data)
{
    mark_appointment_changed((appt_win *)user_data);
}

static void on_recur_spin_button_changed_cb (G_GNUC_UNUSED GtkSpinButton *sb
        , gpointer user_data)
{
    mark_appointment_changed((appt_win *)user_data);
    refresh_recur_calendars((appt_win *)user_data);
}

static void on_appSound_button_clicked_cb (G_GNUC_UNUSED GtkButton *button,
                                           gpointer user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    GtkWidget *file_chooser;
    GtkFileFilter *filter;
    gchar *appSound_entry_filename;
    gchar *sound_file;
    const gchar *filetype[FILETYPE_SIZE] = {
        "*.aiff", "*.al", "*.alsa", "*.au", "*.auto", "*.avr",
        "*.cdr", "*.cvs", "*.dat", "*.vms", "*.gsm", "*.hcom", 
        "*.la", "*.lu", "*.maud", "*.mp3", "*.nul", "*.ogg", "*.ossdsp",
        "*.prc", "*.raw", "*.sb", "*.sf", "*.sl", "*.smp", "*.sndt", 
        "*.sph", "*.8svx", "*.sw", "*.txw", "*.ub", "*.ul", "*.uw",
        "*.voc", "*.vorbis", "*.vox", "*.wav", "*.wve"};
    register int i;

    appSound_entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)apptw->Sound_entry));
    file_chooser = gtk_file_chooser_dialog_new(_("Select a file..."),
            GTK_WINDOW(apptw->Window),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Cancel", GTK_RESPONSE_CANCEL,
            "_OK", GTK_RESPONSE_ACCEPT,
            NULL);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Sound Files"));
    for (i = 0; i < FILETYPE_SIZE; i++) {
        gtk_file_filter_add_pattern(filter, filetype[i]);
    }
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("All Files"));
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), filter);

    gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(file_chooser)
            , PACKAGE_DATA_DIR "/orage/sounds", NULL);

    if (strlen(appSound_entry_filename) > 0)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_chooser)
                , (const gchar *) appSound_entry_filename);
    else
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser)
            , PACKAGE_DATA_DIR "/orage/sounds");

    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
        sound_file = 
            gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
        if (sound_file) {
            gtk_entry_set_text(GTK_ENTRY(apptw->Sound_entry), sound_file);
            gtk_editable_set_position(GTK_EDITABLE(apptw->Sound_entry), -1);
            g_free(sound_file);
        }
    }

    gtk_widget_destroy(file_chooser);
    g_free(appSound_entry_filename);
}

static void app_free_memory(appt_win *apptw)
{
    /* remove myself from event list appointment monitoring list */
    if (apptw->el)
        ((el_win *)apptw->el)->apptw_list = 
                g_list_remove(((el_win *)apptw->el)->apptw_list, apptw);
    /* remove myself from day list appointment monitoring list */
    else if (apptw->dw)
        ((day_win *)apptw->dw)->apptw_list = 
                g_list_remove(((day_win *)apptw->dw)->apptw_list, apptw);
    gtk_widget_destroy(apptw->Window);
    g_free(apptw->xf_uid);
    g_free(apptw->par);
    g_date_time_unref (apptw->par2);
    xfical_appt_free((xfical_appt *)apptw->xf_appt);
    g_free(apptw);
}

static gboolean appWindow_check_and_close(appt_win *apptw)
{
    gint result;

    if (apptw->appointment_changed == TRUE) {
        result = orage_warning_dialog(GTK_WINDOW(apptw->Window)
                , _("The appointment information has been modified.")
                , _("Do you want to continue?")
                , _("No, do not leave")
                , _("Yes, ignore modifications and leave"));

        if (result == GTK_RESPONSE_YES) {
            app_free_memory(apptw);
        }
    }
    else {
        app_free_memory(apptw);
    }
    return(TRUE);
}

static gboolean on_appWindow_delete_event_cb (G_GNUC_UNUSED GtkWidget *widget,
                                              G_GNUC_UNUSED GdkEvent *event,
                                              gpointer user_data)
{
    return(appWindow_check_and_close((appt_win *)user_data));
}

static gboolean orage_validate_datetime(appt_win *apptw, xfical_appt *appt)
{
    /* Journal does not have end time so no need to check */
    if (appt->type == XFICAL_TYPE_JOURNAL
    ||  (appt->type == XFICAL_TYPE_TODO && !appt->use_due_time))
        return(TRUE);

    if (xfical_compare_times(appt) > 0) {
        orage_error_dialog(GTK_WINDOW(apptw->Window)
            , _("The end of this appointment is earlier than the beginning.")
            , NULL);
        return(FALSE);
    }
    else {
        return(TRUE);
    }
}

static void fill_appt_from_apptw_alarm(xfical_appt *appt, appt_win *apptw)
{
    gint i, j, k, l;
    gchar *tmp;

    /* reminder time */
    appt->alarmtime = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(apptw->Alarm_spin_dd)) * 24*60*60
                    + gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(apptw->Alarm_spin_hh)) *    60*60
                    + gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(apptw->Alarm_spin_mm)) *       60
                    ;
    appt->display_alarm_orage = appt->alarmtime ? TRUE : FALSE;

    /* reminder before/after related to start/end */
    /*
    char *when_array[4] = {_("Before Start"), _("Before End")
        , _("After Start"), _("After End")};
        */
    switch (gtk_combo_box_get_active(GTK_COMBO_BOX(apptw->Alarm_when_cb))) {
        case 0:
            appt->alarm_before = TRUE;
            appt->alarm_related_start = TRUE;
            break;
        case 1:
            appt->alarm_before = TRUE;
            appt->alarm_related_start = FALSE;
            break;
        case 2:
            appt->alarm_before = FALSE;
            appt->alarm_related_start = TRUE;
            break;
        case 3:
            appt->alarm_before = FALSE;
            appt->alarm_related_start = FALSE;
            break;
        default:
            appt->alarm_before = TRUE;
            appt->alarm_related_start = TRUE;
            break;
    }

    /* Do we use persistent alarm */
    appt->alarm_persistent = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Per_checkbutton));

    /* Do we use audio alarm */
    appt->sound_alarm = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Sound_checkbutton));

    /* Which sound file will be played */
    if (appt->sound) {
        g_free(appt->sound);
        appt->sound = NULL;
    }
    appt->sound = g_strdup(gtk_entry_get_text(GTK_ENTRY(apptw->Sound_entry)));

    /* the alarm will repeat until someone shuts it off 
     * or it has been played soundrepeat_cnt times */
    appt->soundrepeat = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->SoundRepeat_checkbutton));
    appt->soundrepeat_cnt = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(apptw->SoundRepeat_spin_cnt));
    appt->soundrepeat_len = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(apptw->SoundRepeat_spin_len));

    /* Do we use orage display alarm */
    appt->display_alarm_orage = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Display_checkbutton_orage));

    /* Do we use notify display alarm */
#ifdef HAVE_NOTIFY
    appt->display_alarm_notify = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Display_checkbutton_notify));
#else
    appt->display_alarm_notify = FALSE;
#endif

    /* notify display alarm timeout */
#ifdef HAVE_NOTIFY
    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Display_checkbutton_expire_notify)))
        appt->display_notify_timeout = gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Display_spin_expire_notify));
    else 
        appt->display_notify_timeout = -1;
#else
    appt->display_notify_timeout = -1;
#endif

    /* Do we use procedure alarm */
    appt->procedure_alarm = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Proc_checkbutton));

    /* The actual command. 
     * Note that we need to split this into cmd and the parameters 
     * since that is how libical stores it */
    if (appt->procedure_cmd) {
        g_free(appt->procedure_cmd);
        appt->procedure_cmd = NULL;
    }
    if (appt->procedure_params) {
        g_free(appt->procedure_params);
        appt->procedure_params = NULL;
    }
    tmp = (char *)gtk_entry_get_text(GTK_ENTRY(apptw->Proc_entry));
    l = strlen(tmp);
    for (i = 0; i < l && g_ascii_isspace(tmp[i]); i++)
        ; /* skip blanks from cmd */
    for (j = i; j < l && !g_ascii_isspace(tmp[j]); j++)
        ; /* find first blank after the cmd */
        /* now i points to start of cmd and j points to end of cmd */
    for (k = j; k < l && g_ascii_isspace(tmp[k]); k++)
        ; /* skip blanks from parameters */
        /* now k points to start of params and l points to end of params */
    if (j-i)
        appt->procedure_cmd = g_strndup(tmp+i, j-i);
    if (l-k)
        appt->procedure_params = g_strndup(tmp+k, l-k);
}

/*
 * Function fills an appointment with the contents of an appointment 
 * window
 */
static gboolean fill_appt_from_apptw(xfical_appt *appt, appt_win *apptw)
{
    GtkTextIter start, end;
    GTimeZone *gtz;
    GDateTime *gdt;
    GDateTime *gdt_tmp;
    gint i;
    gchar *tmp, *tmp2;
    gint year;
    gint month;
    gint day;

/* Next line is fix for bug 2811.
 * We need to make sure spin buttons do not have values which are not
 * yet updated = visible.
 * gtk_spin_button_update call would do it, but it seems to cause
 * crash if done in on_app_spin_button_changed_cb and here we should
 * either do it for all spin fields, which takes too many lines of code.
 * if (GTK_WIDGET_HAS_FOCUS(apptw->StartTime_spin_hh))
 *      gtk_spin_button_update;
 * So we just change the focus field and then spin button value gets set.
 * It would be nice to not to have to change the field though.
 */
    gtk_widget_grab_focus(apptw->Title_entry);

            /*********** GENERAL TAB ***********/
    /* type */
    if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->Type_event_rb)))
        appt->type = XFICAL_TYPE_EVENT;
    else if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->Type_todo_rb)))
        appt->type = XFICAL_TYPE_TODO;
    else if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->Type_journal_rb)))
        appt->type = XFICAL_TYPE_JOURNAL;
    else
        g_warning ("%s: coding error, illegal type", G_STRFUNC);

    /* title */
    g_free(appt->title);
    appt->title = g_strdup(gtk_entry_get_text(GTK_ENTRY(apptw->Title_entry)));

    /* location */
    g_free(appt->location);
    appt->location = g_strdup(gtk_entry_get_text(
            GTK_ENTRY(apptw->Location_entry)));

    /* all day */
    appt->allDay = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->AllDay_checkbutton));

    gdt_tmp = g_object_get_data (G_OBJECT (apptw->StartDate_button),
                                 DATE_KEY);
#if USE_GLIB_258
    gtz = g_date_time_get_timezone (gdt_tmp);
#else
    gtz = g_time_zone_new (g_date_time_get_timezone_abbreviation (gdt_tmp));
#endif
    g_date_time_unref (appt->starttime2);
    appt->starttime2 = g_date_time_new (gtz,
                           g_date_time_get_year (gdt_tmp),
                           g_date_time_get_month (gdt_tmp),
                           g_date_time_get_day_of_month (gdt_tmp),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->StartTime_spin_hh)),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->StartTime_spin_mm)),
                           g_date_time_get_seconds (gdt_tmp));

#if (USE_GLIB_258 == 0)
    g_time_zone_unref (gtz);
#endif

    tmp = g_date_time_format (appt->starttime2, XFICAL_APPT_TIME_FORMAT_S0);
    g_strlcpy (appt->starttime, tmp, sizeof (appt->starttime));
    g_free (tmp);

    /* end date and time.
     * Note that timezone is kept upto date all the time
     */
    appt->use_due_time = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->End_checkbutton));

    gdt_tmp = g_object_get_data (G_OBJECT (apptw->EndDate_button),
                                 DATE_KEY);
#if USE_GLIB_258
    gtz = g_date_time_get_timezone (gdt_tmp);
#else
    gtz = g_time_zone_new (g_date_time_get_timezone_abbreviation (gdt_tmp));
#endif
    g_date_time_unref (appt->endtime2);
    appt->endtime2 = g_date_time_new (gtz,
                           g_date_time_get_year (gdt_tmp),
                           g_date_time_get_month (gdt_tmp),
                           g_date_time_get_day_of_month (gdt_tmp),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->EndTime_spin_hh)),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->EndTime_spin_mm)),
                           g_date_time_get_seconds (gdt_tmp));

    tmp = g_date_time_format (appt->endtime2, XFICAL_APPT_TIME_FORMAT_S0);
    g_strlcpy (appt->endtime, tmp, sizeof (appt->endtime));
    g_free (tmp);

#if (USE_GLIB_258 == 0)
    g_time_zone_unref (gtz);
#endif

    /* duration */
    appt->use_duration = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Dur_checkbutton));
    if (appt->allDay)
        appt->duration = gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Dur_spin_dd)) * 24*60*60;
    else
        appt->duration = gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Dur_spin_dd)) * 24*60*60
                        + gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Dur_spin_hh)) *    60*60
                        + gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Dur_spin_mm)) *       60;

    /* Check that end time is after start time. */
    if (!orage_validate_datetime(apptw, appt))
        return(FALSE);

    /* completed date and time.
     * Note that timezone is kept upto date all the time
     */
    appt->completed = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Completed_checkbutton));

    gdt_tmp = g_object_get_data (G_OBJECT (apptw->CompletedDate_button),
                                 DATE_KEY);

#if USE_GLIB_258
    gtz = g_date_time_get_timezone (gdt_tmp);
#else
    gtz = g_time_zone_new (g_date_time_get_timezone_abbreviation (gdt_tmp));
#endif

    gdt = g_date_time_new (gtz,
                           g_date_time_get_year (gdt_tmp),
                           g_date_time_get_month (gdt_tmp),
                           g_date_time_get_day_of_month (gdt_tmp),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->CompletedTime_spin_hh)),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->CompletedTime_spin_mm)),
                           g_date_time_get_seconds (gdt_tmp));

    tmp = g_date_time_format (gdt, XFICAL_APPT_TIME_FORMAT_S0);
    g_date_time_unref (gdt);
    g_strlcpy (appt->completedtime, tmp, sizeof (appt->completedtime));
    g_free (tmp);

#if (USE_GLIB_258 == 0)
    g_time_zone_unref (gtz);
#endif

    /* availability */
    appt->availability = gtk_combo_box_get_active(
            GTK_COMBO_BOX(apptw->Availability_cb));

    /* categories */
    /* Note that gtk_entry_get_text returns empty string, which is not
       the same as null, so tmp must always be freen */
    tmp = g_strdup(gtk_entry_get_text(GTK_ENTRY(apptw->Categories_entry)));
    tmp2 = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(apptw->Categories_cb));
    if (!strcmp(tmp2, _("Not set"))) {
        g_free(tmp2);
        tmp2 = NULL;
    }
    if (ORAGE_STR_EXISTS(tmp)) {
        g_free(appt->categories);
        appt->categories = g_strjoin(",", tmp, tmp2, NULL);
        g_free(tmp2);
    }
    else
        appt->categories = tmp2;
    g_free(tmp);

    /* priority */
    appt->priority = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(apptw->Priority_spin));

    /* notes */
    gtk_text_buffer_get_bounds(apptw->Note_buffer, &start, &end);
    g_free(appt->note);
    appt->note = gtk_text_iter_get_text(&start, &end);

            /*********** ALARM TAB ***********/
    fill_appt_from_apptw_alarm(appt, apptw);

            /*********** RECURRENCE TAB ***********/
    /* recurrence */
    appt->freq = gtk_combo_box_get_active(GTK_COMBO_BOX(apptw->Recur_freq_cb));

    /* recurrence interval */
    appt->interval = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(apptw->Recur_int_spin));

    /* recurrence ending */
    if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_limit_rb))) {
        appt->recur_limit = 0;    /* no limit */
        appt->recur_count = 0;    /* special: means no repeat count limit */
        appt->recur_until[0] = 0; /* special: means no until time limit */
    }
    else if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_count_rb))) {
        appt->recur_limit = 1;    /* count limit */
        appt->recur_count = gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Recur_count_spin));
        appt->recur_until[0] = 0; /* special: means no until time limit */
    }
    else if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_until_rb))) {
        appt->recur_limit = 2;    /* until limit */
        appt->recur_count = 0;    /* special: means no repeat count limit */

        gdt_tmp = g_object_get_data (G_OBJECT (apptw->Recur_until_button),
                                     DATE_KEY);
        g_date_time_get_ymd (gdt_tmp, &year, &month, &day);
        gdt = g_date_time_new_local (year, month, day, 23, 59, 10);
        tmp = g_date_time_format (gdt, XFICAL_APPT_TIME_FORMAT);
        g_strlcpy (appt->recur_until, tmp, sizeof (appt->recur_until));
        g_object_set_data_full (G_OBJECT (apptw->Recur_until_button),
                                DATE_KEY, gdt,
                                (GDestroyNotify)g_date_time_unref);
        g_free (tmp);
    }
    else
        g_warning ("%s: coding error, illegal recurrence", G_STRFUNC);

    /* recurrence weekdays */
    for (i=0; i <= 6; i++) {
        appt->recur_byday[i] = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_byday_cb[i]));
    /* recurrence weekday selection for month/yearly case */
        appt->recur_byday_cnt[i] = gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Recur_byday_spin[i]));
    }

    /* recurrence todo base */
    appt->recur_todo_base_start = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Recur_todo_base_start_rb));

    return(TRUE);
}

static void add_file_select_cb(appt_win *apptw)
{
    GtkWidget *tool_item;
    gint i = 0;
    gboolean use_list = FALSE;

    /* Build insert file selection combobox */
    apptw->File_insert_cb = NULL;
    if (g_par.foreign_count == 0) { /* we do not have foreign files */
        return;
    }

    apptw->File_insert_cb = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(apptw->File_insert_cb
            , _("Add new appointment to this file."));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(apptw->File_insert_cb), _("Orage default file"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(apptw->File_insert_cb), 0);
    for (i = 0; i < g_par.foreign_count; i++) {
        if (!g_par.foreign_data[i].read_only) { /* do not add to RO files */
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(apptw->File_insert_cb)
                    , g_par.foreign_data[i].name);
            use_list = TRUE;
        }
    }

    if (!use_list) { /* it is not needed after all */
        gtk_widget_destroy(apptw->File_insert_cb);
        apptw->File_insert_cb = NULL;
    }

    /* Add insert file combobox to toolbar */
    (void)orage_toolbar_append_separator(apptw->Toolbar, -1);
    tool_item = GTK_WIDGET(gtk_tool_item_new());
    gtk_container_add(GTK_CONTAINER(tool_item), apptw->File_insert_cb);
    gtk_toolbar_insert(GTK_TOOLBAR(apptw->Toolbar), GTK_TOOL_ITEM(tool_item), -1);
}

static void remove_file_select_cb(appt_win *apptw)
{
    if (apptw->File_insert_cb)
        gtk_widget_destroy(apptw->File_insert_cb);
}

static gboolean save_xfical_from_appt_win(appt_win *apptw)
{
    gint i;
    gboolean ok = FALSE, found = FALSE;
    xfical_appt *appt = (xfical_appt *)apptw->xf_appt;
    char *xf_file_id, *tmp;

    if (fill_appt_from_apptw(appt, apptw)) {
        ok = TRUE;
        /* Here we try to save the event... */
        if (!xfical_file_open(TRUE)) {
            g_warning ("%s: file open and update failed: %s",
                       G_STRFUNC, apptw->xf_uid);
            return(FALSE);
        }
        if (apptw->appointment_add) {
            /* first check which file we are adding to */
            if (apptw->File_insert_cb) {
                tmp = gtk_combo_box_text_get_active_text(
                        GTK_COMBO_BOX_TEXT(apptw->File_insert_cb));
                if (strcmp(tmp, _("Orage default file")) == 0) {
                    xf_file_id = g_strdup("O00.");
                }
                else {
                    for (i = 0; i < g_par.foreign_count && !found; i++) {
                        if (strcmp(g_par.foreign_data[i].file, tmp) == 0 ||
                            strcmp(g_par.foreign_data[i].name, tmp) == 0) {
                            found = TRUE;
                        }
                    }
                    if (found) { /* it should always been found */
                        xf_file_id = g_strdup_printf("F%02d.", i-1);
                    }
                    else { /* error! */
                        g_warning ("%s: Matching foreign file not found: %s",
                                   G_STRFUNC, tmp);
                        ok = FALSE;
                    }
                }
            }
            else {
                xf_file_id = g_strdup("O00.");
            }
            if (ok) {
                apptw->xf_uid = g_strdup(xfical_appt_add(xf_file_id, appt));
                g_free(xf_file_id);
                ok = (apptw->xf_uid ? TRUE : FALSE);
            }
            if (ok) {
                apptw->appointment_add = FALSE;
                gtk_widget_set_sensitive(apptw->Duplicate, TRUE);
                gtk_widget_set_sensitive(apptw->File_menu_duplicate, TRUE);
                g_message ("Added: %s", apptw->xf_uid);
                remove_file_select_cb(apptw);
            }
            else {
                g_warning ("%s: Addition failed: %s", G_STRFUNC, apptw->xf_uid);
                (void)orage_error_dialog(GTK_WINDOW(apptw->Window)
                        , _("Appointment addition failed.")
                        , _("Error happened when adding appointment. Look more details from the log file."));
            }
        }
        else {
            ok = xfical_appt_mod(apptw->xf_uid, appt);
            if (ok)
                g_message ("Modified: %s", apptw->xf_uid);
            else {
                g_warning ("%s: Modification failed: %s",
                           G_STRFUNC, apptw->xf_uid);
                (void)orage_error_dialog(GTK_WINDOW(apptw->Window)
                        , _("Appointment update failed.")
                        , _("Look more details from the log file. (Perhaps file was updated external from Orage?)"));
            }
        }
        xfical_file_close(TRUE);
        if (ok) {
            apptw->appointment_new = FALSE;
            mark_appointment_unchanged(apptw);
            refresh_dependent_data(apptw);
        }
    }
    return(ok);
}

static void on_appFileSave_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                             gpointer user_data)
{
    save_xfical_from_appt_win((appt_win *)user_data);
}

static void on_appSave_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                   gpointer user_data)
{
    save_xfical_from_appt_win((appt_win *)user_data);
}

static void save_xfical_from_appt_win_and_close(appt_win *apptw)
{
    if (save_xfical_from_appt_win(apptw)) {
        app_free_memory(apptw);
    }
}

static void on_appFileSaveClose_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    save_xfical_from_appt_win_and_close((appt_win *)user_data);
}

static void on_appSaveClose_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                        gpointer user_data)
{
    save_xfical_from_appt_win_and_close((appt_win *)user_data);
}

static void delete_xfical_from_appt_win(appt_win *apptw)
{
    gint result;
    gboolean ok = FALSE;

    result = orage_warning_dialog(GTK_WINDOW(apptw->Window)
            , _("This appointment will be permanently removed.")
            , _("Do you want to continue?")
            , _("No, cancel the removal")
            , _("Yes, remove it"));
                                 
    if (result == GTK_RESPONSE_YES) {
        if (!apptw->appointment_add) {
            if (!xfical_file_open(TRUE)) {
                g_warning ("%s: file open and removal failed: %s",
                           G_STRFUNC, apptw->xf_uid);
                return;
            }
            ok = xfical_appt_del(apptw->xf_uid);
            if (ok)
                g_message ("Removed: %s", apptw->xf_uid);
            else
                g_warning ("%s: Removal failed: %s", G_STRFUNC, apptw->xf_uid);
            xfical_file_close(TRUE);
        }

        refresh_dependent_data(apptw);
        app_free_memory(apptw);
    }
}

static void on_appDelete_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                     gpointer user_data)
{
    delete_xfical_from_appt_win((appt_win *)user_data);
}

static void on_appFileDelete_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    delete_xfical_from_appt_win((appt_win *)user_data);
}

static void on_appFileClose_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    appWindow_check_and_close((appt_win *)user_data);
}

static void duplicate_xfical_from_appt_win(appt_win *apptw)
{
    gint x, y;
    appt_win *apptw2;
    GDateTime *gdt;

    /* do not keep track of appointments created here */
    gdt = g_date_time_new_now_local ();
    apptw2 = create_appt_win("COPY", apptw->xf_uid, gdt);
    g_date_time_unref (gdt);
    if (apptw2) {
        gtk_window_get_position(GTK_WINDOW(apptw->Window), &x, &y);
        gtk_window_move(GTK_WINDOW(apptw2->Window), x+20, y+20);
    }
}

static void on_appFileDuplicate_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    duplicate_xfical_from_appt_win((appt_win *)user_data);
}

static void on_appDuplicate_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                        gpointer user_data)
{
    duplicate_xfical_from_appt_win((appt_win *)user_data);
}

static void revert_xfical_to_last_saved(appt_win *apptw)
{
    GDateTime *gdt;
    if (!apptw->appointment_new) {
        gdt = g_date_time_new_now_local ();
        fill_appt_window(apptw, "UPDATE", apptw->xf_uid, gdt);
        g_date_time_unref (gdt);
    }
    else {
        fill_appt_window(apptw, "NEW", apptw->par, apptw->par2);
    }
}

static void on_appFileRevert_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    revert_xfical_to_last_saved((appt_win *)user_data);
}

static void on_appRevert_clicked_cb (G_GNUC_UNUSED GtkWidget *b,
                                     gpointer *user_data)
{
    revert_xfical_to_last_saved((appt_win *)user_data);
}

static void on_Date_button_clicked_cb(GtkWidget *button, gpointer *user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    GtkWidget *selDate_dialog;

    selDate_dialog = gtk_dialog_new_with_buttons(
            _("Pick the date"), GTK_WINDOW(apptw->Window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("Today"), 1, "_OK", GTK_RESPONSE_ACCEPT, NULL);

    if (orage_date_button_clicked (button, selDate_dialog))
        mark_appointment_changed(apptw);
}

static void on_recur_Date_button_clicked_cb(GtkWidget *button
        , gpointer *user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    GtkWidget *selDate_dialog;

    selDate_dialog = gtk_dialog_new_with_buttons(
            _("Pick the date"), GTK_WINDOW(apptw->Window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("Today"), 1, "_OK", GTK_RESPONSE_ACCEPT, NULL);

    if (orage_date_button_clicked (button, selDate_dialog))
        mark_appointment_changed(apptw);
    refresh_recur_calendars((appt_win *)user_data);
}

static void on_appStartTimezone_clicked_cb(GtkButton *button
        , gpointer *user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    xfical_appt *appt;

    appt = (xfical_appt *)apptw->xf_appt;
    if (orage_timezone_button_clicked(button, GTK_WINDOW(apptw->Window)
            , &appt->start_tz_loc, TRUE, g_par.local_timezone))
        mark_appointment_changed(apptw);
}

static void on_appEndTimezone_clicked_cb(GtkButton *button
        , gpointer *user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    xfical_appt *appt;

    appt = (xfical_appt *)apptw->xf_appt;
    if (orage_timezone_button_clicked(button, GTK_WINDOW(apptw->Window)
            , &appt->end_tz_loc, TRUE, g_par.local_timezone))
        mark_appointment_changed(apptw);
}

static void on_appCompletedTimezone_clicked_cb(GtkButton *button
        , gpointer *user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    xfical_appt *appt;

    appt = (xfical_appt *)apptw->xf_appt;
    if (orage_timezone_button_clicked(button, GTK_WINDOW(apptw->Window)
            , &appt->completed_tz_loc, TRUE, g_par.local_timezone))
        mark_appointment_changed(apptw);
}

static gint check_exists(gconstpointer a, gconstpointer b)
{
   /*  We actually only care about match or no match.*/
    if (!strcmp(((xfical_exception *)a)->time
              , ((xfical_exception *)b)->time)) {
        return(strcmp(((xfical_exception *)a)->type
                    , ((xfical_exception *)b)->type));
    }
    else {
        return(1); /* does not matter if it is smaller or bigger */
    }
}

static xfical_exception *new_exception(gchar *text)
{
    xfical_exception *recur_exception;
    gint i;
    gchar *ical_str;
    struct tm tm_time = {0};
    GDateTime *gdt;
    gchar *fmt;
#ifndef HAVE_LIBICAL
    char *tmp;
#endif

    recur_exception = g_new(xfical_exception, 1);
    i = strlen(text);
    text[i-2] = '\0';
    if (text[i-1] == '+') {
        g_strlcpy (recur_exception->type, "RDATE",
                   sizeof(recur_exception->type));
        gdt = orage_i18_time_to_gdatetime (text);
        ical_str = g_date_time_format (gdt, XFICAL_APPT_TIME_FORMAT);
        g_date_time_unref (gdt);
    }
    else {
        g_strlcpy (recur_exception->type, "EXDATE",
                   sizeof(recur_exception->type));
#ifdef HAVE_LIBICAL
        /* need to add time also as standard libical can not handle dates
           correctly yet. Check more from BUG 5764.
           We use start time from appointment. */
        /* we should not have dates as we are using standard libical,
           but if this fails (=return NULL) we may have date from somewhere
           else */
        if ((char *)strptime(text, "%x %R", &tm_time) == NULL)
        {
            gdt = orage_i18_date_to_gdatetime (text);
            fmt = XFICAL_APPT_DATE_FORMAT;
        }
        else
        {
            gdt = orage_i18_time_to_gdatetime (text);
            fmt = XFICAL_APPT_TIME_FORMAT;
        }
#else
        /* we should not have date-times as we are using internal libical,
           which only uses dates, but if this returns non null, we may have
           datetime from somewhere else */
        tmp = (char *)strptime(text, "%x", &tm_time);
        if (ORAGE_STR_EXISTS(tmp))
        {
            gdt = orage_i18_time_to_gdatetime (text);
            fmt = XFICAL_APPT_TIME_FORMAT;
        }
        else
        {
            gdt = orage_i18_date_to_gdatetime (text);
            fmt = XFICAL_APPT_DATE_FORMAT;
        }
#endif
        ical_str = g_date_time_format (gdt, fmt);
        g_date_time_unref (gdt);
    }

    g_strlcpy (recur_exception->time, ical_str,
               sizeof (recur_exception->time));
    g_free (ical_str);
    text[i-2] = ' ';
    return(recur_exception);
}

static void recur_row_clicked(GtkWidget *widget
        , GdkEventButton *event, gpointer *user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    xfical_appt *appt;
    gchar *text;
    GList *children;
    GtkWidget *lab;
    xfical_exception *recur_exception, *recur_exception_cur;
    GList *gl_pos;

    if (event->type == GDK_2BUTTON_PRESS) {
        /* first find the text */
        children = gtk_container_get_children(GTK_CONTAINER(widget));
        children = g_list_first(children);
        lab = (GtkWidget *)children->data;
        text = g_strdup(gtk_label_get_text(GTK_LABEL(lab)));

         /* Then, let's keep the GList updated */
        recur_exception = new_exception(text);
        appt = (xfical_appt *)apptw->xf_appt;
        g_free(text);
        if ((gl_pos = g_list_find_custom(appt->recur_exceptions
                    , recur_exception, check_exists))) {
            /* let's remove it */
            recur_exception_cur = gl_pos->data;
            appt->recur_exceptions = 
                    g_list_remove(appt->recur_exceptions, recur_exception_cur);
            g_free(recur_exception_cur);
        }
        else { 
            g_warning ("%s: non existent row (%s)", G_STRFUNC,
                       recur_exception->time);
        }
        g_free(recur_exception);

        /* and finally update the display */
        gtk_widget_destroy(widget);
        mark_appointment_changed(apptw);
        refresh_recur_calendars(apptw);
    }
}

static gboolean add_recur_exception_row (const gchar *p_time,
                                         const gchar *p_type,
                                         appt_win *apptw,
                                         const gboolean only_window)
{
    GtkWidget *ev, *label;
    gchar *text, tmp_type[2];
    xfical_appt *appt;
    xfical_exception *recur_exception;

    /* First build the data */
    if (!strcmp(p_type, "EXDATE"))
        tmp_type[0] = '-';
    else if (!strcmp(p_type, "RDATE"))
        tmp_type[0] = '+';
    else
        tmp_type[0] = p_type[0];
    tmp_type[1] = '\0';
    text = g_strdup_printf("%s %s", p_time, tmp_type);

    /* Then, let's keep the GList updated */
    if (!only_window) {
        recur_exception = new_exception(text);
        appt = (xfical_appt *)apptw->xf_appt;
        if (g_list_find_custom(appt->recur_exceptions, recur_exception
                    , check_exists)) {
            /* this element is already in the list, so no need to add it again.
             * we just clean the memory and leave */
            g_free(recur_exception);
            g_free(text);
            return(FALSE);
        }
        else { /* need to add it */
            appt->recur_exceptions = g_list_prepend(appt->recur_exceptions
                    , recur_exception);
        }
    }

    /* finally put the value visible also */
    label = gtk_label_new(text);
    g_free(text);
    g_object_set (label, "xalign", 0.0, "yalign", 0.5, NULL);
    ev = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(ev), label);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_exception_rows_vbox),
                             ev, NULL, GTK_POS_BOTTOM, 1, 1);
    g_signal_connect(ev, "button-press-event", G_CALLBACK (recur_row_clicked),
                     apptw);
    gtk_widget_show(label);
    gtk_widget_show(ev);
    return(TRUE); /* we added the value */
}

static void recur_month_changed_cb(GtkCalendar *calendar, gpointer user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    xfical_appt *appt;

    appt = (xfical_appt *)apptw->xf_appt;
    /* actually we do not have to do fill_appt_from_apptw always, 
     * but as we are not keeping track of changes, we just do it always */
    fill_appt_from_apptw(appt, apptw);
    xfical_mark_calendar_recur(calendar, appt);
}

static void recur_day_selected_double_click_cb(GtkCalendar *calendar
        , gpointer user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    gchar *cal_date, *type;
    gint hh, mm;

    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Recur_exception_excl_rb))) {
        type = "-";
#ifdef HAVE_LIBICAL
        /* need to add time also as standard libical can not handle dates
           correctly yet. Check more from BUG 5764.
           We use start time from appointment. */
        if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->AllDay_checkbutton)))
            cal_date = orage_cal_to_i18_date (calendar);
        else {
            hh =  gtk_spin_button_get_value_as_int(
                    GTK_SPIN_BUTTON(apptw->StartTime_spin_hh));
            mm =  gtk_spin_button_get_value_as_int(
                    GTK_SPIN_BUTTON(apptw->StartTime_spin_mm));
            cal_date = orage_cal_to_i18_time(calendar, hh, mm);
        }
#else
        /* date is enough */
        cal_date = orage_cal_to_i18_date (calendar);
#endif
    }
    else { /* extra day. This needs also time */
        type = "+";
        hh =  gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Recur_exception_incl_spin_hh));
        mm =  gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Recur_exception_incl_spin_mm));
        cal_date = orage_cal_to_i18_time (calendar, hh, mm);
    }

    if (add_recur_exception_row(cal_date, type, apptw, FALSE)) { /* new data */
        mark_appointment_changed((appt_win *)user_data);
        refresh_recur_calendars((appt_win *)user_data);
    }
    g_free(cal_date);
}

static void fill_appt_window_times(appt_win *apptw, xfical_appt *appt)
{
    gchar *date_to_display;
    int days, hours, minutes;
    GDateTime *gdt;

    /* all day ? */
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(apptw->AllDay_checkbutton), appt->allDay);

    /* start time */
    if (strlen(appt->starttime) > 6 ) {
        gdt = g_date_time_ref (appt->starttime2);
        g_object_set_data_full (G_OBJECT (apptw->StartDate_button),
                                DATE_KEY, gdt,
                                (GDestroyNotify)g_date_time_unref);
        date_to_display = g_date_time_format (gdt, "%x");
        gtk_button_set_label (GTK_BUTTON(apptw->StartDate_button),
                              date_to_display);
        g_free (date_to_display);

        gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->StartTime_spin_hh),
                                   g_date_time_get_hour (gdt));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->Recur_exception_incl_spin_hh),
                                   g_date_time_get_hour (gdt));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->StartTime_spin_mm),
                                   g_date_time_get_minute (gdt));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->Recur_exception_incl_spin_mm),
                                   g_date_time_get_minute (gdt));

        if (appt->start_tz_loc)
        {
            gtk_button_set_label(GTK_BUTTON(apptw->StartTimezone_button)
                    , _(appt->start_tz_loc));
        }
        else /* we should never get here */
            g_warning ("%s: start_tz_loc is null", G_STRFUNC);
    }
    else
        g_warning ("%s: starttime wrong %s", G_STRFUNC, appt->uid);

    /* end time */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            apptw->End_checkbutton), appt->use_due_time);
    if (strlen(appt->endtime) > 6 ) {
        gdt = g_date_time_ref (appt->endtime2);
        g_object_set_data_full (G_OBJECT (apptw->EndDate_button),
                                DATE_KEY, gdt,
                                (GDestroyNotify)g_date_time_unref);
        date_to_display = g_date_time_format (gdt, "%x");
        gtk_button_set_label (GTK_BUTTON (apptw->EndDate_button),
                              date_to_display);
        g_free (date_to_display);

        gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->EndTime_spin_hh),
                                   g_date_time_get_hour (gdt));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->EndTime_spin_mm),
                                   g_date_time_get_minute (gdt));

        if (appt->end_tz_loc) {
            gtk_button_set_label(GTK_BUTTON(apptw->EndTimezone_button)
                    , _(appt->end_tz_loc));
        }
        else /* we should never get here */
            g_warning ("%s: end_tz_loc is null", G_STRFUNC);
    }
    else
        g_warning ("%s: endtime wrong %s", G_STRFUNC, appt->uid);

    /* duration */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(apptw->Dur_checkbutton)
            , appt->use_duration);
    days = appt->duration/(24*60*60);
    hours = (appt->duration-days*(24*60*60))/(60*60);
    minutes = (appt->duration-days*(24*60*60)-hours*(60*60))/(60);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->Dur_spin_dd)
            , (gdouble)days);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->Dur_spin_hh)
            , (gdouble)hours);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->Dur_spin_mm)
            , (gdouble)minutes);

    /* completed time (only available for todo) */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            apptw->Completed_checkbutton), appt->completed);
    if (strlen(appt->completedtime) > 6 ) {
        gdt = orage_icaltime_to_gdatetime (appt->completedtime, FALSE);
        g_object_set_data_full (G_OBJECT (apptw->CompletedDate_button),
                                DATE_KEY, gdt,
                                (GDestroyNotify)g_date_time_unref);
        date_to_display = g_date_time_format (gdt, "%x");
        gtk_button_set_label (GTK_BUTTON(apptw->CompletedDate_button),
                              date_to_display);
        g_free (date_to_display);

        gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->CompletedTime_spin_hh),
                                   g_date_time_get_hour (gdt));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->CompletedTime_spin_mm),
                                   g_date_time_get_minute (gdt));

        if (appt->completed_tz_loc) {
            gtk_button_set_label(GTK_BUTTON(apptw->CompletedTimezone_button)
                    , _(appt->completed_tz_loc));
        }
        else /* we should never get here */
            g_warning ("%s: completed_tz_loc is null", G_STRFUNC);
    }
    else
        g_warning ("%s: completedtime wrong %s", G_STRFUNC, appt->uid);
}

/** @param action action to be taken, possible values: "NEW", "UPDATE" and "COPY"
 *  @param par contains XFICAL_APPT_DATE_FORMAT (yyyymmdd) date for "NEW"
 *         appointment and ical uid for "UPDATE" and "COPY"
 */
static xfical_appt *fill_appt_window_get_new_appt (const gchar *par,
                                                   GDateTime *par_gdt)
{
    xfical_appt *appt;
    GDateTime *gdt_now;
    gchar *time_str;
    gint hour;
    gint minute;
    gint par_year;
    gint par_month;
    gint par_day_of_month;
    gint start_hour;
    gint end_hour;
    gint start_minute;
    gint end_minute;

    appt = xfical_appt_alloc ();
    gdt_now = g_date_time_new_now_local ();
    hour = g_date_time_get_hour (gdt_now);

    g_date_time_get_ymd (par_gdt, &par_year, &par_month, &par_day_of_month);
    if (orage_gdatetime_compare_date (par_gdt, gdt_now) == 0 && (hour < 23))
    {
        /* If we're today, we propose an appointment the next half-hour hour
         * 24 is wrong, we use 00.
         */
        minute = g_date_time_get_minute (gdt_now);
        if (minute <= 30)
        {
            start_hour = hour;
            start_minute = 30;
            end_hour = hour + 1;
            end_minute = 0;
        }
        else
        {
            start_hour = hour + 1;
            start_minute = 0;
            end_hour = hour + 1;
            end_minute = 30;
        }
    }
    else
    {
        /* Otherwise we suggest it at 09:00 in the morning. */
        start_hour = 9;
        start_minute = 0;
        end_hour = 9;
        end_minute = 30;
    }

    g_date_time_unref (appt->starttime2);
    appt->starttime2 = g_date_time_new_local (par_year, par_month,
                                              par_day_of_month, start_hour,
                                              start_minute, 0);

    time_str = g_date_time_format (appt->starttime2, XFICAL_APPT_TIME_FORMAT);
    g_strlcpy (appt->starttime, time_str, sizeof (appt->starttime));
    g_free (time_str);

    g_date_time_unref (appt->endtime2);
    appt->endtime2 = g_date_time_new_local (par_year, par_month,
                                            par_day_of_month, end_hour,
                                            end_minute, 0);

    time_str = g_date_time_format (appt->endtime2, XFICAL_APPT_TIME_FORMAT);
    g_strlcpy (appt->endtime, time_str, sizeof (appt->endtime));
    g_free (time_str);

    if (g_par.local_timezone_utc)
        appt->start_tz_loc = g_strdup ("UTC");
    else if (g_par.local_timezone)
        appt->start_tz_loc = g_strdup (g_par.local_timezone);
    else
        appt->start_tz_loc = g_strdup ("floating");

    appt->end_tz_loc = g_strdup (appt->start_tz_loc);
    appt->duration = 30*60;
    /* use NOT completed by default for new TODO */
    appt->completed = FALSE;
    /* use duration by default for new appointments */
    appt->use_duration = TRUE;
    time_str = g_date_time_format (gdt_now, XFICAL_APPT_TIME_FORMAT_S0);
    g_date_time_unref (gdt_now);
    g_strlcpy (appt->completedtime, time_str, sizeof (appt->completedtime));
    g_free (time_str);
    appt->completed_tz_loc = g_strdup (appt->start_tz_loc);

    read_default_alarm (appt);

    return appt;
}

static xfical_appt *fill_appt_window_get_appt(appt_win *apptw
    , const gchar *action, const gchar *par, GDateTime *par_gdt)
{
    xfical_appt *appt=NULL;

    if (strcmp(action, "NEW") == 0)
        appt = fill_appt_window_get_new_appt (par, par_gdt);
    else if ((strcmp(action, "UPDATE") == 0) || (strcmp(action, "COPY") == 0)) {
        if (!par) {
            g_message ("%s appointment with null id. Ending.", action);
            return(NULL);
        }
        if (!xfical_file_open(TRUE))
            return(NULL);
        if ((appt = xfical_appt_get(par)) == NULL) {
            orage_info_dialog(GTK_WINDOW(apptw->Window)
                , _("This appointment does not exist.")
                , _("It was probably removed, please refresh your screen."));
        }
        xfical_file_close(TRUE);
    }
    else {
        g_error("unknown parameter");
    }

    return(appt);
}

/************************************************************/
/* categories start.                                        */
/************************************************************/

typedef struct _orage_category
{
    gchar *category;
    GdkRGBA color;
} orage_category_struct;

GList *orage_category_list = NULL;

static OrageRc *orage_category_file_open (const gboolean read_only)
{
    gchar *fpath;
    OrageRc *orc;

    fpath = orage_data_file_location(ORAGE_CATEGORIES_DIR_FILE);
    if ((orc = orage_rc_file_open(fpath, read_only)) == NULL) {
        g_warning ("%s: category file open failed.", G_STRFUNC);
    }
    g_free(fpath);

    return(orc);
}

GdkRGBA *orage_category_list_contains(char *categories)
{
    GList *cat_l;
    orage_category_struct *cat;
    
    if (categories == NULL)
        return(NULL);
    for (cat_l = g_list_first(orage_category_list);
         cat_l != NULL;
         cat_l = g_list_next(cat_l)) {
        cat = (orage_category_struct *)cat_l->data;
        if (g_str_has_suffix(categories, cat->category)) {
            return(&cat->color);
        }
    }
    /* not found */
    return(NULL);
}

static void orage_category_free(gpointer gcat, G_GNUC_UNUSED gpointer dummy)
{
    orage_category_struct *cat = (orage_category_struct *)gcat;

    g_free(cat->category);
    g_free(cat);
}

static void orage_category_free_list(void)
{
    g_list_foreach(orage_category_list, orage_category_free, NULL);
    g_list_free(orage_category_list);
    orage_category_list = NULL;
}

void orage_category_get_list(void)
{
    GdkRGBA rgba;
    OrageRc *orc;
    gchar **cat_groups;
    gint i;
    orage_category_struct *cat;

    if (orage_category_list != NULL)
        orage_category_free_list();

    orc = orage_category_file_open(TRUE);
    cat_groups = orage_rc_get_groups(orc);
    for (i = 0; cat_groups[i] != NULL; i++) {
        orage_rc_set_group(orc, cat_groups[i]);
        if (orage_rc_read_color (orc, ORAGE_RC_COLOUR, &rgba, NULL))
        {
            cat = g_new(orage_category_struct, 1);
            cat->category = g_strdup(cat_groups[i]);
            cat->color = rgba;
            orage_category_list = g_list_prepend(orage_category_list, cat);
        }
    }
    g_strfreev(cat_groups);
    orage_rc_file_close(orc);
}

static gboolean category_fill_cb(GtkComboBoxText *cb, char *selection)
{
    OrageRc *orc;
    gchar **cat_gourps;
    gint i;
    gboolean found = FALSE;

    orc = orage_category_file_open(TRUE);
    cat_gourps = orage_rc_get_groups(orc);
    gtk_combo_box_text_append_text(cb, _("Not set"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(cb), 0);
    for (i = 0; cat_gourps[i] != NULL; i++) {
        gtk_combo_box_text_append_text(cb, (const gchar *)cat_gourps[i]);
        if (!found && selection && !strcmp(selection, cat_gourps[i])) {
            /* +1 as we have _("Not set") there first */
            gtk_combo_box_set_active(GTK_COMBO_BOX(cb), i+1);
            found = TRUE;
        }
    }
    g_strfreev(cat_gourps);
    orage_rc_file_close(orc);
    return(found);
}

static void orage_category_refill_cb(appt_win *apptw)
{
    gchar *tmp;

    /* first remember the currently selected value */
    tmp = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(apptw->Categories_cb));

    /* then clear the values by removing the widget and putting it back */
    gtk_widget_destroy(apptw->Categories_cb);
    apptw->Categories_cb = gtk_combo_box_text_new();
    gtk_container_add(GTK_CONTAINER(apptw->Categories_cb_event)
            , apptw->Categories_cb);

    /* and finally fill it with new values */
    category_fill_cb(GTK_COMBO_BOX_TEXT(apptw->Categories_cb), tmp);

    g_free(tmp);
    gtk_widget_show(apptw->Categories_cb);
}

static void fill_category_data(appt_win *apptw, xfical_appt *appt)
{
    gchar *tmp = NULL;

    /* first search the last entry. which is the special color value */
    if (appt->categories) {
        tmp = g_strrstr(appt->categories, ",");
        if (!tmp) /* , not found, let's take the whole string */
            tmp = appt->categories;
        while (*(tmp) == ' ' || *(tmp) == ',') /* skip blanks and , */
            tmp++; 
    }
    if (category_fill_cb(GTK_COMBO_BOX_TEXT(apptw->Categories_cb), tmp) && tmp != NULL) {
        /* we found match. Let's try to hide that from the entry text */
        while (tmp != appt->categories 
                && (*(tmp-1) == ' ' || *(tmp-1) == ','))
            tmp--;
        *tmp = '\0'; /* note that this goes to appt->categories */
    }
    gtk_entry_set_text(GTK_ENTRY(apptw->Categories_entry)
            , (appt->categories ? appt->categories : ""));
}

static void orage_category_write_entry (const gchar *category,
                                        const GdkRGBA *color)
{
    OrageRc *orc;
    gchar *color_str;

    if (!ORAGE_STR_EXISTS(category)) {
        g_message ("%s: empty category. Not written", G_STRFUNC);
        return;
    }
    color_str = gdk_rgba_to_string (color);
    orc = orage_category_file_open(FALSE);
    orage_rc_set_group(orc, category);
    orage_rc_put_str(orc, ORAGE_RC_COLOUR, color_str);
    g_free(color_str);
    orage_rc_file_close(orc);
}

static void orage_category_remove_entry(gchar *category)
{
    OrageRc *orc;

    if (!ORAGE_STR_EXISTS(category)) {
        g_message ("%s: empty category. Not removed", G_STRFUNC);
        return;
    }
    orc = orage_category_file_open(FALSE);
    orage_rc_del_group(orc, category);
    orage_rc_file_close(orc);
}

static void close_cat_window(gpointer user_data)
{
    category_win_struct *catw = (category_win_struct *)user_data;

    orage_category_refill_cb((appt_win *)catw->apptw);
    gtk_widget_destroy(catw->window);
    g_free(catw);
}

static gboolean on_cat_window_delete_event (G_GNUC_UNUSED GtkWidget *w,
                                            G_GNUC_UNUSED GdkEvent *e,
                                            gpointer user_data)
{
    close_cat_window(user_data);

    return(FALSE);
}

static void cat_add_button_clicked (G_GNUC_UNUSED GtkButton *button,
                                    gpointer user_data)
{
    category_win_struct *catw = (category_win_struct *)user_data;
    gchar *entry_category;
    GdkRGBA color;

    entry_category = g_strdup(gtk_entry_get_text((GtkEntry *)catw->new_entry));
    g_strstrip(entry_category);
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (catw->new_color_button),
                                &color);
    orage_category_write_entry(entry_category, &color);
    g_free(entry_category);
    refresh_categories(catw);
}

static void cat_color_button_changed(GtkColorButton *color_button
        , G_GNUC_UNUSED gpointer user_data)
{
    gchar *category;
    GdkRGBA color;

    category = g_object_get_data(G_OBJECT(color_button), "CATEGORY");
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (color_button), &color);
    orage_category_write_entry(category, &color);
}

static void cat_del_button_clicked(GtkButton *button, gpointer user_data)
{
    category_win_struct *catw = (category_win_struct *)user_data;
    gchar *category;

    category = g_object_get_data(G_OBJECT(button), "CATEGORY");
    orage_category_remove_entry(category);
    refresh_categories(catw);
}

static void show_category(const gpointer elem, gpointer user_data)
{
    const orage_category_struct *cat = (const orage_category_struct *)elem;
    category_win_struct *catw = (category_win_struct *)user_data;
    GtkWidget *label, *hbox, *button, *color_button;

    hbox = gtk_grid_new ();
    g_object_set (hbox, "column-spacing", CATEGORIES_SPACING,
                        NULL);

    label = gtk_label_new(cat->category);
    g_object_set (label, "xalign", 0.0, "yalign", 0.5,
                         "hexpand", TRUE,
                         NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);

    color_button = gtk_color_button_new_with_rgba (&cat->color);
    gtk_grid_attach_next_to (GTK_GRID (hbox), color_button, label,
                             GTK_POS_RIGHT, 1, 1);
    g_object_set_data_full(G_OBJECT(color_button), "CATEGORY"
            , g_strdup(cat->category), g_free);
    g_signal_connect((gpointer)color_button, "color-set"
            , G_CALLBACK(cat_color_button_changed), catw);

    button = orage_util_image_button ("list-remove", _("_Remove"));
    gtk_grid_attach_next_to (GTK_GRID (hbox), button, color_button,
                             GTK_POS_RIGHT, 1, 1);
    g_object_set_data_full(G_OBJECT(button), "CATEGORY"
            , g_strdup(cat->category), g_free);
    g_signal_connect((gpointer)button, "clicked"
            , G_CALLBACK(cat_del_button_clicked), catw);

    gtk_grid_attach_next_to (GTK_GRID (catw->cur_frame_vbox), hbox, NULL,
                             GTK_POS_BOTTOM, 1, 1);
}

static void refresh_categories(category_win_struct *catw)
{
    GtkWidget *swin;

    gtk_widget_destroy(catw->cur_frame);

    catw->cur_frame_vbox = gtk_grid_new ();
    g_object_set (catw->cur_frame_vbox, "row-spacing", CATEGORIES_SPACING,
                                        NULL);
    swin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin)
            , GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (swin), catw->cur_frame_vbox);
    catw->cur_frame = orage_create_framebox_with_content(
            _("Current categories"), GTK_SHADOW_IN, swin);
    g_object_set (catw->cur_frame, "margin-top", 5,
                                   "margin-bottom", 5,
                                   "vexpand", TRUE,
                                   "valign", GTK_ALIGN_FILL,
                                   NULL);

    gtk_grid_attach_next_to (GTK_GRID (catw->vbox), catw->cur_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    orage_category_get_list();
    g_list_foreach(orage_category_list, show_category, catw);
    gtk_widget_show_all(catw->cur_frame);
}

static void create_cat_win(category_win_struct *catw)
{
    GtkWidget *label, *hbox, *vbox;

    /***** New category *****/
    vbox = gtk_grid_new ();
    catw->new_frame = orage_create_framebox_with_content(
            _("Add new category with color"), GTK_SHADOW_NONE, vbox);
    g_object_set (catw->new_frame, "margin-top", 5,
                                   "margin-bottom", 5,
                                   NULL);
    gtk_grid_attach_next_to (GTK_GRID (catw->vbox), catw->new_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 10,
                        "margin-bottom", 10,
                        NULL);
    label = gtk_label_new(_("Category:"));
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL,
                             GTK_POS_RIGHT, 1, 1);
    catw->new_entry = gtk_entry_new ();
    g_object_set (catw->new_entry, "margin-right", 5,
                                   "margin-left", 5,
                                   "hexpand", TRUE,
                                   "halign", GTK_ALIGN_FILL,
                                   NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), catw->new_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);
    catw->new_color_button = gtk_color_button_new();
    g_object_set (catw->new_color_button, "margin-right", 10,
                                          NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), catw->new_color_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    catw->new_add_button = orage_util_image_button ("list-add", _("_Add"));
    gtk_grid_attach_next_to (GTK_GRID (hbox), catw->new_add_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL,
                             GTK_POS_RIGHT, 1, 1);
    g_signal_connect((gpointer)catw->new_add_button, "clicked"
            , G_CALLBACK(cat_add_button_clicked), catw);

    /***** Current categories *****/
    /* refresh_categories always destroys frame first, so let's create
     * a dummy for it for the first time */
    vbox = gtk_grid_new ();
    catw->cur_frame = orage_create_framebox_with_content ("dummy",
                                                          GTK_SHADOW_NONE,
                                                          vbox);
    refresh_categories(catw);
}

static void on_categories_button_clicked_cb (G_GNUC_UNUSED GtkWidget *button
        , gpointer *user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    category_win_struct *catw;

    catw = g_new(category_win_struct, 1);
    catw->apptw = (gpointer)apptw; /* remember the caller */
    catw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_modal(GTK_WINDOW(catw->window), TRUE);
    gtk_window_set_title(GTK_WINDOW(catw->window)
            , _("Colors of categories - Orage"));
    gtk_window_set_default_size(GTK_WINDOW(catw->window), 390, 360);

    catw->accelgroup = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(catw->window), catw->accelgroup);

    catw->vbox = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER(catw->window), catw->vbox);

    create_cat_win(catw);

    g_signal_connect((gpointer)catw->window, "delete_event",
        G_CALLBACK(on_cat_window_delete_event), catw);
    gtk_widget_show_all(catw->window);
}

/**********************************************************/
/* categories end.                                        */
/**********************************************************/

static void fill_appt_window_general(appt_win *apptw, xfical_appt *appt
        , const gchar *action)
{
    int i;

    /* type */
    if (appt->type == XFICAL_TYPE_EVENT)
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Type_event_rb), TRUE);
    else if (appt->type == XFICAL_TYPE_TODO)
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Type_todo_rb), TRUE);
    else if (appt->type == XFICAL_TYPE_JOURNAL)
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Type_journal_rb), TRUE);
    else
        g_warning ("%s: Illegal value for type", G_STRFUNC);

    /* appointment name */
    gtk_entry_set_text(GTK_ENTRY(apptw->Title_entry)
            , (appt->title ? appt->title : ""));

    if (strcmp(action, "COPY") == 0) {
        gtk_editable_set_position(GTK_EDITABLE(apptw->Title_entry), -1);
        i = gtk_editable_get_position(GTK_EDITABLE(apptw->Title_entry));
        gtk_editable_insert_text(GTK_EDITABLE(apptw->Title_entry)
                , _(" *** COPY ***"), strlen(_(" *** COPY ***")), &i);
    }

    /* location */
    gtk_entry_set_text(GTK_ENTRY(apptw->Location_entry)
            , (appt->location ? appt->location : ""));

    /* times */
    fill_appt_window_times(apptw, appt);

    /* availability */
    if (appt->availability != -1) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(apptw->Availability_cb)
               , appt->availability);
    }

    /* categories */
    fill_category_data(apptw, appt);

    /* priority */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->Priority_spin)
            , (gdouble)appt->priority);

    /* note */
    gtk_text_buffer_set_text(apptw->Note_buffer
            , (appt->note ? (const gchar *) appt->note : ""), -1);
}

static void fill_appt_window_alarm(appt_win *apptw, xfical_appt *appt)
{
    int day, hours, minutes;
    char *tmp;

    /* alarmtime (comes in seconds) */
    day = appt->alarmtime/(24*60*60);
    hours = (appt->alarmtime-day*(24*60*60))/(60*60);
    minutes = (appt->alarmtime-day*(24*60*60)-hours*(60*60))/(60);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->Alarm_spin_dd)
                , (gdouble)day);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->Alarm_spin_hh)
                , (gdouble)hours);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->Alarm_spin_mm)
                , (gdouble)minutes);

    /* alarmrelation */
    /*
    char *when_array[4] = {
    _("Before Start"), _("Before End"), _("After Start"), _("After End")};
    */
    if (appt->alarm_before)
        if (appt->alarm_related_start)
            gtk_combo_box_set_active(GTK_COMBO_BOX(apptw->Alarm_when_cb), 0);
        else
            gtk_combo_box_set_active(GTK_COMBO_BOX(apptw->Alarm_when_cb), 1);
    else
        if (appt->alarm_related_start)
            gtk_combo_box_set_active(GTK_COMBO_BOX(apptw->Alarm_when_cb), 2);
        else
            gtk_combo_box_set_active(GTK_COMBO_BOX(apptw->Alarm_when_cb), 3);

    /* persistent */
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(apptw->Per_checkbutton), appt->alarm_persistent);

    /* sound */
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(apptw->Sound_checkbutton), appt->sound_alarm);
    gtk_entry_set_text(GTK_ENTRY(apptw->Sound_entry)
            , (appt->sound ? appt->sound : 
                PACKAGE_DATA_DIR "/orage/sounds/Spo.wav"));

    /* sound repeat */
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(apptw->SoundRepeat_checkbutton)
                    , appt->soundrepeat);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->SoundRepeat_spin_cnt)
            , (gdouble)appt->soundrepeat_cnt);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->SoundRepeat_spin_len)
            , (gdouble)appt->soundrepeat_len);

    /* display */
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(apptw->Display_checkbutton_orage)
                    , appt->display_alarm_orage);
    /* display:notify */
#ifdef HAVE_NOTIFY
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(apptw->Display_checkbutton_notify)
                    , appt->display_alarm_notify);
    if (!appt->display_alarm_notify || appt->display_notify_timeout == -1) { 
        /* no timeout */
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Display_checkbutton_expire_notify)
                        , FALSE);
        gtk_spin_button_set_value(
                GTK_SPIN_BUTTON(apptw->Display_spin_expire_notify)
                , (gdouble)0);
    }
    else {
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Display_checkbutton_expire_notify)
                        , TRUE);
        gtk_spin_button_set_value(
                GTK_SPIN_BUTTON(apptw->Display_spin_expire_notify)
                , (gdouble)appt->display_notify_timeout);
    }
#endif

    /* procedure */
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(apptw->Proc_checkbutton), appt->procedure_alarm);
    tmp = g_strconcat(appt->procedure_cmd, " ", appt->procedure_params, NULL);
    gtk_entry_set_text(GTK_ENTRY(apptw->Proc_entry), tmp ? tmp : "");
    g_free(tmp);
}

static void fill_appt_window_recurrence(appt_win *apptw, xfical_appt *appt)
{
    gchar *untildate_to_display;
    gchar *text;
    gdouble recur_count;
    GList *tmp;
    GDateTime *gdt;
    xfical_exception *recur_exception;
    int i;

    gtk_combo_box_set_active(GTK_COMBO_BOX(apptw->Recur_freq_cb), appt->freq);
    switch(appt->recur_limit) {
        case 0: /* no limit */
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(apptw->Recur_limit_rb), TRUE);
            recur_count = 1;
            gdt = g_date_time_new_now_local ();
            break;
        case 1: /* count */
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(apptw->Recur_count_rb), TRUE);
            recur_count = appt->recur_count;
            gdt = g_date_time_new_now_local ();
            break;
        case 2: /* until */
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(apptw->Recur_until_rb), TRUE);
            recur_count = 1;
            gdt = orage_icaltime_to_gdatetime (appt->recur_until, FALSE);
            break;
        default: /* error */
            g_error ("%s: Unsupported recur_limit %d", G_STRFUNC,
                      appt->recur_limit);
    }

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->Recur_count_spin),
                               recur_count);
    untildate_to_display = g_date_time_format (gdt, "%x");
    g_object_set_data_full (G_OBJECT (apptw->Recur_until_button),
                            DATE_KEY, gdt,
                            (GDestroyNotify)g_date_time_unref);
    gtk_button_set_label (GTK_BUTTON(apptw->Recur_until_button),
                          untildate_to_display);
    g_free (untildate_to_display);

    /* weekdays */
    for (i=0; i <= 6; i++) {
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_byday_cb[i])
                        , appt->recur_byday[i]);
        /* which weekday of month / year */
        gtk_spin_button_set_value(
                GTK_SPIN_BUTTON(apptw->Recur_byday_spin[i])
                        , (gdouble)appt->recur_byday_cnt[i]);
    }

    /* interval */
    gtk_spin_button_set_value(
            GTK_SPIN_BUTTON(apptw->Recur_int_spin)
                    , (gdouble)appt->interval);

    if (!appt->recur_byday[0] || !appt->recur_byday[1] || !appt->recur_byday[2] 
    ||  !appt->recur_byday[3] || !appt->recur_byday[4] || !appt->recur_byday[5] 
    ||  !appt->recur_byday[6]) {
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_feature_advanced_rb), TRUE);
    }
    else {
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_feature_normal_rb), TRUE);
    }

    /* todo base */
    if (appt->recur_todo_base_start)
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_todo_base_start_rb), TRUE);
    else
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->Recur_todo_base_done_rb), TRUE);

    /* exceptions */
    for (tmp = g_list_first(appt->recur_exceptions);
         tmp != NULL;
         tmp = g_list_next(tmp)) {
        recur_exception = (xfical_exception *)tmp->data;
        text = orage_icaltime_to_i18_time (recur_exception->time);
        add_recur_exception_row(text, recur_exception->type, apptw, TRUE);
        g_free(text);
    }
    /* note: include times is setup in the fill_appt_window_times */
}

/* Fill appointment window with data */
static gboolean fill_appt_window(appt_win *apptw, const gchar *action,
                                 const gchar *par, GDateTime *gdt_par)
{
    xfical_appt *appt;
    GDateTime *gdt;
    gchar *time_str;

    /********************* INIT *********************/
    g_message ("%s appointment: %s", action, par);
    if ((appt = fill_appt_window_get_appt (apptw, action, par, gdt_par)) == NULL) {
        return(FALSE);
    }
    apptw->xf_appt = appt;

    /* first flags */
    apptw->xf_uid = g_strdup(appt->uid);
    apptw->par = g_strdup (par);
    g_date_time_unref (apptw->par2);
    apptw->par2 = g_date_time_ref (gdt_par);
    apptw->appointment_changed = FALSE;
    if (strcmp(action, "NEW") == 0) {
        apptw->appointment_add = TRUE;
        apptw->appointment_new = TRUE;
    }
    else if (strcmp(action, "UPDATE") == 0) {
        apptw->appointment_add = FALSE;
        apptw->appointment_new = FALSE;
    }
    else if (strcmp(action, "COPY") == 0) {
        /* COPY uses old uid as base and adds new, so
         * add == TRUE && new == FALSE */
        apptw->appointment_add = TRUE;
        apptw->appointment_new = FALSE;
        /* new copy is never readonly even though the original may have been */
        appt->readonly = FALSE;
    }
    else {
        g_free(appt);
        apptw->xf_appt = NULL;
        g_error ("%s: unknown parameter '%s'", G_STRFUNC, action);
        return(FALSE);
    }
    if (apptw->appointment_add) {
        add_file_select_cb(apptw);
    }
    if (!appt->completed) { /* some nice default */
        gdt = g_date_time_new_now_local (); /* probably completed today? */
        time_str = g_date_time_format (gdt, XFICAL_APPT_TIME_FORMAT_S0);
        g_date_time_unref (gdt);
        g_strlcpy (appt->completedtime, time_str, sizeof (appt->completedtime));
        g_free (time_str);
        g_free(appt->completed_tz_loc);
        appt->completed_tz_loc = g_strdup(appt->start_tz_loc);
    }
    /* we only want to enable duplication if we are working with an old
     * existing app (=not adding new) */
    gtk_widget_set_sensitive(apptw->Duplicate, !apptw->appointment_add);
    gtk_widget_set_sensitive(apptw->File_menu_duplicate
            , !apptw->appointment_add);

    /* window title */
    gtk_window_set_title(GTK_WINDOW(apptw->Window)
            , _("New appointment - Orage"));

    /********************* GENERAL tab *********************/
    fill_appt_window_general(apptw, appt, action);

    /********************* ALARM tab *********************/
    fill_appt_window_alarm(apptw, appt);

    /********************* RECURRENCE tab *********************/
    fill_appt_window_recurrence(apptw, appt);

    /********************* FINALIZE *********************/
    set_time_sensitivity(apptw);
    set_repeat_sensitivity(apptw);
    set_sound_sensitivity(apptw);
    set_notify_sensitivity(apptw);
    set_proc_sensitivity(apptw);
    mark_appointment_unchanged(apptw);
    return(TRUE);
}

static void build_menu(appt_win *apptw)
{
    /* Menu bar */
    apptw->Menubar = gtk_menu_bar_new();
    gtk_grid_attach (GTK_GRID (apptw->Vbox), apptw->Menubar, 1, 1, 1, 1);

    /* File menu stuff */
    apptw->File_menu = orage_menu_new(_("_File"), apptw->Menubar);

    apptw->File_menu_save = orage_image_menu_item_new_from_stock("gtk-save"
            , apptw->File_menu, apptw->accel_group);

    apptw->File_menu_saveclose = 
            orage_menu_item_new_with_mnemonic(_("Sav_e and close")
                    , apptw->File_menu);
    gtk_widget_add_accelerator(apptw->File_menu_saveclose
            , "activate", apptw->accel_group
            , GDK_KEY_w, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

    (void)orage_separator_menu_item_new(apptw->File_menu);

    apptw->File_menu_revert = 
            orage_image_menu_item_new_from_stock("gtk-revert-to-saved"
                    , apptw->File_menu, apptw->accel_group);

    apptw->File_menu_duplicate = 
            orage_menu_item_new_with_mnemonic(_("D_uplicate")
                    , apptw->File_menu);
    gtk_widget_add_accelerator(apptw->File_menu_duplicate
            , "activate", apptw->accel_group
            , GDK_KEY_d, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    (void)orage_separator_menu_item_new(apptw->File_menu);

    apptw->File_menu_delete = orage_image_menu_item_new_from_stock("gtk-delete"
            , apptw->File_menu, apptw->accel_group);

    (void)orage_separator_menu_item_new(apptw->File_menu);

    apptw->File_menu_close = orage_image_menu_item_new_from_stock("gtk-close"
            , apptw->File_menu, apptw->accel_group);

    g_signal_connect((gpointer)apptw->File_menu_save, "activate"
            , G_CALLBACK(on_appFileSave_menu_activate_cb), apptw);
    g_signal_connect((gpointer)apptw->File_menu_saveclose, "activate"
            , G_CALLBACK(on_appFileSaveClose_menu_activate_cb), apptw);
    g_signal_connect((gpointer)apptw->File_menu_duplicate, "activate"
            , G_CALLBACK(on_appFileDuplicate_menu_activate_cb), apptw);
    g_signal_connect((gpointer)apptw->File_menu_revert, "activate"
            , G_CALLBACK(on_appFileRevert_menu_activate_cb), apptw);
    g_signal_connect((gpointer)apptw->File_menu_delete, "activate"
            , G_CALLBACK(on_appFileDelete_menu_activate_cb), apptw);
    g_signal_connect((gpointer)apptw->File_menu_close, "activate"
            , G_CALLBACK(on_appFileClose_menu_activate_cb), apptw);
}

static OrageRc *orage_alarm_file_open(gboolean read_only)
{
    gchar *fpath;
    OrageRc *orc;

    fpath = orage_config_file_location(ORAGE_DEFAULT_ALARM_DIR_FILE);
    if (!read_only)  /* we need to empty it before each write */
        if (g_remove(fpath)) {
            g_warning ("%s: g_remove failed.", G_STRFUNC);
        }
    if ((orc = orage_rc_file_open(fpath, read_only)) == NULL) {
        g_warning ("%s: default alarm file open failed.", G_STRFUNC);
    }
    g_free(fpath);

    return(orc);
}

static void store_default_alarm(xfical_appt *appt)
{
    OrageRc *orc;

    orc = orage_alarm_file_open(FALSE);

    orage_rc_set_group(orc, "DEFAULT ALARM");
    orage_rc_put_int(orc, "TIME", appt->alarmtime);
    orage_rc_put_bool(orc, "BEFORE", appt->alarm_before);
    orage_rc_put_bool(orc, "RELATED_START", appt->alarm_related_start);
    orage_rc_put_bool(orc, "PERSISTENT", appt->alarm_persistent);
    orage_rc_put_bool(orc, "SOUND_USE", appt->sound_alarm);
    orage_rc_put_str(orc, "SOUND", appt->sound);
    orage_rc_put_bool(orc, "SOUND_REPEAT_USE", appt->soundrepeat);
    orage_rc_put_int(orc, "SOUND_REPEAT_CNT", appt->soundrepeat_cnt);
    orage_rc_put_int(orc, "SOUND_REPEAT_LEN", appt->soundrepeat_len);
    orage_rc_put_bool(orc, "DISPLAY_ORAGE_USE", appt->display_alarm_orage);
    orage_rc_put_bool(orc, "DISPLAY_NOTIFY_USE", appt->display_alarm_notify);
    orage_rc_put_int(orc, "DISPLAY_NOTIFY_TIMEOUT"
            , appt->display_notify_timeout);
    orage_rc_put_bool(orc, "PROCEDURE_USE", appt->procedure_alarm);
    orage_rc_put_str(orc, "PROCEDURE_CMD", appt->procedure_cmd);
    orage_rc_put_str(orc, "PROCEDURE_PARAMS", appt->procedure_params);
    orage_rc_file_close(orc);
}

static void read_default_alarm(xfical_appt *appt)
{
    OrageRc *orc;

    orc = orage_alarm_file_open(TRUE);

    orage_rc_set_group(orc, "DEFAULT ALARM");
    appt->alarmtime = orage_rc_get_int(orc, "TIME", 5*60); /* 5 mins */
    appt->alarm_before = orage_rc_get_bool(orc, "BEFORE", TRUE);
    appt->alarm_related_start = orage_rc_get_bool(orc, "RELATED_START", TRUE);
    appt->alarm_persistent = orage_rc_get_bool(orc, "PERSISTENT", FALSE);
    appt->sound_alarm = orage_rc_get_bool(orc, "SOUND_USE", TRUE);
    if (appt->sound)
        g_free(appt->sound);
    appt->sound = orage_rc_get_str(orc, "SOUND"
            , PACKAGE_DATA_DIR "/orage/sounds/Spo.wav");
    appt->soundrepeat = orage_rc_get_bool(orc, "SOUND_REPEAT_USE", FALSE);
    appt->soundrepeat_cnt = orage_rc_get_int(orc, "SOUND_REPEAT_CNT", 500);
    appt->soundrepeat_len = orage_rc_get_int(orc, "SOUND_REPEAT_LEN", 2);
    appt->display_alarm_orage = orage_rc_get_bool(orc, "DISPLAY_ORAGE_USE"
            , TRUE);
    appt->display_alarm_notify = orage_rc_get_bool(orc, "DISPLAY_NOTIFY_USE"
            , FALSE);
    appt->display_notify_timeout = orage_rc_get_int(orc
            , "DISPLAY_NOTIFY_TIMEOUT", 0);
    appt->procedure_alarm = orage_rc_get_bool(orc, "PROCEDURE_USE", FALSE);
    if (appt->procedure_cmd)
        g_free(appt->procedure_cmd);
    appt->procedure_cmd = orage_rc_get_str(orc, "PROCEDURE_CMD", "");
    if (appt->procedure_params)
        g_free(appt->procedure_params);
    appt->procedure_params = orage_rc_get_str(orc, "PROCEDURE_PARAMS", "");
    orage_rc_file_close(orc);
}

static gboolean is_same_date (GDateTime *gdt1, GDateTime *gdt2)
{
    if (g_date_time_get_year (gdt1) != g_date_time_get_year (gdt2))
        return FALSE;

    if (g_date_time_get_day_of_year (gdt1) != g_date_time_get_day_of_year (gdt2))
        return FALSE;

    return TRUE;
}

static gchar *create_action_time (xfical_appt *appt)
{
    gchar *action_time;
    gchar *fmt;
    gchar *start_str;
    gchar *end_str;

    if (g_date_time_compare (appt->starttime2, appt->endtime2) == 0)
    {
        action_time = g_date_time_format (appt->starttime2, "%x %R");
        return action_time;
    }

    if (appt->allDay == TRUE)
    {
        fmt = "%x";

        if (is_same_date (appt->starttime2, appt->endtime2) == TRUE)
        {
            action_time = g_date_time_format (appt->starttime2, fmt);
            return action_time;
        }
    }
    else
        fmt = "%x %R";

    start_str = g_date_time_format (appt->starttime2, fmt);
    end_str = g_date_time_format (appt->endtime2, fmt);

    action_time = g_strconcat (start_str, " - ", end_str, NULL);
    g_free (start_str);
    g_free (end_str);

    return action_time;
}

static void on_test_button_clicked_cb (G_GNUC_UNUSED GtkButton *button
        , gpointer user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    xfical_appt *appt = (xfical_appt *)apptw->xf_appt;
    alarm_struct cur_alarm;

    fill_appt_from_apptw(appt, apptw);

    /* no need for alarm time as we are doing this now */
    cur_alarm.alarm_time = NULL;
    if (appt->uid)
        cur_alarm.uid = g_strdup(appt->uid);
    else
        cur_alarm.uid = NULL;

    cur_alarm.action_time = create_action_time (appt);

    cur_alarm.title = g_strdup(appt->title);
    cur_alarm.description = g_strdup(appt->note);
    cur_alarm.persistent = appt->alarm_persistent;
    cur_alarm.display_orage = appt->display_alarm_orage;
    cur_alarm.display_notify = appt->display_alarm_notify;
    cur_alarm.notify_refresh = TRUE; /* not needed ? */
    cur_alarm.notify_timeout = appt->display_notify_timeout;
    cur_alarm.audio = appt->sound_alarm;
    if (appt->sound)
        cur_alarm.sound = g_strdup(appt->sound);
    else 
        cur_alarm.sound = NULL;
    cur_alarm.repeat_cnt = appt->soundrepeat_cnt;
    cur_alarm.repeat_delay = appt->soundrepeat_len;
    cur_alarm.procedure = appt->procedure_alarm;
    if (appt->procedure_alarm)
        cur_alarm.cmd = g_strconcat(appt->procedure_cmd, " "
                , appt->procedure_params, NULL);
    else
        cur_alarm.cmd = NULL;
    create_reminders(&cur_alarm);
    g_free(cur_alarm.uid);
    g_free(cur_alarm.action_time);
    g_free(cur_alarm.title);
    g_free(cur_alarm.description);
    g_free(cur_alarm.sound);
    g_free(cur_alarm.cmd);
}

static void on_appDefault_save_button_clicked_cb (
    G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    xfical_appt *appt = (xfical_appt *)apptw->xf_appt;

    fill_appt_from_apptw_alarm(appt, apptw);
    store_default_alarm(appt);
}

static void on_appDefault_read_button_clicked_cb (
    G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
    appt_win *apptw = (appt_win *)user_data;
    xfical_appt *appt = (xfical_appt *)apptw->xf_appt;

    read_default_alarm(appt);
    fill_appt_window_alarm(apptw, appt);
}

static void build_toolbar(appt_win *apptw)
{
    gint i = 0;

    apptw->Toolbar = gtk_toolbar_new();
    gtk_grid_attach (GTK_GRID (apptw->Vbox), apptw->Toolbar, 1, 2, 1, 1);

    /* Add buttons to the toolbar */
    apptw->Save = orage_toolbar_append_button(apptw->Toolbar
            , "document-save", _("Save"), i++);
    apptw->SaveClose = orage_toolbar_append_button(apptw->Toolbar
            , "window-close", _("Save and close"), i++);

    (void)orage_toolbar_append_separator(apptw->Toolbar, i++);

    apptw->Revert = orage_toolbar_append_button(apptw->Toolbar
            , "document-revert", _("Revert"), i++);
    apptw->Duplicate = orage_toolbar_append_button(apptw->Toolbar
            , "edit-copy", _("Duplicate"), i++);

    (void)orage_toolbar_append_separator(apptw->Toolbar, i++);

    apptw->Delete = orage_toolbar_append_button(apptw->Toolbar
            , "edit-delete", _("Delete"), i++);

    g_signal_connect((gpointer)apptw->Save, "clicked"
            , G_CALLBACK(on_appSave_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->SaveClose, "clicked"
            , G_CALLBACK(on_appSaveClose_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Revert, "clicked"
            , G_CALLBACK(on_appRevert_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Duplicate, "clicked"
            , G_CALLBACK(on_appDuplicate_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Delete, "clicked"
            , G_CALLBACK(on_appDelete_clicked_cb), apptw);
}

static void build_general_page(appt_win *apptw)
{
    gint row;
    GtkWidget *label, *event, *hbox;
    const gchar *availability_array[2] = {_("Free"), _("Busy")};

    apptw->TableGeneral = orage_table_new (BORDER_SIZE);
    apptw->General_notebook_page = apptw->TableGeneral;
    apptw->General_tab_label = gtk_label_new(_("General"));

    gtk_notebook_append_page(GTK_NOTEBOOK(apptw->Notebook)
            , apptw->General_notebook_page, apptw->General_tab_label);

    /* type */
    apptw->Type_label = gtk_label_new(_("Type "));
    hbox =  gtk_grid_new ();
    apptw->Type_event_rb = gtk_radio_button_new_with_label(NULL, _("Event"));
    g_object_set (apptw->Type_event_rb, "margin", 15, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), apptw->Type_event_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Type_event_rb
            , _("Event that will happen sometime. For example:\nMeeting or birthday or TV show."));

    apptw->Type_todo_rb = gtk_radio_button_new_with_mnemonic_from_widget(
            GTK_RADIO_BUTTON(apptw->Type_event_rb) , _("Todo"));
    g_object_set (apptw->Type_todo_rb, "margin", 15, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), apptw->Type_todo_rb,
                             apptw->Type_event_rb, GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Type_todo_rb
            , _("Something that you should do sometime. For example:\nWash your car or test new version of Orage."));

    apptw->Type_journal_rb = gtk_radio_button_new_with_mnemonic_from_widget(
            GTK_RADIO_BUTTON(apptw->Type_event_rb) , _("Journal"));
    g_object_set (apptw->Type_journal_rb, "margin", 15, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), apptw->Type_journal_rb,
                             apptw->Type_todo_rb, GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Type_journal_rb
            , _("Make a note that something happened. For example:\nRemark that your mother called or first snow came."));

    orage_table_add_row(apptw->TableGeneral
            , apptw->Type_label, hbox
            , row = 0, (GTK_EXPAND | GTK_FILL), (0));

    /* title */
    apptw->Title_label = gtk_label_new(_("Title "));
    apptw->Title_entry = gtk_entry_new();
    orage_table_add_row(apptw->TableGeneral
            , apptw->Title_label, apptw->Title_entry
            , ++row, (GTK_EXPAND | GTK_FILL), (0));

    /* location */
    apptw->Location_label = gtk_label_new(_("Location"));
    apptw->Location_entry = gtk_entry_new();
    orage_table_add_row(apptw->TableGeneral
            , apptw->Location_label, apptw->Location_entry
            , ++row, (GTK_EXPAND | GTK_FILL), (0));

    /* All day */
    apptw->AllDay_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("All day event"));
    orage_table_add_row(apptw->TableGeneral
            , NULL, apptw->AllDay_checkbutton
            , ++row, (GTK_EXPAND | GTK_FILL), (0));

    /* start time */
    apptw->Start_label = gtk_label_new(_("Start"));
    apptw->StartDate_button = gtk_button_new();
    apptw->StartTime_spin_hh = gtk_spin_button_new_with_range(0, 23, 1);
    apptw->StartTime_spin_mm = gtk_spin_button_new_with_range(0, 59, 1);
    apptw->StartTimezone_button = gtk_button_new();
    apptw->StartTime_hbox = datetime_hbox_new(
            apptw->StartDate_button, 
            apptw->StartTime_spin_hh, 
            apptw->StartTime_spin_mm, 
            apptw->StartTimezone_button);
    orage_table_add_row(apptw->TableGeneral
            , apptw->Start_label, apptw->StartTime_hbox
            , ++row, (GTK_SHRINK | GTK_FILL), (GTK_SHRINK | GTK_FILL));

    /* end time */
    apptw->End_label = gtk_label_new(_("End"));
    apptw->End_hbox = gtk_grid_new ();
    /* translators: leave some spaces after this so that it looks better on
     * the screen */
    apptw->End_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("Set      "));
    gtk_grid_attach_next_to (GTK_GRID (apptw->End_hbox),
            apptw->End_checkbutton, NULL, GTK_POS_RIGHT, 1, 1);
    apptw->EndDate_button = gtk_button_new();
    apptw->EndTime_spin_hh = gtk_spin_button_new_with_range(0, 23, 1);
    apptw->EndTime_spin_mm = gtk_spin_button_new_with_range(0, 59, 1);
    apptw->EndTimezone_button = gtk_button_new();
    apptw->EndTime_hbox = datetime_hbox_new(
            apptw->EndDate_button, 
            apptw->EndTime_spin_hh, 
            apptw->EndTime_spin_mm, 
            apptw->EndTimezone_button);
    g_object_set (apptw->EndTime_hbox, "hexpand", TRUE,
                                       "halign", GTK_ALIGN_FILL, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->End_hbox),
            apptw->EndTime_hbox, NULL, GTK_POS_RIGHT, 1, 1);
    orage_table_add_row(apptw->TableGeneral
            , apptw->End_label, apptw->End_hbox
            , ++row, (GTK_SHRINK | GTK_FILL), (GTK_SHRINK | GTK_FILL));

    /* duration */
    apptw->Dur_hbox = gtk_grid_new ();
    apptw->Dur_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("Duration"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Dur_hbox),
            apptw->Dur_checkbutton, NULL, GTK_POS_RIGHT, 1, 1);
    apptw->Dur_spin_dd = gtk_spin_button_new_with_range(0, 1000, 1);
    apptw->Dur_spin_dd_label = gtk_label_new(_("days"));
    apptw->Dur_spin_hh = gtk_spin_button_new_with_range(0, 23, 1);
    apptw->Dur_spin_hh_label = gtk_label_new(_("hours"));
    apptw->Dur_spin_mm = gtk_spin_button_new_with_range(0, 59, 5);
    apptw->Dur_spin_mm_label = gtk_label_new(_("mins"));
    apptw->Dur_time_hbox = orage_period_hbox_new(TRUE, FALSE
            , apptw->Dur_spin_dd, apptw->Dur_spin_dd_label
            , apptw->Dur_spin_hh, apptw->Dur_spin_hh_label
            , apptw->Dur_spin_mm, apptw->Dur_spin_mm_label);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Dur_hbox),
            apptw->Dur_time_hbox, NULL, GTK_POS_RIGHT, 1, 1);
    orage_table_add_row(apptw->TableGeneral
            , NULL, apptw->Dur_hbox
            , ++row, (GTK_FILL), (GTK_FILL));
    
    /* Availability (only for EVENT) */
    apptw->Availability_label = gtk_label_new(_("Availability"));
    apptw->Availability_cb = orage_create_combo_box_with_content(
            availability_array, 2);
    orage_table_add_row(apptw->TableGeneral
            , apptw->Availability_label, apptw->Availability_cb
            , ++row, (GTK_FILL), (GTK_FILL));
    
    /* completed (only for TODO) */
    apptw->Completed_label = gtk_label_new(_("Completed"));
    apptw->Completed_hbox = gtk_grid_new ();
    apptw->Completed_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("Done"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Completed_hbox),
            apptw->Completed_checkbutton, NULL, GTK_POS_RIGHT, 1, 1);
    label = gtk_label_new(" ");
    g_object_set (label, "margin", 4, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Completed_hbox),
            label, NULL, GTK_POS_RIGHT, 1, 1);
    apptw->CompletedDate_button = gtk_button_new();
    apptw->CompletedTime_spin_hh = gtk_spin_button_new_with_range(0, 23, 1);
    apptw->CompletedTime_spin_mm = gtk_spin_button_new_with_range(0, 59, 1);
    apptw->CompletedTimezone_button = gtk_button_new();
    apptw->CompletedTime_hbox = datetime_hbox_new(
            apptw->CompletedDate_button, 
            apptw->CompletedTime_spin_hh, 
            apptw->CompletedTime_spin_mm, 
            apptw->CompletedTimezone_button);
    g_object_set (apptw->CompletedTime_hbox, "hexpand", TRUE,
                                             "halign", GTK_ALIGN_FILL, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Completed_hbox),
            apptw->CompletedTime_hbox, NULL, GTK_POS_RIGHT, 1, 1);
    orage_table_add_row(apptw->TableGeneral
            , apptw->Completed_label, apptw->Completed_hbox
            , ++row, (GTK_FILL), (GTK_FILL));

    /* categories */
    apptw->Categories_label = gtk_label_new(_("Categories"));
    apptw->Categories_hbox = gtk_grid_new ();
    apptw->Categories_entry = gtk_entry_new();
    g_object_set (apptw->Categories_entry, "hexpand", TRUE,
                                           "halign", GTK_ALIGN_FILL, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Categories_hbox),
                             apptw->Categories_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Categories_cb = gtk_combo_box_text_new();
    g_object_set (apptw->Categories_cb, "margin-left", 4,
                                        "margin-right", 4, NULL);
    apptw->Categories_cb_event =  gtk_event_box_new(); /* needed for tooltips */
    gtk_container_add (GTK_CONTAINER(apptw->Categories_cb_event),
                       apptw->Categories_cb);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Categories_hbox),
                             apptw->Categories_cb_event, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Categories_cb_event
            , _("This is special category, which can be used to color this appointment in list views."));
    apptw->Categories_button = gtk_button_new_with_mnemonic (_("_Colour"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Categories_hbox),
                             apptw->Categories_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Categories_button
            , _("update colors for categories."));
    orage_table_add_row(apptw->TableGeneral
            , apptw->Categories_label, apptw->Categories_hbox
            , ++row, (GTK_EXPAND | GTK_FILL), (0));

    /* priority */
    apptw->Priority_label = gtk_label_new(_("Priority"));
    apptw->Priority_spin = gtk_spin_button_new_with_range(0, 9, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(apptw->Priority_spin), TRUE);
    gtk_widget_set_tooltip_text(apptw->Priority_spin
            , _("If you set this 8 or bigger, the appointment is NOT shown on list windows.\nYou can use that feature to unclutter your list windows, but it makes it more difficult to find this appointment.\n(Alarms will still fire normally.)\n(There is undocumented parameter so that you can change the default limit of 8.)"));
    hbox = gtk_grid_new ();
    gtk_grid_attach_next_to (GTK_GRID (hbox), apptw->Priority_spin, NULL,
                             GTK_POS_RIGHT, 1, 1);
    orage_table_add_row(apptw->TableGeneral
            , apptw->Priority_label, hbox
            , ++row, (GTK_FILL), (GTK_FILL));

    /* note */
    apptw->Note = gtk_label_new(_("Note"));
    apptw->Note_Scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    event =  gtk_event_box_new(); /* only needed for tooltips */
    gtk_container_add(GTK_CONTAINER(event), apptw->Note_Scrolledwindow);
    gtk_scrolled_window_set_policy(
            GTK_SCROLLED_WINDOW(apptw->Note_Scrolledwindow)
            , GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(
            GTK_SCROLLED_WINDOW(apptw->Note_Scrolledwindow), GTK_SHADOW_IN);
    apptw->Note_buffer = gtk_text_buffer_new(NULL);
    apptw->Note_textview = 
            gtk_text_view_new_with_buffer(GTK_TEXT_BUFFER(apptw->Note_buffer));
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(apptw->Note_textview)
            , GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(apptw->Note_Scrolledwindow)
            , apptw->Note_textview);
    orage_table_add_row(apptw->TableGeneral
            , apptw->Note, event
            , ++row, (GTK_EXPAND | GTK_FILL), (GTK_EXPAND | GTK_FILL));

    gtk_widget_set_tooltip_text(event
            , _("These shorthand commands take effect immediately:\n    <D> inserts current date in local date format\n    <T> inserts time and\n    <DT> inserts date and time.\n\nThese are converted only later when they are seen:\n    <&Ynnnn> is translated to current year minus nnnn.\n(This can be used for example in birthday reminders to tell how old the person will be.)"));

    /* Take care of the title entry to build the appointment window title */
    g_signal_connect((gpointer)apptw->Title_entry, "changed"
            , G_CALLBACK(on_appTitle_entry_changed_cb), apptw);
}

static void enable_general_page_signals(appt_win *apptw)
{
    g_signal_connect((gpointer)apptw->Type_event_rb, "clicked"
            , G_CALLBACK(app_type_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Type_todo_rb, "clicked"
            , G_CALLBACK(app_type_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Type_journal_rb, "clicked"
            , G_CALLBACK(app_type_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->AllDay_checkbutton, "clicked"
            , G_CALLBACK(app_time_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->StartDate_button, "clicked"
            , G_CALLBACK(on_Date_button_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->StartTime_spin_hh, "changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->StartTime_spin_mm, "changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->StartTimezone_button, "clicked"
            , G_CALLBACK(on_appStartTimezone_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->End_checkbutton, "clicked"
            , G_CALLBACK(app_time_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->EndDate_button, "clicked"
            , G_CALLBACK(on_Date_button_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->EndTime_spin_hh, "changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->EndTime_spin_mm, "changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->EndTimezone_button, "clicked"
            , G_CALLBACK(on_appEndTimezone_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Dur_checkbutton, "clicked"
            , G_CALLBACK(app_time_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Dur_spin_dd, "value-changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Dur_spin_hh, "value-changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Dur_spin_mm, "value-changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Completed_checkbutton, "clicked"
            , G_CALLBACK(app_time_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->CompletedDate_button, "clicked"
            , G_CALLBACK(on_Date_button_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->CompletedTime_spin_hh, "changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->CompletedTime_spin_mm, "changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->CompletedTimezone_button, "clicked"
            , G_CALLBACK(on_appCompletedTimezone_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Location_entry, "changed"
            , G_CALLBACK(on_app_entry_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Categories_entry, "changed"
            , G_CALLBACK(on_app_entry_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Categories_cb, "changed"
            , G_CALLBACK(on_app_combobox_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Categories_button, "clicked"
            , G_CALLBACK(on_categories_button_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Availability_cb, "changed"
            , G_CALLBACK(on_app_combobox_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Note_buffer, "changed"
            , G_CALLBACK(on_appNote_buffer_changed_cb), apptw);
}

static void build_alarm_page(appt_win *apptw)
{
    gint row;
    GtkWidget *event, *vbox;
    GtkWidget *separator;
    GtkWidget *padding;
    const gchar *when_array[4] = {_("Before Start"), _("Before End")
        , _("After Start"), _("After End")};

    /***** Header *****/
    vbox = gtk_grid_new ();
    apptw->TableAlarm = orage_table_new (BORDER_SIZE);
    gtk_grid_attach_next_to (GTK_GRID (vbox), apptw->TableAlarm, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    apptw->Alarm_notebook_page = vbox;
    apptw->Alarm_tab_label = gtk_label_new(_("Alarm"));

    gtk_notebook_append_page(GTK_NOTEBOOK(apptw->Notebook)
            , apptw->Alarm_notebook_page, apptw->Alarm_tab_label);

    /***** ALARM TIME *****/
    apptw->Alarm_label = gtk_label_new(_("Alarm time"));
    apptw->Alarm_hbox = gtk_grid_new ();
    apptw->Alarm_spin_dd = gtk_spin_button_new_with_range(0, 100, 1);
    apptw->Alarm_spin_dd_label = gtk_label_new(_("days"));
    apptw->Alarm_spin_hh = gtk_spin_button_new_with_range(0, 23, 1);
    apptw->Alarm_spin_hh_label = gtk_label_new(_("hours"));
    apptw->Alarm_spin_mm = gtk_spin_button_new_with_range(0, 59, 5);
    apptw->Alarm_spin_mm_label = gtk_label_new(_("mins"));
    apptw->Alarm_time_hbox = orage_period_hbox_new(FALSE, TRUE
            , apptw->Alarm_spin_dd, apptw->Alarm_spin_dd_label
            , apptw->Alarm_spin_hh, apptw->Alarm_spin_hh_label
            , apptw->Alarm_spin_mm, apptw->Alarm_spin_mm_label);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Alarm_hbox),
                             apptw->Alarm_time_hbox, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Alarm_when_cb = orage_create_combo_box_with_content(when_array, 4);
    event =  gtk_event_box_new(); /* only needed for tooltips */
    gtk_container_add(GTK_CONTAINER(event), apptw->Alarm_when_cb);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Alarm_hbox), event, NULL,
                             GTK_POS_RIGHT, 1, 1);

    orage_table_add_row(apptw->TableAlarm
            , apptw->Alarm_label, apptw->Alarm_hbox
            , row = 0, (GTK_FILL), (GTK_FILL));
    gtk_widget_set_tooltip_text(event
            , _("Often you want to get alarm:\n 1) before Event start\n 2) before Todo end\n 3) after Todo start"));

    /***** Persistent Alarm *****/
    apptw->Per_hbox = gtk_grid_new ();
    apptw->Per_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("Persistent alarm"));
    g_object_set (apptw->Per_checkbutton, "margin-top", 6,
                                          "margin-bottom", 6,
                                          "hexpand", TRUE, NULL);
    gtk_widget_set_tooltip_text(apptw->Per_checkbutton
            , _("Select this if you want Orage to remind you even if it has not been active when the alarm happened."));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Per_hbox), apptw->Per_checkbutton,
                             NULL, GTK_POS_RIGHT, 1, 1);

    orage_table_add_row(apptw->TableAlarm
            , NULL, apptw->Per_hbox
            , ++row, (GTK_FILL), (GTK_FILL));

    /***** Audio Alarm *****/
    apptw->Sound_label = gtk_label_new(_("Sound"));

    apptw->Sound_hbox = gtk_grid_new ();
    apptw->Sound_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("Use"));
    gtk_widget_set_tooltip_text(apptw->Sound_checkbutton
            , _("Select this if you want audible alarm"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Sound_hbox),
                             apptw->Sound_checkbutton, NULL, GTK_POS_RIGHT,
                             1, 1);

    apptw->Sound_entry = gtk_entry_new();
    g_object_set (apptw->Sound_entry, "margin-left", 10,
                                      "margin-right", 6,
                                      "halign", GTK_ALIGN_FILL,
                                      "hexpand", TRUE, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Sound_hbox),
                             apptw->Sound_entry, NULL, GTK_POS_RIGHT,
                             1, 1);
    apptw->Sound_button = orage_util_image_button ("document-open", _("_Open"));
    g_object_set (apptw->Sound_button, "halign", GTK_ALIGN_FILL,
                                       "hexpand", FALSE, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Sound_hbox),
                             apptw->Sound_button, NULL, GTK_POS_RIGHT,
                             1, 1);

    orage_table_add_row(apptw->TableAlarm
            , apptw->Sound_label, apptw->Sound_hbox
            , ++row, (GTK_FILL), (GTK_FILL));

    apptw->SoundRepeat_hbox = gtk_grid_new ();
    apptw->SoundRepeat_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("Repeat alarm sound"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->SoundRepeat_hbox),
                             apptw->SoundRepeat_checkbutton, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->SoundRepeat_spin_cnt = gtk_spin_button_new_with_range(1, 999, 10);
    g_object_set (apptw->SoundRepeat_spin_cnt, "margin-left", 30,
                                               "margin-right", 3,
                                               NULL);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(apptw->SoundRepeat_spin_cnt)
            , TRUE);
    gtk_grid_attach_next_to (GTK_GRID (apptw->SoundRepeat_hbox),
                             apptw->SoundRepeat_spin_cnt, NULL, GTK_POS_RIGHT,
                             1, 1);

    apptw->SoundRepeat_spin_cnt_label = gtk_label_new(_("times"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->SoundRepeat_hbox),
                             apptw->SoundRepeat_spin_cnt_label, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->SoundRepeat_spin_len = gtk_spin_button_new_with_range(1, 250, 1);
    g_object_set (apptw->SoundRepeat_spin_len, "margin-left", 30,
                                               "margin-right", 3,
                                               NULL);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(apptw->SoundRepeat_spin_len)
            , TRUE);

    gtk_grid_attach_next_to (GTK_GRID (apptw->SoundRepeat_hbox),
                             apptw->SoundRepeat_spin_len, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->SoundRepeat_spin_len_label = gtk_label_new(_("sec interval"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->SoundRepeat_hbox),
                             apptw->SoundRepeat_spin_len_label, NULL,
                             GTK_POS_RIGHT, 1, 1);

    orage_table_add_row(apptw->TableAlarm
            , NULL, apptw->SoundRepeat_hbox
            , ++row, (GTK_EXPAND | GTK_FILL), (0));

    /***** Display Alarm *****/
    apptw->Display_label = gtk_label_new(_("Visual"));

    apptw->Display_hbox_orage = gtk_grid_new ();
    apptw->Display_checkbutton_orage = 
            gtk_check_button_new_with_mnemonic(_("Use Orage window"));
    gtk_widget_set_tooltip_text(apptw->Display_checkbutton_orage
            , _("Select this if you want Orage window alarm"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Display_hbox_orage),
                             apptw->Display_checkbutton_orage, NULL,
                             GTK_POS_RIGHT, 1, 1);

    orage_table_add_row(apptw->TableAlarm
            , apptw->Display_label, apptw->Display_hbox_orage
            , ++row, (GTK_EXPAND | GTK_FILL), (0));

#ifdef HAVE_NOTIFY
    apptw->Display_hbox_notify = gtk_grid_new ();
    apptw->Display_checkbutton_notify = 
            gtk_check_button_new_with_mnemonic(_("Use notification"));
    gtk_widget_set_tooltip_text(apptw->Display_checkbutton_notify
            , _("Select this if you want notification alarm"));
    g_object_set (apptw->Display_checkbutton_notify, "margin-right", 10,
                                                     NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Display_hbox_notify),
                             apptw->Display_checkbutton_notify, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->Display_checkbutton_expire_notify = 
            gtk_check_button_new_with_mnemonic(_("Set timeout"));
    gtk_widget_set_tooltip_text(apptw->Display_checkbutton_expire_notify
            , _("Select this if you want notification to expire automatically")
            );
    gtk_grid_attach_next_to (GTK_GRID (apptw->Display_hbox_notify),
                             apptw->Display_checkbutton_expire_notify, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->Display_spin_expire_notify = 
            gtk_spin_button_new_with_range(0, 999, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(apptw->Display_spin_expire_notify)
            , TRUE);
    gtk_widget_set_tooltip_text(apptw->Display_spin_expire_notify
            , _("0 = system default expiration time"));
    g_object_set (apptw->Display_spin_expire_notify, "margin-left", 20,
                                                     "margin-right", 10,
                                                     NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Display_hbox_notify),
                             apptw->Display_spin_expire_notify, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->Display_spin_expire_notify_label = gtk_label_new(_("seconds"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Display_hbox_notify),
                             apptw->Display_spin_expire_notify_label, NULL,
                             GTK_POS_RIGHT, 1, 1);

    orage_table_add_row(apptw->TableAlarm
            , NULL, apptw->Display_hbox_notify
            , ++row, (GTK_EXPAND | GTK_FILL), (0));
#endif

    /***** Procedure Alarm *****/
    apptw->Proc_label = gtk_label_new(_("Procedure"));

    apptw->Proc_hbox = gtk_grid_new ();
    apptw->Proc_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("Use"));
    gtk_widget_set_tooltip_text(apptw->Proc_checkbutton
            , _("Select this if you want procedure or script alarm"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Proc_hbox),
                             apptw->Proc_checkbutton, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->Proc_entry = gtk_entry_new();
    g_object_set (apptw->Proc_entry, "margin-left", 10,
                                     "halign", GTK_ALIGN_FILL,
                                     "hexpand", TRUE, NULL);
    gtk_widget_set_tooltip_text(apptw->Proc_entry
            , _("You must enter all escape etc characters yourself.\nThis string is just given to shell to process.\nThe following special commands are replaced at run time:\n\t<&T>  appointment title\n\t<&D>  appointment description\n\t<&AT> alarm time\n\t<&ST> appointment start time\n\t<&ET> appointment end time"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Proc_hbox),
                             apptw->Proc_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);

    orage_table_add_row(apptw->TableAlarm
            , apptw->Proc_label, apptw->Proc_hbox
            , ++row, (GTK_FILL), (GTK_FILL));

    /***** Test Alarm *****/
    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    g_object_set (separator, "margin", 3, NULL);
    gtk_grid_attach_next_to (GTK_GRID (vbox), separator, NULL, GTK_POS_BOTTOM,
                             1, 1);
    apptw->Test_button = orage_util_image_button ("system-run", _("_Execute"));
    gtk_widget_set_tooltip_text(apptw->Test_button
            , _("Test this alarm by raising it now"));
    gtk_grid_attach_next_to (GTK_GRID (vbox), apptw->Test_button, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    padding = gtk_label_new ("");
    g_object_set (padding, "valign", GTK_ALIGN_FILL,
                           "vexpand", TRUE, NULL);
    gtk_grid_attach_next_to (GTK_GRID (vbox), padding, NULL, GTK_POS_BOTTOM,
                             1, 1);

    /***** Default Alarm Settings *****/
    apptw->Default_hbox = gtk_grid_new ();
    apptw->Default_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(apptw->Default_label)
            , _("<b>Default alarm</b>"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Default_hbox),
                             apptw->Default_label, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->Default_savebutton = orage_util_image_button ("document-save",
                                                         _("_Save"));
    g_object_set (apptw->Default_savebutton, "margin-left", 6,
                                             "halign", GTK_ALIGN_FILL,
                                             "hexpand", TRUE, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Default_hbox),
                             apptw->Default_savebutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Default_savebutton
            , _("Store current settings as default alarm"));
    apptw->Default_readbutton = orage_util_image_button ("document-revert",
                                                         _("_Revert"));
    g_object_set (apptw->Default_readbutton, "margin-left", 6,
                                             "halign", GTK_ALIGN_FILL,
                                             "hexpand", TRUE, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Default_hbox),
                             apptw->Default_readbutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Default_readbutton
            , _("Set current settings from default alarm"));

    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    g_object_set (separator, "margin", 3, NULL);
    gtk_grid_attach_next_to (GTK_GRID (vbox), separator, NULL, GTK_POS_BOTTOM,
                             1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), apptw->Default_hbox, NULL,
                             GTK_POS_BOTTOM, 1, 1);
}

static void enable_alarm_page_signals(appt_win *apptw)
{
    g_signal_connect((gpointer)apptw->Alarm_spin_dd, "value-changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Alarm_spin_hh, "value-changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Alarm_spin_mm, "value-changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Alarm_when_cb, "changed"
            , G_CALLBACK(on_app_combobox_changed_cb), apptw);

    g_signal_connect((gpointer)apptw->Sound_checkbutton, "clicked"
            , G_CALLBACK(app_sound_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Sound_entry, "changed"
            , G_CALLBACK(on_app_entry_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Sound_button, "clicked"
            , G_CALLBACK(on_appSound_button_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->SoundRepeat_checkbutton, "clicked"
            , G_CALLBACK(app_sound_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->SoundRepeat_spin_cnt, "value-changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->SoundRepeat_spin_len, "value-changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);

    g_signal_connect((gpointer)apptw->Display_checkbutton_orage, "clicked"
            , G_CALLBACK(app_checkbutton_clicked_cb), apptw);
#ifdef HAVE_NOTIFY
    g_signal_connect((gpointer)apptw->Display_checkbutton_notify, "clicked"
            , G_CALLBACK(app_notify_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Display_checkbutton_expire_notify
            , "clicked"
            , G_CALLBACK(app_notify_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Display_spin_expire_notify
            , "value-changed"
            , G_CALLBACK(on_app_spin_button_changed_cb), apptw);
#endif

    g_signal_connect((gpointer)apptw->Proc_checkbutton, "clicked"
            , G_CALLBACK(app_proc_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Proc_entry, "changed"
            , G_CALLBACK(on_app_entry_changed_cb), apptw);

    g_signal_connect((gpointer)apptw->Test_button, "clicked"
            , G_CALLBACK(on_test_button_clicked_cb), apptw);

    g_signal_connect((gpointer)apptw->Default_savebutton, "clicked"
            , G_CALLBACK(on_appDefault_save_button_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Default_readbutton, "clicked"
            , G_CALLBACK(on_appDefault_read_button_clicked_cb), apptw);
}

static void build_recurrence_page(appt_win *apptw)
{
    gint row, i;
    guint y, m;
    const gchar *recur_freq_array[6] = {
        _("None"), _("Daily"), _("Weekly"), _("Monthly"), _("Yearly"), _("Hourly")};
    char *weekday_array[7] = {
        _("Mon"), _("Tue"), _("Wed"), _("Thu"), _("Fri"), _("Sat"), _("Sun")};
    GtkWidget *hbox;

    apptw->TableRecur = orage_table_new (BORDER_SIZE);
    apptw->Recur_notebook_page = apptw->TableRecur;
    apptw->Recur_tab_label = gtk_label_new(_("Recurrence"));

    gtk_notebook_append_page(GTK_NOTEBOOK(apptw->Notebook)
            , apptw->Recur_notebook_page, apptw->Recur_tab_label);

    /* complexity */
    apptw->Recur_feature_label = gtk_label_new(_("Complexity"));
    apptw->Recur_feature_hbox = gtk_grid_new ();
    apptw->Recur_feature_normal_rb = gtk_radio_button_new_with_label(NULL
            , _("Basic"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_feature_hbox),
                             apptw->Recur_feature_normal_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Recur_feature_advanced_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                    GTK_RADIO_BUTTON(apptw->Recur_feature_normal_rb)
                            , _("Advanced"));
    g_object_set (apptw->Recur_feature_advanced_rb, "margin-left", 20, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_feature_hbox),
                             apptw->Recur_feature_advanced_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Recur_feature_normal_rb
            , _("Use this if you want regular repeating event"));
    gtk_widget_set_tooltip_text(apptw->Recur_feature_advanced_rb
            , _("Use this if you need complex times like:\n Every Saturday and Sunday or \n First Tuesday every month"));
    orage_table_add_row(apptw->TableRecur
            , apptw->Recur_feature_label, apptw->Recur_feature_hbox
            , row = 0, (GTK_EXPAND | GTK_FILL), (0));

    /* frequency */
    apptw->Recur_freq_label = gtk_label_new(_("Frequency"));
    apptw->Recur_freq_hbox = gtk_grid_new ();
    apptw->Recur_freq_cb = orage_create_combo_box_with_content(
            recur_freq_array, 6);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_freq_hbox),
                             apptw->Recur_freq_cb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Recur_int_spin_label1 = gtk_label_new(_("Each"));
    g_object_set (apptw->Recur_int_spin_label1, "margin-left", 5,
                                                "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_freq_hbox),
                             apptw->Recur_int_spin_label1, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Recur_int_spin = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(apptw->Recur_int_spin), TRUE);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_freq_hbox),
                             apptw->Recur_int_spin, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Recur_int_spin_label2 = gtk_label_new(_("occurrence"));
    g_object_set (apptw->Recur_int_spin_label2, "margin-left", 5,
                                                "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_freq_hbox),
                             apptw->Recur_int_spin_label2, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Recur_int_spin
            , _("Limit frequency to certain interval.\n For example: Every third day:\n Frequency = Daily and Interval = 3"));
    orage_table_add_row(apptw->TableRecur
            , apptw->Recur_freq_label, apptw->Recur_freq_hbox
            , ++row, (GTK_EXPAND | GTK_FILL), (GTK_FILL));

    /* limitation */
    hbox = gtk_grid_new ();
    apptw->Recur_limit_label = gtk_label_new(_("Limit"));
    apptw->Recur_limit_rb = gtk_radio_button_new_with_label(NULL
            , _("Repeat forever"));
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             apptw->Recur_limit_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->Recur_count_hbox = gtk_grid_new ();
    apptw->Recur_count_rb = gtk_radio_button_new_with_mnemonic_from_widget(
            GTK_RADIO_BUTTON(apptw->Recur_limit_rb), _("Repeat "));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_count_hbox),
                             apptw->Recur_count_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Recur_count_spin = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(apptw->Recur_count_spin)
            , TRUE);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_count_hbox),
                             apptw->Recur_count_spin, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Recur_count_label = gtk_label_new(_("times"));
    g_object_set (apptw->Recur_count_label, "margin-left", 5,
                                            "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_count_hbox),
                             apptw->Recur_count_label, NULL,
                             GTK_POS_RIGHT, 1, 1);
    g_object_set (apptw->Recur_count_hbox, "margin-left", 20,
                                           "margin-right", 20, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             apptw->Recur_count_hbox, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->Recur_until_hbox = gtk_grid_new ();
    apptw->Recur_until_rb = gtk_radio_button_new_with_mnemonic_from_widget(
            GTK_RADIO_BUTTON(apptw->Recur_limit_rb), _("Repeat until "));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_until_hbox),
                             apptw->Recur_until_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Recur_until_button = gtk_button_new();
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_until_hbox),
                             apptw->Recur_until_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             apptw->Recur_until_hbox, NULL,
                             GTK_POS_RIGHT, 1, 1);
    orage_table_add_row(apptw->TableRecur
            , apptw->Recur_limit_label, hbox
            , ++row, (GTK_EXPAND | GTK_FILL), (0));

    /* weekdays (only for complex settings) */
    apptw->Recur_byday_label = gtk_label_new(_("Weekdays"));
    apptw->Recur_byday_hbox = gtk_grid_new ();
    g_object_set (apptw->Recur_byday_hbox, "column-homogeneous", TRUE,
                                           NULL);
    for (i=0; i <= 6; i++) {
        apptw->Recur_byday_cb[i] = 
                gtk_check_button_new_with_mnemonic(weekday_array[i]);
        g_object_set (apptw->Recur_byday_cb[i], "halign", GTK_ALIGN_CENTER,
                                                NULL);
        gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_byday_hbox),
                                 apptw->Recur_byday_cb[i], NULL,
                                 GTK_POS_RIGHT, 1, 1);
    }
    orage_table_add_row(apptw->TableRecur
            , apptw->Recur_byday_label, apptw->Recur_byday_hbox
            , ++row, (GTK_EXPAND | GTK_FILL), (0));

    apptw->Recur_byday_spin_label = gtk_label_new(_("Which day"));
    apptw->Recur_byday_spin_hbox = gtk_grid_new ();
    g_object_set (apptw->Recur_byday_spin_hbox, "column-homogeneous", TRUE,
                                                NULL);
    for (i=0; i <= 6; i++) {
        apptw->Recur_byday_spin[i] = 
                gtk_spin_button_new_with_range(-9, 9, 1);
        gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_byday_spin_hbox),
                                 apptw->Recur_byday_spin[i], NULL,
                                 GTK_POS_RIGHT, 1, 1);
        gtk_widget_set_tooltip_text(apptw->Recur_byday_spin[i]
                , _("Specify which weekday for monthly and yearly events.\n For example:\n Second Wednesday each month:\n\tFrequency = Monthly,\n\tWeekdays = check only Wednesday,\n\tWhich day = select 2 from the number below Wednesday"));
    }
    orage_table_add_row(apptw->TableRecur
            , apptw->Recur_byday_spin_label, apptw->Recur_byday_spin_hbox
            , ++row ,(GTK_EXPAND | GTK_FILL), (0));

    /* TODO base (only for TODOs) */
    apptw->Recur_todo_base_label = gtk_label_new(_("TODO base"));
    apptw->Recur_todo_base_hbox = gtk_grid_new ();
    apptw->Recur_todo_base_start_rb = gtk_radio_button_new_with_label(NULL
            , _("Start"));
    g_object_set (apptw->Recur_todo_base_start_rb, "margin-left", 15,
                                                   "margin-right", 15, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_todo_base_hbox),
                             apptw->Recur_todo_base_start_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Recur_todo_base_done_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                    GTK_RADIO_BUTTON(apptw->Recur_todo_base_start_rb)
                            , _("Completed"));
    g_object_set (apptw->Recur_todo_base_done_rb, "margin-left", 15,
                                                  "margin-right", 15, NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_todo_base_hbox),
                             apptw->Recur_todo_base_done_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(apptw->Recur_todo_base_start_rb
            , _("TODO reoccurs regularly starting on start time and repeating after each interval no matter when it was last completed"));
    gtk_widget_set_tooltip_text(apptw->Recur_todo_base_done_rb
            , _("TODO reoccurrency is based on complete time and repeats after the interval counted from the last completed time.\n(Note that you can not tell anything about the history of the TODO since reoccurrence base changes after each completion.)"));
    orage_table_add_row(apptw->TableRecur
            , apptw->Recur_todo_base_label, apptw->Recur_todo_base_hbox
            , ++row ,(GTK_EXPAND | GTK_FILL), (0));

    /* exceptions */
    apptw->Recur_exception_label = gtk_label_new(_("Exceptions"));
    apptw->Recur_exception_hbox = gtk_grid_new ();
    apptw->Recur_exception_scroll_win = gtk_scrolled_window_new(NULL, NULL);

    /* FIXME: Using width-request is hack. Recur_exception_scroll_win should
     * take all remaing space from grid and Recur_exception_type_vbox should
     * aligned to end. But we don't know width of other wigets in this tab and
     * this leads to streched exception row. Ideal solution would be
     * Recur_exception_type_vbox "halign" set to GTK_ALIGN_END and
     * Recur_exception_scroll_win "width-request" is removed.
     */
    g_object_set (apptw->Recur_exception_scroll_win, "hexpand", TRUE,
                                                     "width-request", 80,
                                                     NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_exception_hbox),
                             apptw->Recur_exception_scroll_win, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_scrolled_window_set_policy(
            GTK_SCROLLED_WINDOW(apptw->Recur_exception_scroll_win)
            , GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    apptw->Recur_exception_rows_vbox = gtk_grid_new ();

    gtk_container_add (GTK_CONTAINER (apptw->Recur_exception_scroll_win),
                                      apptw->Recur_exception_rows_vbox);

    gtk_widget_set_tooltip_text(apptw->Recur_exception_scroll_win
            , _("Add more exception dates by clicking the calendar days below.\nException is either exclusion(-) or inclusion(+) depending on the selection.\nRemove by clicking the data."));
    apptw->Recur_exception_type_vbox = gtk_grid_new ();
    g_object_set (apptw->Recur_exception_type_vbox, "hexpand", TRUE,
                                                    "halign", GTK_ALIGN_FILL,
                                                    "margin-left", 5,
                                                    "margin-right", 15,
                                                    NULL);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_exception_hbox),
                             apptw->Recur_exception_type_vbox, NULL,
                             GTK_POS_RIGHT, 1, 1);
    apptw->Recur_exception_excl_rb = gtk_radio_button_new_with_label(NULL
            , _("Add excluded date (-)"));
    gtk_widget_set_tooltip_text(apptw->Recur_exception_excl_rb
            , _("Excluded days are full days where this appointment is not happening"));
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_exception_type_vbox),
                             apptw->Recur_exception_excl_rb, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    apptw->Recur_exception_incl_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                    GTK_RADIO_BUTTON(apptw->Recur_exception_excl_rb)
                    , _("Add included time (+)"));
    gtk_widget_set_tooltip_text(apptw->Recur_exception_incl_rb
            , _("Included times have same timezone than start time, but they may have different time"));
    apptw->Recur_exception_incl_spin_hh = 
            gtk_spin_button_new_with_range(0, 23, 1);
    apptw->Recur_exception_incl_spin_mm = 
            gtk_spin_button_new_with_range(0, 59, 1);
    apptw->Recur_exception_incl_time_hbox = datetime_hbox_new(
            apptw->Recur_exception_incl_rb
            , apptw->Recur_exception_incl_spin_hh
            , apptw->Recur_exception_incl_spin_mm, NULL);

    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_exception_type_vbox),
                             apptw->Recur_exception_incl_time_hbox, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    orage_table_add_row(apptw->TableRecur
            , apptw->Recur_exception_label, apptw->Recur_exception_hbox
            , ++row ,(GTK_EXPAND | GTK_FILL), (0));

    /* calendars showing the action days */
    apptw->Recur_calendar_label = gtk_label_new(_("Action dates"));
    apptw->Recur_calendar_hbox = gtk_grid_new ();
    apptw->Recur_calendar1 = gtk_calendar_new();
    gtk_calendar_set_display_options(GTK_CALENDAR(apptw->Recur_calendar1)
            , GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES);
    gtk_calendar_get_date(GTK_CALENDAR(apptw->Recur_calendar1), &y, &m, NULL);
    gtk_calendar_select_day(GTK_CALENDAR(apptw->Recur_calendar1), 0);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_calendar_hbox),
                             apptw->Recur_calendar1, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->Recur_calendar2 = gtk_calendar_new();
    gtk_calendar_set_display_options(GTK_CALENDAR(apptw->Recur_calendar2)
            , GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES);
    if (++m>11) {
        m=0;
        y++;
    }
    gtk_calendar_select_month(GTK_CALENDAR(apptw->Recur_calendar2), m, y);
    gtk_calendar_select_day(GTK_CALENDAR(apptw->Recur_calendar2), 0);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_calendar_hbox),
                             apptw->Recur_calendar2, NULL,
                             GTK_POS_RIGHT, 1, 1);

    apptw->Recur_calendar3 = gtk_calendar_new();
    gtk_calendar_set_display_options(GTK_CALENDAR(apptw->Recur_calendar3)
            , GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES);
    if (++m>11) {
        m=0;
        y++;
    }
    gtk_calendar_select_month(GTK_CALENDAR(apptw->Recur_calendar3), m, y);
    gtk_calendar_select_day(GTK_CALENDAR(apptw->Recur_calendar3), 0);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_calendar_hbox),
                             apptw->Recur_calendar3, NULL,
                             GTK_POS_RIGHT, 1, 1);
    orage_table_add_row(apptw->TableRecur
            , apptw->Recur_calendar_label, apptw->Recur_calendar_hbox
            , ++row ,(GTK_EXPAND | GTK_FILL), (0));

}

static void enable_recurrence_page_signals(appt_win *apptw)
{
    gint i;

    g_signal_connect((gpointer)apptw->Recur_feature_normal_rb, "clicked"
            , G_CALLBACK(app_recur_feature_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_feature_advanced_rb, "clicked"
            , G_CALLBACK(app_recur_feature_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_freq_cb, "changed"
            , G_CALLBACK(on_freq_combobox_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_int_spin, "value-changed"
            , G_CALLBACK(on_recur_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_limit_rb, "clicked"
            , G_CALLBACK(app_recur_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_count_rb, "clicked"
            , G_CALLBACK(app_recur_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_count_spin, "value-changed"
            , G_CALLBACK(on_recur_spin_button_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_until_rb, "clicked"
            , G_CALLBACK(app_recur_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_until_button, "clicked"
            , G_CALLBACK(on_recur_Date_button_clicked_cb), apptw);
    for (i=0; i <= 6; i++) {
        g_signal_connect((gpointer)apptw->Recur_byday_cb[i], "clicked"
                , G_CALLBACK(app_recur_checkbutton_clicked_cb), apptw);
        g_signal_connect((gpointer)apptw->Recur_byday_spin[i], "value-changed"
                , G_CALLBACK(on_recur_spin_button_changed_cb), apptw);
    }
    g_signal_connect((gpointer)apptw->Recur_todo_base_start_rb, "clicked"
            , G_CALLBACK(app_recur_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_todo_base_done_rb, "clicked"
            , G_CALLBACK(app_recur_checkbutton_clicked_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_calendar1, "month-changed"
            , G_CALLBACK(recur_month_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_calendar2, "month-changed"
            , G_CALLBACK(recur_month_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_calendar3, "month-changed"
            , G_CALLBACK(recur_month_changed_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_calendar1
            , "day_selected_double_click"
            , G_CALLBACK(recur_day_selected_double_click_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_calendar2
            , "day_selected_double_click"
            , G_CALLBACK(recur_day_selected_double_click_cb), apptw);
    g_signal_connect((gpointer)apptw->Recur_calendar3
            , "day_selected_double_click"
            , G_CALLBACK(recur_day_selected_double_click_cb), apptw);
}

appt_win *create_appt_win (const gchar *action, gchar *par, GDateTime *gdt_par)
{
    appt_win *apptw;
    GdkWindow *window;

    /*  initialisation + main window + base vbox */
    apptw = g_new(appt_win, 1);
    apptw->xf_uid = NULL;
    apptw->par = NULL;
    apptw->par2 = g_date_time_new_now_local ();
    apptw->xf_appt = NULL;
    apptw->el = NULL;
    apptw->dw = NULL;
    apptw->appointment_changed = FALSE;
    apptw->accel_group = gtk_accel_group_new();

    apptw->Window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#if 0
    gtk_window_set_default_size(GTK_WINDOW(apptw->Window), 450, 325);
#endif
    gtk_window_add_accel_group(GTK_WINDOW(apptw->Window), apptw->accel_group);

    apptw->Vbox = gtk_grid_new ();
    gtk_container_add(GTK_CONTAINER(apptw->Window), apptw->Vbox);

    build_menu(apptw);
    build_toolbar(apptw);

    /* ********** Here begins tabs ********** */
    apptw->Notebook = gtk_notebook_new();
    gtk_grid_attach (GTK_GRID (apptw->Vbox), apptw->Notebook, 1, 3, 1, 1);
    gtk_container_set_border_width(GTK_CONTAINER (apptw->Notebook), 5);

    build_general_page(apptw);
    build_alarm_page(apptw);
    build_recurrence_page(apptw);

    g_signal_connect((gpointer)apptw->Window, "delete-event"
            , G_CALLBACK(on_appWindow_delete_event_cb), apptw);

    if (fill_appt_window(apptw, action, par, gdt_par)) { /* all fine */
        enable_general_page_signals(apptw);
        enable_alarm_page_signals(apptw);
        enable_recurrence_page_signals(apptw);
        gtk_widget_show_all(apptw->Window);
        recur_hide_show(apptw);
        type_hide_show(apptw);
        readonly_hide_show(apptw);
        g_signal_connect((gpointer)apptw->Notebook, "switch-page"
                , G_CALLBACK(on_notebook_page_switch), apptw);
        gtk_widget_grab_focus(apptw->Title_entry);
        window = gtk_widget_get_window (GTK_WIDGET(apptw->Window));
        gdk_x11_window_set_user_time(window, gdk_x11_get_server_time(window));
        gtk_window_present(GTK_WINDOW(apptw->Window));
    }
    else { /* failed to get data */
        app_free_memory(apptw);
        apptw = NULL;
    }

    return(apptw);
}
