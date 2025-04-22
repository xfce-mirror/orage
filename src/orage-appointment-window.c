/*
 * Copyright (c) 2021-2023 Erkki Moorits
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
 *     Free Software Foundation
 *     51 Franklin Street, 5th Floor
 *     Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#define _XOPEN_SOURCE /* glibc2 needs this */

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

#include "orage-alarm-structure.h"
#include "orage-category.h"
#include "orage-i18n.h"
#include "orage-window.h"
#include "functions.h"
#include "ical-code.h"
#include "timezone_selection.h"
#include "event-list.h"
#include "orage-week-window.h"
#include "orage-appointment-window.h"
#include "parameters.h"
#include "reminder.h"
#include "orage-rc-file.h"
#include "xfical_exception.h"

#define BORDER_SIZE 20
#define FILETYPE_SIZE 38

#define CATEGORIES_SPACING 10
#define EXCEPTION_KEY "xfical_exception_key"

#define APPOINTMENT_TIME_PROPERTY "appointment-time"
#define APPOINTMENT_ACTION_PROPERTY "appointment-action"
#define APPOINTMENT_PARAMETER_PROPERTY "appointment-parameter"

#define RECURRENCE_NONE "none"
#define RECURRENCE_DAILY "daily"
#define RECURRENCE_WEEKLY "weekly"
#define RECURRENCE_MONTHLY "monthly"
#define RECURRENCE_YEARLY "yearly"
#define RECURRENCE_HOURLY "hourly"

#define RECUR_FREQ_ARRAY_ELEMENTS 6
#define NR_OF_RECUR_CALENDARS 3

typedef enum
{
    NEW_APPT_WIN,
    UPDATE_APPT_WIN,
    COPY_APPT_WIN
} appt_win_action;

struct _OrageAppointmentWindow
{
    GtkWindow __parent__;

    GtkAccelGroup *accel_group;

    GtkWidget *Vbox;

    GtkWidget *Menubar;
    GtkWidget *File_menu;
    GtkWidget *File_menu_save;
    GtkWidget *File_menu_saveclose;
    GtkWidget *File_menu_revert;
    GtkWidget *File_menu_duplicate;
    GtkWidget *File_menu_delete;
    GtkWidget *File_menu_close;
    GtkWidget *File_insert_cb;

    GtkWidget *Toolbar;
    GtkWidget *Save;
    GtkWidget *SaveClose;
    GtkWidget *Revert;
    GtkWidget *Duplicate;
    GtkWidget *Delete;

    GtkWidget *Notebook;
    GtkWidget *TableGeneral;
    GtkWidget *General_tab_label;
    GtkWidget *General_notebook_page;
    GtkWidget *Type_label;
    GtkWidget *Type_event_rb;
    GtkWidget *Type_todo_rb;
    GtkWidget *Type_journal_rb;
    GtkWidget *Title_label;
    GtkWidget *Title_entry;
    GtkWidget *Location_label;
    GtkWidget *Location_entry;
    GtkWidget *AllDay_checkbutton;
    GtkWidget *Start_label;
    GtkWidget *StartDate_button;
    GtkWidget *StartTime_spin_hh;
    GtkWidget *StartTime_spin_mm;
    GtkWidget *StartTimezone_button;
    GtkWidget *StartTime_hbox;
    GtkWidget *End_label;
    GtkWidget *End_hbox;
    GtkWidget *End_checkbutton;
    GtkWidget *EndTime_hbox;
    GtkWidget *EndDate_button;
    GtkWidget *EndTime_spin_hh;
    GtkWidget *EndTime_spin_mm;
    GtkWidget *EndTimezone_button;
    GtkWidget *Dur_hbox;
    GtkWidget *Dur_checkbutton;
    GtkWidget *Dur_time_hbox;
    GtkWidget *Dur_spin_dd;
    GtkWidget *Dur_spin_dd_label;
    GtkWidget *Dur_spin_hh;
    GtkWidget *Dur_spin_hh_label;
    GtkWidget *Dur_spin_mm;
    GtkWidget *Dur_spin_mm_label;
    GtkWidget *Completed_label;
    GtkWidget *Completed_hbox;
    GtkWidget *Completed_checkbutton;
    GtkWidget *CompletedTime_hbox;
    GtkWidget *CompletedDate_button;
    GtkWidget *CompletedTime_spin_hh;
    GtkWidget *CompletedTime_spin_mm;
    GtkWidget *CompletedTimezone_button;
    GtkWidget *Availability_label;
    GtkWidget *Availability_cb;
    GtkWidget *Categories_label;
    GtkWidget *Categories_hbox;
    GtkWidget *Categories_entry;
    GtkWidget *Categories_cb;
    GtkWidget *Categories_cb_event;
    GtkWidget *Categories_button;
    GtkWidget *Priority_label;
    GtkWidget *Priority_spin;
    GtkWidget *Note;
    GtkWidget *Note_Scrolledwindow;
    GtkWidget *Note_textview;
    GtkTextBuffer *Note_buffer;

    GtkWidget *Alarm_notebook_page;
    GtkWidget *Alarm_tab_label;
    GtkWidget *TableAlarm;
    GtkWidget *Alarm_label;
    GtkWidget *Alarm_hbox;
    GtkWidget *Alarm_time_hbox;
    GtkWidget *Alarm_spin_dd;
    GtkWidget *Alarm_spin_dd_label;
    GtkWidget *Alarm_spin_hh;
    GtkWidget *Alarm_spin_hh_label;
    GtkWidget *Alarm_spin_mm;
    GtkWidget *Alarm_spin_mm_label;
    GtkWidget *Alarm_when_cb;
    GtkWidget *Per_hbox;
    GtkWidget *Per_checkbutton;
    GtkWidget *Sound_label;
    GtkWidget *Sound_hbox;
    GtkWidget *Sound_checkbutton;
    GtkWidget *Sound_entry;
    GtkWidget *Sound_button;
    GtkWidget *SoundRepeat_hbox;
    GtkWidget *SoundRepeat_checkbutton;
    GtkWidget *SoundRepeat_spin_cnt;
    GtkWidget *SoundRepeat_spin_cnt_label;
    GtkWidget *SoundRepeat_spin_len;
    GtkWidget *SoundRepeat_spin_len_label;
    GtkWidget *Display_label;
    GtkWidget *Display_hbox_orage;
    GtkWidget *Display_checkbutton_orage;
#ifdef HAVE_NOTIFY
    GtkWidget *Display_hbox_notify;
    GtkWidget *Display_checkbutton_notify;
    GtkWidget *Display_checkbutton_expire_notify;
    GtkWidget *Display_spin_expire_notify;
    GtkWidget *Display_spin_expire_notify_label;
#endif
    GtkWidget *Proc_label;
    GtkWidget *Proc_hbox;
    GtkWidget *Proc_checkbutton;
    GtkWidget *Proc_entry;
    GtkWidget *Test_button;
    GtkWidget *Default_label;
    GtkWidget *Default_hbox;
    GtkWidget *Default_savebutton;
    GtkWidget *Default_readbutton;

    GtkWidget *Recur_notebook_page;
    GtkWidget *Recur_tab_label;
    GtkWidget *TableRecur;
    GtkWidget *Recur_freq_label;
    GtkWidget *Recur_freq_hbox;
    GtkWidget *Recur_freq_cb;
    GtkWidget *Recur_limit_rb;
    GtkWidget *Recur_count_rb;
    GtkWidget *Recur_count_spin;
    GtkWidget *Recur_until_rb;
    GtkWidget *Recur_until_button;
    GtkWidget *Recur_todo_base_label;
    GtkWidget *Recur_todo_base_hbox;
    GtkWidget *Recur_todo_base_start_rb;
    GtkWidget *Recur_todo_base_done_rb;
    GtkWidget *Recur_exception_label;
    GtkWidget *Recur_exception_hbox;
    GtkWidget *Recur_exception_scroll_win;
    GtkWidget *Recur_exception_rows_vbox;
    GtkWidget *Recur_exception_type_vbox;
    GtkWidget *Recur_exception_excl_rb;
    GtkWidget *Recur_exception_incl_rb;
    GtkWidget *Recur_exception_incl_time_hbox;
    GtkWidget *Recur_exception_incl_spin_hh;
    GtkWidget *Recur_exception_incl_spin_mm;
    GtkWidget *Recur_calendar_label;
    GtkWidget *Recur_calendar_hbox;
    GtkWidget *Recur_calendar[NR_OF_RECUR_CALENDARS];

    GtkStack  *recurrence_frequency_box;
    GtkWidget *recurrence_limit_box;

    GtkWidget *recurrence_hourly_interval_spin;

    GtkWidget *recurrence_daily_interval_spin;

    GtkWidget *recurrence_weekly_byday[7]; /* 0=Mo, 1=Tu ... 6=Su */
    GtkWidget *recurrence_weekly_interval_spin;

    GtkWidget *recurrence_monthly_beginning_selector;
    GtkWidget *recurrence_monthly_end_selector;
    GtkWidget *recurrence_monthly_every_selector;
    GtkWidget *recurrence_monthly_begin_spin;
    GtkWidget *recurrence_monthly_end_spin;
    GtkWidget *recurrence_monthly_week_selector;
    GtkWidget *recurrence_monthly_day_selector;

    GtkWidget *recurecnce_yearly_week_selector;
    GtkWidget *recurecnce_yearly_day_selector;
    GtkWidget *recurecnce_yearly_month_selector;

    GDateTime *appointment_time;
    GDateTime *appointment_time_2;
    gboolean appointment_add;       /* are we adding app */
    gboolean appointment_changed;   /* has this app been modified now */
    gboolean appointment_new;       /* is this new = no uid yet */
    /* COPY uses old uid as base and adds new, so
     * add == TRUE && new == FALSE */

    void *xf_appt; /* this is xfical_appt */
    gchar *xf_uid;

    el_win *el;          /* used to refresh calling event list */
    OrageWeekWindow *dw; /* used to refresh calling day list */

    appt_win_action action;
    gchar *par;
};

G_DEFINE_TYPE (OrageAppointmentWindow, orage_appointment_window, GTK_TYPE_WINDOW)

enum
{
    PROP_0,
    PROP_APPOINTMENT_TIME,
    PROP_APPOINTMENT_ACTION,
    PROP_APPOINTMENT_PAR,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = {NULL,};

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

    OrageAppointmentWindow *apptw;
} category_win_struct;

static void refresh_categories(category_win_struct *catw);
static void read_default_alarm(xfical_appt *appt);
static void fill_appt_window (OrageAppointmentWindow *apptw,
                                appt_win_action action,
                                const gchar *par, GDateTime *gdt_par);
static gboolean fill_appt_from_apptw(xfical_appt *appt, OrageAppointmentWindow *apptw);

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

static void mark_appointment_changed (OrageAppointmentWindow *apptw)
{
    if (apptw->appointment_changed)
        return;

    apptw->appointment_changed = TRUE;
    gtk_widget_set_sensitive (apptw->Revert, TRUE);
    gtk_widget_set_sensitive (apptw->File_menu_revert, TRUE);
}

static void mark_appointment_unchanged (OrageAppointmentWindow *apptw)
{
    if (apptw->appointment_changed) {
        apptw->appointment_changed = FALSE;
        gtk_widget_set_sensitive(apptw->Revert, FALSE);
        gtk_widget_set_sensitive(apptw->File_menu_revert, FALSE);
    }
}

static void set_time_sensitivity (OrageAppointmentWindow *apptw)
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

static void set_repeat_sensitivity (OrageAppointmentWindow *apptw)
{
    gint freq;

    freq = gtk_combo_box_get_active(GTK_COMBO_BOX(apptw->Recur_freq_cb));
    if (freq != XFICAL_FREQ_NONE)
        gtk_widget_set_sensitive(apptw->Recur_until_rb, TRUE);
}

static void reurrence_set_visible (OrageAppointmentWindow *apptw)
{
    switch (gtk_combo_box_get_active (GTK_COMBO_BOX (apptw->Recur_freq_cb)))
    {
        case XFICAL_FREQ_NONE:
            gtk_stack_set_visible_child_name (
                    GTK_STACK (apptw->recurrence_frequency_box), RECURRENCE_NONE);
            gtk_widget_set_sensitive (apptw->recurrence_limit_box, FALSE);
            break;

        case XFICAL_FREQ_DAILY:
            gtk_stack_set_visible_child_name (
                    GTK_STACK (apptw->recurrence_frequency_box), RECURRENCE_DAILY);
            gtk_widget_set_sensitive (apptw->recurrence_limit_box, TRUE);
            break;

        case XFICAL_FREQ_WEEKLY:
            gtk_stack_set_visible_child_name (
                    GTK_STACK (apptw->recurrence_frequency_box),
                               RECURRENCE_WEEKLY);
            gtk_widget_set_sensitive (apptw->recurrence_limit_box, TRUE);
            break;

        case XFICAL_FREQ_MONTHLY:
            gtk_stack_set_visible_child_name (
                    GTK_STACK (apptw->recurrence_frequency_box),
                               RECURRENCE_MONTHLY);
            gtk_widget_set_sensitive (apptw->recurrence_limit_box, TRUE);
            break;

        case XFICAL_FREQ_YEARLY:
            gtk_stack_set_visible_child_name (
                    GTK_STACK (apptw->recurrence_frequency_box),
                               RECURRENCE_YEARLY);
            gtk_widget_set_sensitive (apptw->recurrence_limit_box, TRUE);
            break;

        case XFICAL_FREQ_HOURLY:
            gtk_stack_set_visible_child_name (
                    GTK_STACK (apptw->recurrence_frequency_box),
                               RECURRENCE_HOURLY);
            gtk_widget_set_sensitive (apptw->recurrence_limit_box, TRUE);
            break;

        default:
            g_assert_not_reached ();
            break;
    }
}

static void type_hide_show (OrageAppointmentWindow *apptw)
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
    set_time_sensitivity (apptw); /* todo has different settings */
}

static void readonly_hide_show (OrageAppointmentWindow *apptw)
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

static void set_sound_sensitivity (OrageAppointmentWindow *apptw)
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

static void set_notify_sensitivity (OrageAppointmentWindow *apptw)
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
#else
    /* Suppress compiler warning for unused parameter. */
    (void)apptw;
#endif
}

static void set_proc_sensitivity (OrageAppointmentWindow *apptw)
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
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    type_hide_show (apptw);
    mark_appointment_changed (apptw);
}

static void on_appTitle_entry_changed_cb (G_GNUC_UNUSED GtkEditable *entry
        , gpointer user_data)
{
    gchar *title, *application_name;
    const gchar *appointment_name;
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);

    appointment_name = gtk_entry_get_text (GTK_ENTRY (apptw->Title_entry));
    application_name = _("Orage");

    if (ORAGE_STR_EXISTS(appointment_name))
        title = g_strdup_printf("%s - %s", appointment_name, application_name);
    else
        title = g_strdup_printf("%s", application_name);

    gtk_window_set_title (GTK_WINDOW (apptw), title);

    g_free(title);
    mark_appointment_changed (apptw);
}

static void app_time_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb
        , gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    set_time_sensitivity (apptw);
    mark_appointment_changed (apptw);
}

static void refresh_recur_calendars (OrageAppointmentWindow *apptw)
{
    guint i;
    GtkCalendar *cal;
    xfical_appt *appt;

    appt = (xfical_appt *)apptw->xf_appt;
    if (apptw->appointment_changed)
        fill_appt_from_apptw (appt, apptw);

    for (i = 0; i < NR_OF_RECUR_CALENDARS; i++)
    {
        cal = GTK_CALENDAR (apptw->Recur_calendar[i]);
        xfical_mark_calendar_recur (cal, appt);
    }
}

static void on_notebook_page_switch (G_GNUC_UNUSED GtkNotebook *notebook,
                                     G_GNUC_UNUSED GtkWidget *page,
                                     guint page_num, gpointer user_data)
{
    if (page_num == 2)
        refresh_recur_calendars (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void app_recur_checkbutton_clicked_cb (
    G_GNUC_UNUSED GtkCheckButton *checkbutton, gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    set_repeat_sensitivity (apptw);
    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void on_app_recur_checkbutton_clicked_cb (
    G_GNUC_UNUSED GtkCheckButton *checkbutton, gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);

    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void app_sound_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb
        , gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    set_sound_sensitivity (apptw);
    mark_appointment_changed (apptw);
}

#ifdef HAVE_NOTIFY
static void app_notify_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb
        , gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    set_notify_sensitivity (apptw);
    mark_appointment_changed (apptw);
}
#endif

static void app_proc_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb
        , gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    set_proc_sensitivity (apptw);
    mark_appointment_changed (apptw);
}

static void app_checkbutton_clicked_cb (G_GNUC_UNUSED GtkCheckButton *cb,
                                        gpointer user_data)
{
    mark_appointment_changed (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void refresh_dependent_data (OrageAppointmentWindow *apptw)
{
    OrageApplication *app;

    if (apptw->el != NULL)
        refresh_el_win (apptw->el);
    if (apptw->dw != NULL)
        orage_week_window_refresh (apptw->dw);

    app = ORAGE_APPLICATION (g_application_get_default ());
    orage_mark_appointments (ORAGE_WINDOW (orage_application_get_window (app)));
}

static void on_appNote_buffer_changed_cb (G_GNUC_UNUSED GtkTextBuffer *b,
                                          gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
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

    mark_appointment_changed (apptw);
}

static void on_app_entry_changed_cb (G_GNUC_UNUSED GtkEditable *entry,
                                     gpointer user_data)
{
    mark_appointment_changed (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_freq_combobox_changed_cb (G_GNUC_UNUSED GtkComboBox *cb,
                                         gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
#if 0
    set_repeat_sensitivity (apptw);
#else
    reurrence_set_visible (apptw);
#endif
    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void on_app_combobox_changed_cb (G_GNUC_UNUSED GtkComboBox *cb,
                                        gpointer user_data)
{
    mark_appointment_changed (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_app_spin_button_changed_cb (G_GNUC_UNUSED GtkSpinButton *sb,
                                           gpointer user_data)
{
    mark_appointment_changed (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_recur_spin_button_changed_cb (G_GNUC_UNUSED GtkSpinButton *sb
        , gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);

    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void on_recur_combobox_changed_cb (G_GNUC_UNUSED GtkComboBox *cb,
                                          gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);

    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void on_appSound_button_clicked_cb (G_GNUC_UNUSED GtkButton *button,
                                           gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
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
    gint i;

    appSound_entry_filename = g_strdup(gtk_entry_get_text(
            GTK_ENTRY (apptw->Sound_entry)));
    file_chooser = gtk_file_chooser_dialog_new(_("Select a file..."),
            GTK_WINDOW (apptw),
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

    gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (file_chooser),
                                          PACKAGE_DATADIR "/orage/sounds", NULL);

    if (strlen (appSound_entry_filename) > 0)
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (file_chooser),
                                       (const gchar *)appSound_entry_filename);
    else
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (file_chooser),
                                             PACKAGE_DATADIR "/orage/sounds");

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

static void app_free_memory (OrageAppointmentWindow *apptw)
{
    gtk_widget_destroy (GTK_WIDGET (apptw));
}

static gboolean appWindow_check_and_close (OrageAppointmentWindow *apptw)
{
    gint result;

    if (apptw->appointment_changed == TRUE) {
        result = orage_warning_dialog (GTK_WINDOW (apptw)
                , _("The appointment information has been modified.")
                , _("Do you want to continue?")
                , _("No, do not leave")
                , _("Yes, ignore modifications and leave"));

        if (result == GTK_RESPONSE_YES) {
            app_free_memory (apptw);
        }
    }
    else {
        app_free_memory (apptw);
    }
    return(TRUE);
}

static gboolean on_appWindow_delete_event_cb (G_GNUC_UNUSED GtkWidget *widget,
                                              G_GNUC_UNUSED GdkEvent *event,
                                              gpointer user_data)
{
    return appWindow_check_and_close (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static gboolean orage_validate_datetime (OrageAppointmentWindow *apptw,
                                         xfical_appt *appt)
{
    /* Journal does not have end time so no need to check */
    if (appt->type == XFICAL_TYPE_JOURNAL
    ||  (appt->type == XFICAL_TYPE_TODO && !appt->use_due_time))
        return(TRUE);

    if (xfical_compare_times(appt) > 0) {
        orage_error_dialog(GTK_WINDOW (apptw)
            , _("The end of this appointment is earlier than the beginning.")
            , NULL);
        return(FALSE);
    }
    else {
        return(TRUE);
    }
}

static void fill_appt_from_recurrence_none (G_GNUC_UNUSED xfical_appt *appt)
{
}

static void fill_appt_from_recurrence_daily (xfical_appt *appt,
                                             OrageAppointmentWindow *apptw)
{
    appt->interval = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (apptw->recurrence_daily_interval_spin));
}

static void fill_appt_from_recurrence_weekly (xfical_appt *appt,
                                              OrageAppointmentWindow *apptw)
{
    guint i;

    for (i = 0; i <= 6; i++)
    {
        appt->recur_byday[i] = gtk_toggle_button_get_active (
                GTK_TOGGLE_BUTTON (apptw->recurrence_weekly_byday[i]));
    }

    appt->interval = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (apptw->recurrence_weekly_interval_spin));
}

static void fill_appt_from_recurrence_monthly (xfical_appt *appt,
                                               OrageAppointmentWindow *apptw)
{
    if (gtk_toggle_button_get_active (
        GTK_TOGGLE_BUTTON (apptw->recurrence_monthly_beginning_selector)))
    {
        appt->recur_month_type = XFICAL_RECUR_MONTH_TYPE_BEGIN;
        appt->recur_month_days = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (apptw->recurrence_monthly_begin_spin));
    }
    else if (gtk_toggle_button_get_active (
             GTK_TOGGLE_BUTTON (apptw->recurrence_monthly_end_selector)))
    {
        appt->recur_month_type = XFICAL_RECUR_MONTH_TYPE_END;
        appt->recur_month_days = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (apptw->recurrence_monthly_end_spin));
    }
    else if (gtk_toggle_button_get_active (
             GTK_TOGGLE_BUTTON (apptw->recurrence_monthly_every_selector)))
    {
        appt->recur_month_type = XFICAL_RECUR_MONTH_TYPE_EVERY;
        appt->recur_week_sel = gtk_combo_box_get_active (
                GTK_COMBO_BOX (apptw->recurrence_monthly_week_selector));
        appt->recur_day_sel = gtk_combo_box_get_active (
                GTK_COMBO_BOX (apptw->recurrence_monthly_day_selector));
    }
    else
        g_assert_not_reached ();
}

static void fill_appt_from_recurrence_yearly (xfical_appt *appt,
                                              OrageAppointmentWindow *apptw)
{
    appt->recur_week_sel = gtk_combo_box_get_active (
            GTK_COMBO_BOX (apptw->recurecnce_yearly_week_selector));
    appt->recur_day_sel = gtk_combo_box_get_active (
            GTK_COMBO_BOX (apptw->recurecnce_yearly_day_selector));
    appt->recur_month_sel = gtk_combo_box_get_active (
            GTK_COMBO_BOX (apptw->recurecnce_yearly_month_selector));
}

static void fill_appt_from_recurrence_hourly (xfical_appt *appt,
                                              OrageAppointmentWindow *apptw)
{
    appt->interval = gtk_spin_button_get_value_as_int (
            GTK_SPIN_BUTTON (apptw->recurrence_hourly_interval_spin));
}

static void fill_appt_from_apptw_alarm (xfical_appt *appt,
                                        OrageAppointmentWindow *apptw)
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

static void fill_appt_from_recurrence (xfical_appt *appt,
                                       OrageAppointmentWindow *apptw)
{
    GDateTime *gdt;
    GDateTime *gdt_tmp;
    gint year;
    gint month;
    gint day;

    appt->freq = gtk_combo_box_get_active (GTK_COMBO_BOX (apptw->Recur_freq_cb));

    /* recurrence ending */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (apptw->Recur_limit_rb)))
    {
        appt->recur_limit = XFICAL_RECUR_NO_LIMIT;
        appt->recur_count = 0;    /* special: means no repeat count limit */
    }
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (apptw->Recur_count_rb)))
    {
        appt->recur_limit = XFICAL_RECUR_COUNT;
        appt->recur_count = gtk_spin_button_get_value_as_int (
                GTK_SPIN_BUTTON (apptw->Recur_count_spin));
    }
    else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (apptw->Recur_until_rb)))
    {
        appt->recur_limit = XFICAL_RECUR_UNTIL;
        appt->recur_count = 0;    /* special: means no repeat count limit */

        gdt_tmp = g_object_get_data (G_OBJECT (apptw->Recur_until_button),
                                     DATE_KEY);
        g_date_time_get_ymd (gdt_tmp, &year, &month, &day);
        gdt = g_date_time_new_local (year, month, day, 23, 59, 10);
        orage_gdatetime_unref (appt->recur_until);
        appt->recur_until = g_date_time_ref (gdt);
        g_object_set_data_full (G_OBJECT (apptw->Recur_until_button),
                                DATE_KEY, gdt,
                                (GDestroyNotify)g_date_time_unref);
        gtk_widget_set_sensitive (apptw->Recur_until_button, TRUE);
    }
    else
        g_assert_not_reached ();

    /* recurrence todo base */
    appt->recur_todo_base_start = gtk_toggle_button_get_active (
            GTK_TOGGLE_BUTTON (apptw->Recur_todo_base_start_rb));

    /**************************************************************************/

    switch (appt->freq)
    {
        case XFICAL_FREQ_NONE:
            fill_appt_from_recurrence_none (appt);
            break;

        case XFICAL_FREQ_DAILY:
            fill_appt_from_recurrence_daily (appt, apptw);
            break;

        case XFICAL_FREQ_WEEKLY:
            fill_appt_from_recurrence_weekly (appt, apptw);
            break;

        case XFICAL_FREQ_MONTHLY:
            fill_appt_from_recurrence_monthly (appt, apptw);
            break;

        case XFICAL_FREQ_YEARLY:
            fill_appt_from_recurrence_yearly (appt, apptw);
            break;

        case XFICAL_FREQ_HOURLY:
            fill_appt_from_recurrence_hourly (appt, apptw);
            break;

        default:
            g_assert_not_reached ();
            break;
    }
}

/*
 * Function fills an appointment with the contents of an appointment 
 * window
 */
static gboolean fill_appt_from_apptw (xfical_appt *appt,
                                      OrageAppointmentWindow *apptw)
{
    GtkTextIter start, end;
    GTimeZone *gtz;
    GDateTime *gdt_tmp;
    gchar *tmp, *tmp2;

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
        g_assert_not_reached ();

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

    gdt_tmp = g_object_get_data (G_OBJECT (apptw->StartDate_button), DATE_KEY);
    gtz = g_date_time_get_timezone (gdt_tmp);
    orage_gdatetime_unref (appt->starttime);
    appt->starttime = g_date_time_new (gtz,
                           g_date_time_get_year (gdt_tmp),
                           g_date_time_get_month (gdt_tmp),
                           g_date_time_get_day_of_month (gdt_tmp),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->StartTime_spin_hh)),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->StartTime_spin_mm)),
                           0);

    /* end date and time.
     * Note that timezone is kept upto date all the time
     */
    appt->use_due_time = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->End_checkbutton));

    gdt_tmp = g_object_get_data (G_OBJECT (apptw->EndDate_button),
                                 DATE_KEY);
    gtz = g_date_time_get_timezone (gdt_tmp);
    orage_gdatetime_unref (appt->endtime);
    appt->endtime = g_date_time_new (gtz,
                           g_date_time_get_year (gdt_tmp),
                           g_date_time_get_month (gdt_tmp),
                           g_date_time_get_day_of_month (gdt_tmp),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->EndTime_spin_hh)),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->EndTime_spin_mm)),
                           0);

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
    if (!orage_validate_datetime (apptw, appt))
        return(FALSE);

    /* completed date and time.
     * Note that timezone is kept upto date all the time
     */
    appt->completed = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Completed_checkbutton));

    gdt_tmp = g_object_get_data (G_OBJECT (apptw->CompletedDate_button),
                                 DATE_KEY);
    gtz = g_date_time_get_timezone (gdt_tmp);
    orage_gdatetime_unref (appt->completedtime);
    appt->completedtime = g_date_time_new (gtz,
                           g_date_time_get_year (gdt_tmp),
                           g_date_time_get_month (gdt_tmp),
                           g_date_time_get_day_of_month (gdt_tmp),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->CompletedTime_spin_hh)),
                           gtk_spin_button_get_value_as_int (
                                    GTK_SPIN_BUTTON (apptw->CompletedTime_spin_mm)),
                           0);

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
    fill_appt_from_apptw_alarm (appt, apptw);

    /*********** RECURRENCE TAB ***********/
    fill_appt_from_recurrence (appt, apptw);

    return(TRUE);
}

static void add_file_select_cb (OrageAppointmentWindow *apptw)
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

    if (use_list)
    {
        /* Add insert file combobox to toolbar */
        (void)orage_toolbar_append_separator (apptw->Toolbar, -1);
        tool_item = GTK_WIDGET (gtk_tool_item_new ());

        gtk_container_add (GTK_CONTAINER (tool_item), apptw->File_insert_cb);
        gtk_toolbar_insert (GTK_TOOLBAR (apptw->Toolbar),
                            GTK_TOOL_ITEM (tool_item), -1);
    }
    else
    {
        /* It is not needed after all. */
        gtk_widget_destroy (apptw->File_insert_cb);
        apptw->File_insert_cb = NULL;
    }
}

static void remove_file_select_cb (OrageAppointmentWindow *apptw)
{
    if (apptw->File_insert_cb)
        gtk_widget_destroy(apptw->File_insert_cb);
}

static gboolean save_xfical_from_appt_win (OrageAppointmentWindow *apptw)
{
    gint i;
    gboolean ok = FALSE, found = FALSE;
    xfical_appt *appt = (xfical_appt *)apptw->xf_appt;
    char *xf_file_id, *tmp;

    if (fill_appt_from_apptw (appt, apptw)) {
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
                remove_file_select_cb (apptw);
            }
            else {
                g_warning ("%s: Addition failed: %s", G_STRFUNC, apptw->xf_uid);
                orage_error_dialog (GTK_WINDOW (apptw)
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
                orage_error_dialog (GTK_WINDOW (apptw)
                        , _("Appointment update failed.")
                        , _("Look more details from the log file. (Perhaps file was updated external from Orage?)"));
            }
        }
        xfical_file_close(TRUE);
        if (ok) {
            apptw->appointment_new = FALSE;
            mark_appointment_unchanged (apptw);
            refresh_dependent_data (apptw);
        }
    }
    return(ok);
}

static void on_appFileSave_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                             gpointer user_data)
{
    save_xfical_from_appt_win (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_appSave_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                   gpointer user_data)
{
    save_xfical_from_appt_win (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void save_xfical_from_appt_win_and_close (OrageAppointmentWindow *apptw)
{
    if (save_xfical_from_appt_win (apptw)) {
        app_free_memory (apptw);
    }
}

static void on_appFileSaveClose_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    save_xfical_from_appt_win_and_close (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_appSaveClose_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                        gpointer user_data)
{
    save_xfical_from_appt_win_and_close (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void delete_xfical_from_appt_win (OrageAppointmentWindow *apptw)
{
    gint result;
    gboolean ok = FALSE;

    result = orage_warning_dialog (GTK_WINDOW (apptw)
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

        refresh_dependent_data (apptw);
        app_free_memory (apptw);
    }
}

static void on_appDelete_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                     gpointer user_data)
{
    delete_xfical_from_appt_win (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_appFileDelete_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    delete_xfical_from_appt_win (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_appFileClose_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    appWindow_check_and_close (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void duplicate_xfical_from_appt_win (OrageAppointmentWindow *apptw)
{
    gint x, y;
    OrageAppointmentWindow *apptw2;

    apptw2 = (OrageAppointmentWindow *)orage_appointment_window_new_copy (apptw->xf_uid);

    if (apptw2) {
        gtk_window_get_position (GTK_WINDOW (apptw), &x, &y);
        gtk_window_move (GTK_WINDOW (apptw2), x+20, y+20);
    }
}

static void on_appFileDuplicate_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    duplicate_xfical_from_appt_win (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_appDuplicate_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                        gpointer user_data)
{
    duplicate_xfical_from_appt_win (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void revert_xfical_to_last_saved (OrageAppointmentWindow *apptw)
{
    GDateTime *gdt;
    if (!apptw->appointment_new) {
        gdt = g_date_time_new_now_local ();
        fill_appt_window (apptw, UPDATE_APPT_WIN, apptw->xf_uid, gdt);
        g_date_time_unref (gdt);
    }
    else {
        fill_appt_window (apptw, NEW_APPT_WIN, NULL, apptw->appointment_time);
    }
}

static void on_appFileRevert_menu_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi
        , gpointer user_data)
{
    revert_xfical_to_last_saved (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_appRevert_clicked_cb (G_GNUC_UNUSED GtkWidget *b,
                                     gpointer *user_data)
{
    revert_xfical_to_last_saved (ORAGE_APPOINTMENT_WINDOW (user_data));
}

static void on_Date_button_clicked_cb (GtkWidget *button, gpointer *user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    GtkWidget *selDate_dialog;

    selDate_dialog = gtk_dialog_new_with_buttons(
            _("Pick the date"), GTK_WINDOW(apptw),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("Today"), 1, "_OK", GTK_RESPONSE_ACCEPT, NULL);

    if (orage_date_button_clicked (button, selDate_dialog))
        mark_appointment_changed (apptw);
}

static void on_recur_Date_button_clicked_cb (GtkWidget *button
        , gpointer *user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    GtkWidget *selDate_dialog;

    selDate_dialog = gtk_dialog_new_with_buttons(
            _("Pick the date"), GTK_WINDOW(apptw),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("Today"), 1, "_OK", GTK_RESPONSE_ACCEPT, NULL);

    if (orage_date_button_clicked (button, selDate_dialog))
        mark_appointment_changed (apptw);

    refresh_recur_calendars (apptw);
}

static void on_appStartTimezone_clicked_cb (GtkButton *button
        , gpointer *user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    xfical_appt *appt;

    appt = (xfical_appt *)apptw->xf_appt;
    if (orage_timezone_button_clicked(button, GTK_WINDOW (apptw)
            , &appt->start_tz_loc, TRUE, g_par.local_timezone))
        mark_appointment_changed (apptw);
}

static void on_appEndTimezone_clicked_cb (GtkButton *button
        , gpointer *user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    xfical_appt *appt;

    appt = (xfical_appt *)apptw->xf_appt;
    if (orage_timezone_button_clicked(button, GTK_WINDOW(apptw)
            , &appt->end_tz_loc, TRUE, g_par.local_timezone))
        mark_appointment_changed (apptw);
}

static void on_appCompletedTimezone_clicked_cb (GtkButton *button
        , gpointer *user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    xfical_appt *appt;

    appt = (xfical_appt *)apptw->xf_appt;
    if (orage_timezone_button_clicked(button, GTK_WINDOW(apptw)
            , &appt->completed_tz_loc, TRUE, g_par.local_timezone))
        mark_appointment_changed (apptw);
}

static gint check_exists(gconstpointer a, gconstpointer b)
{
    const xfical_exception *ex_a = (const xfical_exception *)a;
    const xfical_exception *ex_b = (const xfical_exception *)b;
    GDateTime *time_a = xfical_exception_get_time (ex_a);
    GDateTime *time_b = xfical_exception_get_time (ex_b);
    xfical_exception_type type_a;
    xfical_exception_type type_b;

    /*  We actually only care about match or no match.*/
    if (g_date_time_compare (time_a, time_b) == 0) {
        type_a = xfical_exception_get_type (ex_a);
        type_b = xfical_exception_get_type (ex_b);
        return !(type_a == type_b);
    }
    else {
        return(1); /* does not matter if it is smaller or bigger */
    }
}

static void recur_row_clicked (GtkWidget *widget
        , GdkEventButton *event, gpointer *user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    xfical_appt *appt;
    gchar *time_str;
    GList *children;
    GtkWidget *lab;
    xfical_exception *recur_exception, *recur_exception_cur;
    GList *gl_pos;

    if (event->type == GDK_2BUTTON_PRESS) {
        /* first find the text */
        children = gtk_container_get_children(GTK_CONTAINER(widget));
        children = g_list_first(children);
        lab = GTK_WIDGET (children->data);
        recur_exception = xfical_exception_ref (
                g_object_get_data (G_OBJECT (lab), EXCEPTION_KEY));
        appt = apptw->xf_appt;
        if ((gl_pos = g_list_find_custom(appt->recur_exceptions
                    , recur_exception, check_exists))) {
            /* let's remove it */
            recur_exception_cur = gl_pos->data;
            appt->recur_exceptions =
                    g_list_remove(appt->recur_exceptions, recur_exception_cur);
            xfical_exception_unref (recur_exception_cur);
        }
        else {
            time_str = g_date_time_format (xfical_exception_get_time (
                    recur_exception), "%F %T");
            g_warning ("%s: non existent row (%s)", G_STRFUNC, time_str);
            g_free (time_str);
        }
        xfical_exception_unref (recur_exception);

        /* and finally update the display */
        gtk_widget_destroy(widget);
        mark_appointment_changed (apptw);
        refresh_recur_calendars (apptw);
    }
}

static gboolean add_recur_exception_row (xfical_exception *except,
                                           OrageAppointmentWindow *apptw,
                                           const gboolean only_window)
{
    GtkWidget *ev, *label;
    gchar *text;
    xfical_appt *appt;
    xfical_exception *recur_exception;
    GDateTime *p_time_gdt;
    xfical_exception_type p_type;

    text = xfical_exception_to_i18 (except);

    /* Then, let's keep the GList updated */
    if (!only_window) {
        appt = apptw->xf_appt;
        p_time_gdt = xfical_exception_get_time (except);
        p_type = xfical_exception_get_type (except);
        recur_exception = xfical_exception_new (p_time_gdt, appt->allDay, p_type);
        appt = apptw->xf_appt;
        if (g_list_find_custom(appt->recur_exceptions, recur_exception
                    , check_exists)) {
            /* this element is already in the list, so no need to add it again.
             * we just clean the memory and leave */
            xfical_exception_unref (recur_exception);
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
    g_object_set_data_full (G_OBJECT (label),
                            EXCEPTION_KEY, xfical_exception_ref (except),
                            (GDestroyNotify)xfical_exception_unref);
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

static void recur_month_changed_cb (GtkCalendar *calendar, gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    xfical_appt *appt = (xfical_appt *)apptw->xf_appt;

    /* actually we do not have to do fill_appt_from_apptw always,
     * but as we are not keeping track of changes, we just do it always */
    fill_appt_from_apptw (appt, apptw);
    xfical_mark_calendar_recur (calendar, appt);
}

static void recur_day_selected_double_click_cb (GtkCalendar *calendar
        , gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    xfical_exception_type type;
    GDateTime *gdt;
    gint hh, mm;
    gboolean all_day;
    xfical_exception *except;

    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(apptw->Recur_exception_excl_rb))) {
        type = EXDATE;
        /* need to add time also as standard libical can not handle dates
           correctly yet. Check more from BUG 5764.
           We use start time from appointment. */
        if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(apptw->AllDay_checkbutton)))
        {
            gdt = orage_cal_to_gdatetime (calendar, 1, 1);
            all_day = TRUE;
        }
        else {
            hh =  gtk_spin_button_get_value_as_int(
                    GTK_SPIN_BUTTON(apptw->StartTime_spin_hh));
            mm =  gtk_spin_button_get_value_as_int(
                    GTK_SPIN_BUTTON(apptw->StartTime_spin_mm));
            gdt = orage_cal_to_gdatetime (calendar, hh, mm);
            all_day = FALSE;
        }
    }
    else { /* extra day. This needs also time */
        type = RDATE;
        hh =  gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Recur_exception_incl_spin_hh));
        mm =  gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(apptw->Recur_exception_incl_spin_mm));
        gdt = orage_cal_to_gdatetime (calendar, hh, mm);
        all_day = FALSE;
    }

    except = xfical_exception_new (gdt, all_day, type);
    g_date_time_unref (gdt);

    if (add_recur_exception_row (except, apptw, FALSE)) { /* new data */
        mark_appointment_changed (apptw);
        refresh_recur_calendars (apptw);
    }

    xfical_exception_unref (except);
}

static void fill_appt_window_times (OrageAppointmentWindow *apptw,
                                      xfical_appt *appt)
{
    gchar *date_to_display;
    int days, hours, minutes;
    GDateTime *gdt;

    /* all day ? */
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(apptw->AllDay_checkbutton), appt->allDay);

    /* start time */
    gdt = g_date_time_ref (appt->starttime);
    g_object_set_data_full (G_OBJECT (apptw->StartDate_button),
                            DATE_KEY, gdt,
                            (GDestroyNotify)g_date_time_unref);
    date_to_display = orage_gdatetime_to_i18_time (gdt, TRUE);
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

    /* end time */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            apptw->End_checkbutton), appt->use_due_time);
    if (appt->endtime) {
        gdt = g_date_time_ref (appt->endtime);
        g_object_set_data_full (G_OBJECT (apptw->EndDate_button),
                                DATE_KEY, gdt,
                                (GDestroyNotify)g_date_time_unref);
        date_to_display = orage_gdatetime_to_i18_time (gdt, TRUE);
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
    if (appt->completedtime) {
        gdt = g_date_time_ref (appt->completedtime);
        g_object_set_data_full (G_OBJECT (apptw->CompletedDate_button),
                                DATE_KEY, gdt,
                                (GDestroyNotify)g_date_time_unref);
        date_to_display = orage_gdatetime_to_i18_time (gdt, TRUE);
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

static xfical_appt *fill_appt_window_get_new_appt (GDateTime *par_gdt)
{
    xfical_appt *appt;
    GDateTime *gdt_now;
    gint hour;
    gint minute;
    gint par_year;
    gint par_month;
    gint par_day_of_month;
    gint start_hour;
    gint end_hour;
    gint start_minute;
    gint end_minute;

    appt = xfical_appt_new_day (par_gdt);
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

    orage_gdatetime_unref (appt->starttime);
    appt->starttime = g_date_time_new_local (par_year, par_month,
                                              par_day_of_month, start_hour,
                                              start_minute, 0);

    orage_gdatetime_unref (appt->endtime);
    appt->endtime = g_date_time_new_local (par_year, par_month,
                                            par_day_of_month, end_hour,
                                            end_minute, 0);

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
    orage_gdatetime_unref (appt->completedtime);
    appt->completedtime = gdt_now;
    appt->completed_tz_loc = g_strdup (appt->start_tz_loc);

    read_default_alarm (appt);

    return appt;
}

static xfical_appt *fill_appt_window_update_appt (OrageAppointmentWindow *apptw,
                                                  const gchar *uid)
{
    xfical_appt *appt;

    g_return_val_if_fail (uid != NULL, NULL);

    if (!xfical_file_open (TRUE))
        return NULL;

    appt = xfical_appt_get (uid);
    if (appt == NULL)
    {
        orage_info_dialog (GTK_WINDOW (apptw),
                _("This appointment does not exist."),
                _("It was probably removed, please refresh your screen."));
    }

    xfical_file_close (TRUE);

    return appt;
}

static xfical_appt *fill_appt_window_get_appt (OrageAppointmentWindow *apptw,
                                               const appt_win_action action,
                                               const gchar *uid,
                                               GDateTime *par_gdt)
{
    xfical_appt *appt;

    switch (action)
    {
        case NEW_APPT_WIN:
            appt = fill_appt_window_get_new_appt (par_gdt);
            break;

        case UPDATE_APPT_WIN:
        case COPY_APPT_WIN:
            appt = fill_appt_window_update_appt (apptw, uid);
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    return(appt);
}

/************************************************************/
/* categories start.                                        */
/************************************************************/

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

static void orage_category_refill_cb (OrageAppointmentWindow *apptw)
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

static void fill_category_data (OrageAppointmentWindow *apptw,
                                  xfical_appt *appt)
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

static void close_cat_window(gpointer user_data)
{
    category_win_struct *catw = (category_win_struct *)user_data;

    orage_category_refill_cb (catw->apptw);
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
    GList *orage_category_list;
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

    orage_category_list = orage_category_get_list ();
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
    category_win_struct *catw;

    catw = g_new(category_win_struct, 1);
    catw->apptw = ORAGE_APPOINTMENT_WINDOW (user_data); /* remember the caller */
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

static void fill_appt_window_general (OrageAppointmentWindow *apptw,
                                        xfical_appt *appt,
                                        const appt_win_action action)
{
    gchar *copy_str;
    gint i;

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

    if (action == COPY_APPT_WIN)
    {
        copy_str = g_strconcat (" ", _("*** COPY ***"), NULL);
        gtk_editable_set_position(GTK_EDITABLE(apptw->Title_entry), -1);
        i = gtk_editable_get_position(GTK_EDITABLE(apptw->Title_entry));
        gtk_editable_insert_text (GTK_EDITABLE (apptw->Title_entry), copy_str,
                                 strlen (copy_str), &i);
        g_free (copy_str);
    }

    /* location */
    gtk_entry_set_text(GTK_ENTRY(apptw->Location_entry)
            , (appt->location ? appt->location : ""));

    /* times */
    fill_appt_window_times (apptw, appt);

    /* availability */
    if (appt->availability != -1) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(apptw->Availability_cb)
               , appt->availability);
    }

    /* categories */
    fill_category_data (apptw, appt);

    /* priority */
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(apptw->Priority_spin)
            , (gdouble)appt->priority);

    /* note */
    gtk_text_buffer_set_text (apptw->Note_buffer,
                              (appt->note ? appt->note : ""), -1);
}

static void fill_appt_window_alarm (OrageAppointmentWindow *apptw,
                                      xfical_appt *appt)
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
#if 0
    char *when_array[4] = {
    _("Before Start"), _("Before End"), _("After Start"), _("After End")};
#endif
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
    gtk_entry_set_text (GTK_ENTRY (apptw->Sound_entry),
                        (appt->sound ? appt->sound : PACKAGE_DATADIR "/orage/sounds/Spo.wav"));

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

static void fill_appt_window_monthly_recurrence (OrageAppointmentWindow *apptw,
                                                 const xfical_appt *appt)
{
    gtk_combo_box_set_active (
            GTK_COMBO_BOX (apptw->recurrence_monthly_week_selector),
            appt->recur_week_sel);

    switch (appt->recur_month_type)
    {
        case XFICAL_RECUR_MONTH_TYPE_BEGIN:
            gtk_toggle_button_set_active (
                    GTK_TOGGLE_BUTTON (apptw->recurrence_monthly_beginning_selector),
                    TRUE);
            gtk_spin_button_set_value (
                    GTK_SPIN_BUTTON (apptw->recurrence_monthly_begin_spin),
                    appt->recur_month_days);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_begin_spin,
                                      TRUE);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_end_spin,
                                      FALSE);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_week_selector,
                                      FALSE);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_day_selector,
                                      FALSE);
            break;

        case XFICAL_RECUR_MONTH_TYPE_END:
            gtk_toggle_button_set_active (
                    GTK_TOGGLE_BUTTON (apptw->recurrence_monthly_end_selector),
                    TRUE);
            gtk_spin_button_set_value (
                    GTK_SPIN_BUTTON (apptw->recurrence_monthly_end_spin),
                    appt->recur_month_days);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_begin_spin,
                                      FALSE);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_end_spin,
                                      TRUE);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_week_selector,
                                      FALSE);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_day_selector,
                                      FALSE);
            break;

        case XFICAL_RECUR_MONTH_TYPE_EVERY:
            gtk_toggle_button_set_active (
                    GTK_TOGGLE_BUTTON (apptw->recurrence_monthly_every_selector),
                    TRUE);
            gtk_combo_box_set_active (
                    GTK_COMBO_BOX (apptw->recurrence_monthly_week_selector),
                    appt->recur_week_sel);
            gtk_combo_box_set_active (
                    GTK_COMBO_BOX (apptw->recurrence_monthly_day_selector),
                    appt->recur_day_sel);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_begin_spin,
                                      FALSE);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_end_spin,
                                      FALSE);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_week_selector,
                                      TRUE);
            gtk_widget_set_sensitive (apptw->recurrence_monthly_day_selector,
                                      TRUE);
            break;

        default:
            g_assert_not_reached ();
    }
}

static void fill_appt_window_yearly_recurrence (OrageAppointmentWindow *apptw,
                                                const xfical_appt *appt)
{
    gtk_combo_box_set_active (
            GTK_COMBO_BOX (apptw->recurecnce_yearly_week_selector),
            appt->recur_week_sel);
    gtk_combo_box_set_active (
            GTK_COMBO_BOX (apptw->recurecnce_yearly_day_selector),
            appt->recur_day_sel);
    gtk_combo_box_set_active (
            GTK_COMBO_BOX (apptw->recurecnce_yearly_month_selector),
            appt->recur_month_sel);
}

static void fill_appt_window_recurrence (OrageAppointmentWindow *apptw,
                                         xfical_appt *appt)
{
    gchar *untildate_to_display;
    gdouble recur_count;
    GList *tmp;
    GDateTime *gdt;
    xfical_exception *recur_exception;
    gint i;

    gtk_combo_box_set_active(GTK_COMBO_BOX(apptw->Recur_freq_cb), appt->freq);
    switch(appt->recur_limit) {
        case XFICAL_RECUR_NO_LIMIT:
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(apptw->Recur_limit_rb), TRUE);
            recur_count = 1;
            gdt = g_date_time_new_now_local ();
            break;

        case XFICAL_RECUR_COUNT:
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(apptw->Recur_count_rb), TRUE);
            gtk_widget_set_sensitive (apptw->Recur_count_spin, TRUE);
            recur_count = appt->recur_count;
            gdt = g_date_time_new_now_local ();
            break;

        case XFICAL_RECUR_UNTIL:
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(apptw->Recur_until_rb), TRUE);
            gtk_widget_set_sensitive (apptw->Recur_until_button, TRUE);
            recur_count = 1;
            gdt = g_date_time_ref (appt->recur_until);
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (apptw->Recur_count_spin),
                               recur_count);
    untildate_to_display = orage_gdatetime_to_i18_time (gdt, TRUE);
    g_object_set_data_full (G_OBJECT (apptw->Recur_until_button),
                            DATE_KEY, gdt,
                            (GDestroyNotify)g_date_time_unref);
    gtk_button_set_label (GTK_BUTTON (apptw->Recur_until_button),
                          untildate_to_display);
    g_free (untildate_to_display);

    gtk_combo_box_set_active (
            GTK_COMBO_BOX (apptw->recurrence_monthly_day_selector),
            appt->recur_day_sel);

    gtk_spin_button_set_value (
            GTK_SPIN_BUTTON (apptw->recurrence_daily_interval_spin),
            appt->interval);
    gtk_spin_button_set_value (
            GTK_SPIN_BUTTON (apptw->recurrence_weekly_interval_spin),
            appt->interval);
    gtk_spin_button_set_value (
            GTK_SPIN_BUTTON (apptw->recurrence_hourly_interval_spin),
            appt->interval);

    fill_appt_window_monthly_recurrence (apptw, appt);
    fill_appt_window_yearly_recurrence (apptw, appt);

    /* weekdays */
    for (i=0; i <= 6; i++) {
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(apptw->recurrence_weekly_byday[i])
                        , appt->recur_byday[i]);
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
        add_recur_exception_row (recur_exception, apptw, TRUE);
    }
    /* note: include times is setup in the fill_appt_window_times */
}

/** Fill appointment window with data. */
static void fill_appt_window (OrageAppointmentWindow *apptw,
                                const appt_win_action action,
                                const gchar *par, GDateTime *gdt_par)
{
    const gchar *action_str;
    xfical_appt *appt;
    gchar *appointment_id;

    /********************* INIT *********************/
    appt = fill_appt_window_get_appt (apptw, action, par, gdt_par);
    g_return_if_fail (appt != NULL);

    apptw->xf_appt = appt;

    switch (action)
    {
        case NEW_APPT_WIN:
            action_str = "NEW";
            apptw->appointment_add = TRUE;
            apptw->appointment_new = TRUE;
            appointment_id = orage_gdatetime_to_i18_time (gdt_par, TRUE);
            break;

        case UPDATE_APPT_WIN:
            action_str = "UPDATE";
            apptw->appointment_add = FALSE;
            apptw->appointment_new = FALSE;
            appointment_id = g_strdup (par);
            break;

        case COPY_APPT_WIN:
            action_str = "COPY";

            /* COPY uses old uid as base and adds new, so
             * add == TRUE && new == FALSE
             */
            apptw->appointment_add = TRUE;
            apptw->appointment_new = FALSE;

            /* New copy is never readonly even though the original may have
             * been.
             */
            appt->readonly = FALSE;
            appointment_id = g_strdup (par);
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    g_message ("%s appointment: '%s'", action_str, appointment_id);
    g_free (appointment_id);
    apptw->xf_uid = g_strdup(appt->uid);
    g_date_time_unref (apptw->appointment_time);
    apptw->appointment_time = g_date_time_ref (gdt_par);
    apptw->appointment_changed = FALSE;

    if (apptw->appointment_add) {
        add_file_select_cb(apptw);
    }
    if (!appt->completed) { /* some nice default */
        orage_gdatetime_unref (appt->completedtime);
        appt->completedtime = g_date_time_new_now_local (); /* probably completed today? */
        g_free(appt->completed_tz_loc);
        appt->completed_tz_loc = g_strdup(appt->start_tz_loc);
    }
    /* we only want to enable duplication if we are working with an old
     * existing app (=not adding new) */
    gtk_widget_set_sensitive(apptw->Duplicate, !apptw->appointment_add);
    gtk_widget_set_sensitive(apptw->File_menu_duplicate
            , !apptw->appointment_add);

    /* window title */
    gtk_window_set_title(GTK_WINDOW(apptw), _("New appointment - Orage"));

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
}

static void build_menu (OrageAppointmentWindow *apptw)
{
    /* Menu bar */
    apptw->Menubar = gtk_menu_bar_new();
    gtk_grid_attach (GTK_GRID (apptw->Vbox), apptw->Menubar, 1, 1, 1, 1);

    /* File menu stuff */
    apptw->File_menu = orage_menu_new(_("_File"), apptw->Menubar);

    apptw->File_menu_save = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-save", apptw->File_menu, apptw->accel_group);

    apptw->File_menu_saveclose =
            orage_menu_item_new_with_mnemonic(_("Sav_e and close")
                    , apptw->File_menu);
    gtk_widget_add_accelerator(apptw->File_menu_saveclose
            , "activate", apptw->accel_group
            , GDK_KEY_w, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

    (void)orage_separator_menu_item_new(apptw->File_menu);

    apptw->File_menu_revert = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-revert-to-saved", apptw->File_menu, apptw->accel_group);

    apptw->File_menu_duplicate =
            orage_menu_item_new_with_mnemonic(_("D_uplicate")
                    , apptw->File_menu);
    gtk_widget_add_accelerator(apptw->File_menu_duplicate
            , "activate", apptw->accel_group
            , GDK_KEY_d, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    (void)orage_separator_menu_item_new(apptw->File_menu);

    apptw->File_menu_delete = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-delete", apptw->File_menu, apptw->accel_group);

    (void)orage_separator_menu_item_new(apptw->File_menu);

    apptw->File_menu_close = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-close", apptw->File_menu, apptw->accel_group);

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
    appt->sound = orage_rc_get_str (orc, "SOUND", PACKAGE_DATADIR "/orage/sounds/Spo.wav");
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
    gboolean all_day;
    gchar *start_str;
    gchar *end_str;

    if (g_date_time_compare (appt->starttime, appt->endtime) == 0)
    {
        action_time = orage_gdatetime_to_i18_time (appt->starttime, FALSE);
        return action_time;
    }

    if (appt->allDay == TRUE)
    {
        if (is_same_date (appt->starttime, appt->endtime) == TRUE)
        {
            action_time = orage_gdatetime_to_i18_time (appt->starttime, TRUE);
            return action_time;
        }
        all_day = TRUE;
    }
    else
        all_day = FALSE;

    start_str = orage_gdatetime_to_i18_time (appt->starttime, all_day);
    end_str = orage_gdatetime_to_i18_time (appt->endtime, all_day);

    action_time = g_strconcat (start_str, " - ", end_str, NULL);
    g_free (start_str);
    g_free (end_str);

    return action_time;
}

static void on_test_button_clicked_cb (G_GNUC_UNUSED GtkButton *button
        , gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    xfical_appt *appt = (xfical_appt *)apptw->xf_appt;
    alarm_struct *cur_alarm = orage_alarm_new ();

    fill_appt_from_apptw (appt, apptw);

    /* no need for alarm time as we are doing this now */
    cur_alarm->alarm_time = NULL;
    cur_alarm->uid = (appt->uid) ? g_strdup (appt->uid) : NULL;
    cur_alarm->action_time = create_action_time (appt);

    cur_alarm->title = g_strdup (appt->title);
    cur_alarm->description = g_strdup (appt->note);
    cur_alarm->persistent = appt->alarm_persistent;
    cur_alarm->display_orage = appt->display_alarm_orage;
    cur_alarm->display_notify = appt->display_alarm_notify;
    cur_alarm->notify_timeout = appt->display_notify_timeout;
    cur_alarm->audio = appt->sound_alarm;
    cur_alarm->sound = (appt->sound) ? g_strdup (appt->sound) : NULL;
    cur_alarm->repeat_cnt = appt->soundrepeat_cnt;
    cur_alarm->repeat_delay = appt->soundrepeat_len;
    cur_alarm->procedure = appt->procedure_alarm;
    cur_alarm->cmd = (appt->procedure_alarm)
                   ? g_strconcat (appt->procedure_cmd, " ",
                                  appt->procedure_params, NULL)
                   : NULL;

    create_reminders (cur_alarm);
    orage_alarm_unref (cur_alarm);
}

static void on_appDefault_save_button_clicked_cb (
    G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    xfical_appt *appt = (xfical_appt *)apptw->xf_appt;

    fill_appt_from_apptw_alarm (appt, apptw);
    store_default_alarm(appt);
}

static void on_appDefault_read_button_clicked_cb (
    G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    xfical_appt *appt = (xfical_appt *)apptw->xf_appt;

    read_default_alarm(appt);
    fill_appt_window_alarm(apptw, appt);
}

static void on_recur_monthly_begin_toggled_cb (GtkToggleButton *button,
                                               gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    const gboolean enabled =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    gtk_widget_set_sensitive (apptw->recurrence_monthly_begin_spin, enabled);

    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void on_recur_monthly_end_toggled_cb (GtkToggleButton *button,
                                             gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    const gboolean enabled =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    gtk_widget_set_sensitive (apptw->recurrence_monthly_end_spin, enabled);

    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void on_recur_monthly_every_toggled_cb (GtkToggleButton *button,
                                               gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    const gboolean enabled =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    gtk_widget_set_sensitive (apptw->recurrence_monthly_week_selector, enabled);
    gtk_widget_set_sensitive (apptw->recurrence_monthly_day_selector, enabled);

    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void on_recur_limit_toggled_cb (GtkToggleButton *button,
                                       gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    const gboolean enabled =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    if (enabled)
    {
        gtk_widget_set_sensitive (apptw->Recur_count_spin, FALSE);
        gtk_widget_set_sensitive (apptw->Recur_until_button, FALSE);
    }

    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void on_recur_count_toggled_cb (GtkToggleButton *button,
                                       gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    const gboolean enabled =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    if (enabled)
    {
        gtk_widget_set_sensitive (apptw->Recur_count_spin, TRUE);
        gtk_widget_set_sensitive (apptw->Recur_until_button, FALSE);
    }

    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static void on_recur_until_toggled_cb (GtkToggleButton *button,
                                       gpointer user_data)
{
    OrageAppointmentWindow *apptw = ORAGE_APPOINTMENT_WINDOW (user_data);
    const gboolean enabled =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    if (enabled)
    {
        gtk_widget_set_sensitive (apptw->Recur_count_spin, FALSE);
        gtk_widget_set_sensitive (apptw->Recur_until_button, TRUE);
    }

    mark_appointment_changed (apptw);
    refresh_recur_calendars (apptw);
}

static GtkWidget *create_week_combo_box (void)
{
    const gchar *week_list[5] =
    {
        _("first"), _("second"), _("third"), _("fourth"), _("last")
    };

    return orage_create_combo_box_with_content (week_list, 5);
}

static GtkWidget *create_weekday_combo_box (void)
{
    const gchar *weekday_list[7] =
    {
        _("Monday"), _("Tuesday"), _("Wednesday"), _("Thursday"), _("Friday"),
        _("Saturday"), _("Sunday")
    };

    return orage_create_combo_box_with_content (weekday_list, 7);
}

static GtkWidget *build_recurrence_box_daily (OrageAppointmentWindow *apptw)
{
    GtkBox *box;
    GtkWidget *box_widget;
    GtkWidget *repeat_days_label1;
    GtkWidget *repeat_days_label2;

    repeat_days_label1 = gtk_label_new (_("Every"));
    apptw->recurrence_daily_interval_spin =
            gtk_spin_button_new_with_range (1, 100, 1);
    gtk_spin_button_set_wrap (
            GTK_SPIN_BUTTON (apptw->recurrence_daily_interval_spin), TRUE);
    repeat_days_label2 = gtk_label_new (_("day(s)"));
    box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_pack_start (box, repeat_days_label1, FALSE, FALSE, 0);
    gtk_box_pack_start (box, apptw->recurrence_daily_interval_spin,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (box, repeat_days_label2, FALSE, FALSE, 0);

    box_widget = (GtkWidget *)box;
    gtk_widget_set_visible (box_widget, TRUE);

    return box_widget;
}

static GtkWidget *build_recurrence_box_weekly (OrageAppointmentWindow *apptw)
{
    const gchar *weekday_array[7] =
    {
        _("Mon"), _("Tue"), _("Wed"), _("Thu"), _("Fri"), _("Sat"), _("Sun")
    };

    guint i;
    GtkBox *box;
    GtkWidget *box_widget;
    GtkBox *weekday_box;
    GtkBox *repeat_weeks_box;
    GtkWidget *repeat_weeks_label1;
    GtkWidget *repeat_weeks_label2;

    repeat_weeks_label1 = gtk_label_new (_("Every"));
    apptw->recurrence_weekly_interval_spin =
            gtk_spin_button_new_with_range (1, 100, 1);
    g_object_set (apptw->recurrence_weekly_interval_spin, "valign",
                  GTK_ALIGN_CENTER, "vexpand", FALSE, NULL);
    gtk_spin_button_set_wrap (
            GTK_SPIN_BUTTON (apptw->recurrence_weekly_interval_spin), TRUE);
    repeat_weeks_label2 = gtk_label_new (_("week(s) on:"));
    repeat_weeks_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 7));
    gtk_box_pack_start (repeat_weeks_box, repeat_weeks_label1, FALSE, FALSE, 0);
    gtk_box_pack_start (repeat_weeks_box,
                        apptw->recurrence_weekly_interval_spin, FALSE, FALSE,0);
    gtk_box_pack_start (repeat_weeks_box, repeat_weeks_label2, FALSE, FALSE, 0);

    weekday_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
    for (i = 0; i <= 6; i++)
    {
        apptw->recurrence_weekly_byday[i] =
                gtk_check_button_new_with_mnemonic (weekday_array[i]);
        gtk_box_pack_start (weekday_box, apptw->recurrence_weekly_byday[i],
                            FALSE, FALSE, 7);
    }

    box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 7));
    gtk_box_pack_start (box, GTK_WIDGET (repeat_weeks_box), FALSE, FALSE, 0);
    gtk_box_pack_start (box, GTK_WIDGET (weekday_box), FALSE, FALSE, 0);

    box_widget = (GtkWidget *)box;
    gtk_widget_set_visible (box_widget, TRUE);

    return box_widget;
}

static GtkWidget *build_recurrence_box_monthly (OrageAppointmentWindow *apptw)
{
    GtkBox *box;
    GtkWidget *box_widget;
    GtkBox *beginning_box;
    GtkBox *end_box;
    GtkBox *every_box;

    apptw->recurrence_monthly_beginning_selector = (
            gtk_radio_button_new_with_label (NULL,
            _("From the beginning of the month:")));
    apptw->recurrence_monthly_begin_spin =
            gtk_spin_button_new_with_range (1, MAX_MONTH_RECURRENCE_DAY, 1);
    beginning_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_box_pack_start (beginning_box,
                        apptw->recurrence_monthly_beginning_selector, FALSE,
                        FALSE, 0);
    gtk_box_pack_start (beginning_box, apptw->recurrence_monthly_begin_spin,
                        FALSE, FALSE, 0);

    apptw->recurrence_monthly_end_selector =
            gtk_radio_button_new_with_mnemonic_from_widget (
            GTK_RADIO_BUTTON (apptw->recurrence_monthly_beginning_selector),
            _("From the end of the month:"));
    apptw->recurrence_monthly_end_spin =
            gtk_spin_button_new_with_range (1, 28, 1);
    gtk_widget_set_sensitive (apptw->recurrence_monthly_end_spin, FALSE);
    end_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
    gtk_box_pack_start (end_box, apptw->recurrence_monthly_end_selector,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (end_box, apptw->recurrence_monthly_end_spin,
                        FALSE, FALSE, 0);

    apptw->recurrence_monthly_every_selector =
            gtk_radio_button_new_with_mnemonic_from_widget (
            GTK_RADIO_BUTTON (apptw->recurrence_monthly_beginning_selector),
            _("Every:"));
    apptw->recurrence_monthly_week_selector = create_week_combo_box ();
    apptw->recurrence_monthly_day_selector = create_weekday_combo_box ();
    gtk_widget_set_sensitive (apptw->recurrence_monthly_week_selector, FALSE);
    gtk_widget_set_sensitive (apptw->recurrence_monthly_day_selector, FALSE);
    every_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_pack_start (every_box, apptw->recurrence_monthly_every_selector,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (every_box, apptw->recurrence_monthly_week_selector,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (every_box, apptw->recurrence_monthly_day_selector,
                        FALSE, FALSE, 0);

    box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 7));
    gtk_box_pack_start (box, GTK_WIDGET (beginning_box), FALSE, FALSE, 0);
    gtk_box_pack_start (box, GTK_WIDGET (end_box), FALSE, FALSE, 0);
    gtk_box_pack_start (box, GTK_WIDGET (every_box), FALSE, FALSE, 0);

    box_widget = (GtkWidget *)box;
    gtk_widget_set_visible (box_widget, TRUE);

    return box_widget;
}

static GtkWidget *build_recurrence_box_yearly (OrageAppointmentWindow *apptw)
{
    const gchar *month_list[12] =
    {
        _("January"), _("February"), _("March"), _("April"), _("May"),
        _("June"), _("July"), _("August"), _("September"), _("October"),
        _("November"), _("December")
    };

    GtkWidget *box_widget;
    GtkWidget *separator_label2;
    GtkBox *box;
    GtkWidget *every_label;

    every_label = gtk_label_new (_("Every"));
    apptw->recurecnce_yearly_week_selector = create_week_combo_box ();
    apptw->recurecnce_yearly_day_selector = create_weekday_combo_box ();

    /* TRANSLATORS: this string is part of date line, for example
     * "second Thursday of August". In some languages ​​it is not necessary to use
     * the string "of", it can be replaced with an empty string.
     */
    separator_label2 = gtk_label_new (_("of"));
    apptw->recurecnce_yearly_month_selector =
            orage_create_combo_box_with_content (month_list, 12);
    box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_pack_start (box, every_label, FALSE, FALSE, 0);
    gtk_box_pack_start (box, apptw->recurecnce_yearly_week_selector,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (box, apptw->recurecnce_yearly_day_selector,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (box, separator_label2, FALSE, FALSE, 0);
    gtk_box_pack_start (box, apptw->recurecnce_yearly_month_selector,
                        FALSE, FALSE, 0);

    box_widget = (GtkWidget *)box;
    gtk_widget_set_visible (box_widget, TRUE);

    return box_widget;
}

static GtkWidget *build_recurrence_box_hourly (OrageAppointmentWindow *apptw)
{
    GtkBox *box;
    GtkWidget *box_widget;
    GtkWidget *repeat_hours_label1;
    GtkWidget *repeat_hours_label2;

    repeat_hours_label1 = gtk_label_new (_("Every"));
    apptw->recurrence_hourly_interval_spin =
            gtk_spin_button_new_with_range (1, 24, 1);
    gtk_spin_button_set_wrap (
            GTK_SPIN_BUTTON (apptw->recurrence_hourly_interval_spin), TRUE);
    repeat_hours_label2 = gtk_label_new (_("hour(s)"));

    box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5));
    gtk_box_pack_start (box, repeat_hours_label1, FALSE, FALSE, 0);
    gtk_box_pack_start (box, apptw->recurrence_hourly_interval_spin,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (box, repeat_hours_label2, FALSE, FALSE, 0);

    box_widget = (GtkWidget *)box;
    gtk_widget_set_visible (box_widget, TRUE);

    return box_widget;
}

static GtkWidget *build_recurrence_box_none (void)
{
    GtkWidget *box;
    GtkWidget *box_widget;

    box = gtk_label_new ("");

    box_widget = (GtkWidget *)box;
    gtk_widget_set_visible (box_widget, TRUE);

    return box_widget;
}

static GtkWidget *build_limits_box (OrageAppointmentWindow *apptw)
{
    GtkBox *box;
    GtkBox *limit_repeat_box;
    GtkBox *limit_until_box;
    GtkWidget *limit_repeat_label;

    box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20));
    apptw->Recur_limit_rb =
            gtk_radio_button_new_with_label (NULL, _("Repeat forever"));
    gtk_box_pack_start (box, apptw->Recur_limit_rb, FALSE, FALSE, 0);

    limit_repeat_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5));
    apptw->Recur_count_rb = gtk_radio_button_new_with_mnemonic_from_widget (
            GTK_RADIO_BUTTON (apptw->Recur_limit_rb), _("Repeat"));
    gtk_box_pack_start (limit_repeat_box, apptw->Recur_count_rb,
                        FALSE, FALSE, 0);
    apptw->Recur_count_spin = gtk_spin_button_new_with_range (1, 100, 1);
    gtk_widget_set_sensitive (apptw->Recur_count_spin, FALSE);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (apptw->Recur_count_spin), TRUE);
    gtk_box_pack_start (limit_repeat_box, apptw->Recur_count_spin,
                        FALSE, FALSE, 0);
    limit_repeat_label = gtk_label_new (_("times"));
    gtk_box_pack_start (limit_repeat_box, limit_repeat_label, FALSE, FALSE, 0);
    gtk_box_pack_start (box, GTK_WIDGET (limit_repeat_box), FALSE, FALSE, 0);

    limit_until_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5));
    apptw->Recur_until_rb = gtk_radio_button_new_with_mnemonic_from_widget(
            GTK_RADIO_BUTTON (apptw->Recur_limit_rb), _("Repeat until"));
    gtk_box_pack_start (limit_until_box, apptw->Recur_until_rb,
                        FALSE, FALSE, 0);
    apptw->Recur_until_button = gtk_button_new ();
    gtk_widget_set_sensitive (apptw->Recur_until_button, FALSE);
    gtk_box_pack_start (limit_until_box, apptw->Recur_until_button,
                        FALSE, FALSE, 0);
    gtk_box_pack_start (box, GTK_WIDGET (limit_until_box), FALSE, FALSE, 0);

    return (GtkWidget *)box;
}

static GtkWidget *build_todo_box_cell (OrageAppointmentWindow *apptw)
{
    GtkBox *box;

    box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20));
    apptw->Recur_todo_base_start_rb =
            gtk_radio_button_new_with_label (NULL, _("Start"));
    gtk_box_pack_start (box, apptw->Recur_todo_base_start_rb, FALSE, FALSE, 0);
    apptw->Recur_todo_base_done_rb =
            gtk_radio_button_new_with_mnemonic_from_widget (
                GTK_RADIO_BUTTON (apptw->Recur_todo_base_start_rb),
                _("Completed"));
    gtk_box_pack_start (box, apptw->Recur_todo_base_done_rb, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text (apptw->Recur_todo_base_start_rb,
        _("TODO reoccurs regularly starting on start time and repeating after each interval no matter when it was last completed"));
    gtk_widget_set_tooltip_text (apptw->Recur_todo_base_done_rb,
        _("TODO reoccurrency is based on complete time and repeats after the interval counted from the last completed time.\n(Note that you can not tell anything about the history of the TODO since reoccurrence base changes after each completion.)"));

    g_signal_connect (apptw->Recur_todo_base_start_rb, "clicked",
                      G_CALLBACK (app_recur_checkbutton_clicked_cb), apptw);
    g_signal_connect (apptw->Recur_todo_base_done_rb, "clicked",
                      G_CALLBACK (app_recur_checkbutton_clicked_cb), apptw);

    return (GtkWidget *)box;
}

static GtkWidget *align_box_contents (GtkWidget *box)
{
    g_object_set (box, "valign", GTK_ALIGN_START, NULL);

    return box;
}

static void build_toolbar (OrageAppointmentWindow *apptw)
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

    g_signal_connect ((gpointer)apptw->Save, "clicked"
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

static void build_general_page (OrageAppointmentWindow *apptw)
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
    apptw->Type_label = gtk_label_new(_("Type"));
    hbox = gtk_grid_new ();
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

    orage_table_add_row (apptw->TableGeneral, apptw->Type_label, hbox, row = 0,
                         OTBL_EXPAND | OTBL_FILL, 0);

    /* title */
    apptw->Title_label = gtk_label_new (_("Title"));
    apptw->Title_entry = gtk_entry_new();
    orage_table_add_row (apptw->TableGeneral, apptw->Title_label,
                         apptw->Title_entry, ++row, OTBL_EXPAND | OTBL_FILL, 0);

    /* location */
    apptw->Location_label = gtk_label_new(_("Location"));
    apptw->Location_entry = gtk_entry_new();
    orage_table_add_row (apptw->TableGeneral, apptw->Location_label,
                         apptw->Location_entry, ++row,
                         OTBL_EXPAND | OTBL_FILL, 0);

    /* All day */
    apptw->AllDay_checkbutton =
            gtk_check_button_new_with_mnemonic(_("All day event"));
    orage_table_add_row (apptw->TableGeneral, NULL, apptw->AllDay_checkbutton,
                         ++row, OTBL_EXPAND | OTBL_FILL, 0);

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
    orage_table_add_row (apptw->TableGeneral, apptw->Start_label,
                         apptw->StartTime_hbox, ++row,
                         OTBL_SHRINK | OTBL_FILL, OTBL_SHRINK | OTBL_FILL);

    /* end time */
    apptw->End_label = gtk_label_new(_("End"));
    apptw->End_hbox = gtk_grid_new ();
    apptw->End_checkbutton =
            gtk_check_button_new_with_mnemonic(_("Set"));
    g_object_set (apptw->End_checkbutton, "margin-right", 20, NULL);
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
    orage_table_add_row (apptw->TableGeneral, apptw->End_label, apptw->End_hbox,
                         ++row, OTBL_SHRINK | OTBL_FILL,
                         OTBL_SHRINK | OTBL_FILL);

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
    orage_table_add_row (apptw->TableGeneral, NULL, apptw->Dur_hbox, ++row,
                         OTBL_FILL, OTBL_FILL);

    /* Availability (only for EVENT) */
    apptw->Availability_label = gtk_label_new(_("Availability"));
    apptw->Availability_cb = orage_create_combo_box_with_content(
            availability_array, 2);
    orage_table_add_row (apptw->TableGeneral, apptw->Availability_label,
                         apptw->Availability_cb, ++row, OTBL_FILL, OTBL_FILL);

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
    orage_table_add_row (apptw->TableGeneral, apptw->Completed_label,
                         apptw->Completed_hbox, ++row, OTBL_FILL, OTBL_FILL);

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
    orage_table_add_row (apptw->TableGeneral, apptw->Categories_label,
                         apptw->Categories_hbox, ++row,
                         OTBL_EXPAND | OTBL_FILL, 0);

    /* priority */
    apptw->Priority_label = gtk_label_new(_("Priority"));
    apptw->Priority_spin = gtk_spin_button_new_with_range(0, 9, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(apptw->Priority_spin), TRUE);
    gtk_widget_set_tooltip_text(apptw->Priority_spin
            , _("If you set this 8 or bigger, the appointment is NOT shown on list windows.\nYou can use that feature to unclutter your list windows, but it makes it more difficult to find this appointment.\n(Alarms will still fire normally.)\n(There is undocumented parameter so that you can change the default limit of 8.)"));
    hbox = gtk_grid_new ();
    gtk_grid_attach_next_to (GTK_GRID (hbox), apptw->Priority_spin, NULL,
                             GTK_POS_RIGHT, 1, 1);
    orage_table_add_row (apptw->TableGeneral, apptw->Priority_label, hbox,
                         ++row, OTBL_FILL, OTBL_FILL);

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
    orage_table_add_row (apptw->TableGeneral, apptw->Note, event, ++row,
                         OTBL_EXPAND | OTBL_FILL, OTBL_EXPAND | OTBL_FILL);

    gtk_widget_set_tooltip_text(event
            , _("These shorthand commands take effect immediately:\n    <D> inserts current date in local date format\n    <T> inserts time\n    <DT> inserts date and time.\n\nThese are converted only later when they are seen:\n    <&Ynnnn> is translated to current year minus nnnn.\n(This can be used for example in birthday reminders to tell how old the person will be.)"));

    /* Take care of the title entry to build the appointment window title */
    g_signal_connect((gpointer)apptw->Title_entry, "changed"
            , G_CALLBACK(on_appTitle_entry_changed_cb), apptw);
}

static void enable_general_page_signals (OrageAppointmentWindow *apptw)
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

static void build_alarm_page (OrageAppointmentWindow *apptw)
{
    gboolean audio_enabled;
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

    orage_table_add_row (apptw->TableAlarm, apptw->Alarm_label,
                         apptw->Alarm_hbox, row = 0, OTBL_FILL, OTBL_FILL);
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

    orage_table_add_row (apptw->TableAlarm, NULL, apptw->Per_hbox, ++row,
                         OTBL_FILL, OTBL_FILL);

    /***** Audio Alarm *****/
    audio_enabled = ORAGE_STR_EXISTS (g_par.sound_application);
    apptw->Sound_label = gtk_label_new(_("Sound"));

    apptw->Sound_hbox = gtk_grid_new ();
    apptw->Sound_checkbutton =
            gtk_check_button_new_with_mnemonic(_("Use"));

    if (audio_enabled)
    {
        gtk_widget_set_tooltip_text (apptw->Sound_checkbutton,
                                    _("Select this if you want audible alarm"));
    }
    else
    {
        gtk_widget_set_tooltip_text (apptw->Sound_hbox,
                                     _("Sound command is not set"));
    }

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
    gtk_widget_set_sensitive (GTK_WIDGET (apptw->Sound_hbox), audio_enabled);
    gtk_grid_attach_next_to (GTK_GRID (apptw->Sound_hbox),
                             apptw->Sound_button, NULL, GTK_POS_RIGHT,
                             1, 1);

    orage_table_add_row (apptw->TableAlarm, apptw->Sound_label,
                         apptw->Sound_hbox, ++row, OTBL_FILL, OTBL_FILL);

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

    orage_table_add_row (apptw->TableAlarm, NULL, apptw->SoundRepeat_hbox,
                         ++row, OTBL_EXPAND | OTBL_FILL, 0);

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

    orage_table_add_row (apptw->TableAlarm, apptw->Display_label,
                         apptw->Display_hbox_orage, ++row,
                         OTBL_EXPAND | OTBL_FILL, 0);

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

    orage_table_add_row (apptw->TableAlarm, NULL, apptw->Display_hbox_notify,
                         ++row, OTBL_EXPAND | OTBL_FILL, 0);
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

    orage_table_add_row (apptw->TableAlarm, apptw->Proc_label, apptw->Proc_hbox,
                         ++row, OTBL_FILL, OTBL_FILL);

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

static void enable_alarm_page_signals (OrageAppointmentWindow *apptw)
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

static void build_recurrence_page (OrageAppointmentWindow *apptw)
{
    const gchar *recur_freq_array[RECUR_FREQ_ARRAY_ELEMENTS] =
    {
        _("None"), _("Daily"), _("Weekly"),
        _("Monthly"), _("Yearly"), _("Hourly")
    };

    guint row = 0;
    guint y, m;
    guint i;

    GtkWidget *recur_table;
    GtkWidget *limit_label;
    GtkWidget *frequency_label;
    GtkGrid *frequency_box;
    GtkCalendar *calendar;

    recur_table = orage_table_new (BORDER_SIZE);
    apptw->Recur_notebook_page = recur_table;
    apptw->Recur_tab_label = gtk_label_new (_("Recurrence"));

    gtk_notebook_append_page (GTK_NOTEBOOK (apptw->Notebook),
                              apptw->Recur_notebook_page,
                              apptw->Recur_tab_label);

    /******************************* Frequency ********************************/
    frequency_label = gtk_label_new (_("Frequency"));
    frequency_box = (GtkGrid *)gtk_grid_new ();
    apptw->Recur_freq_cb = orage_create_combo_box_with_content (
            recur_freq_array, RECUR_FREQ_ARRAY_ELEMENTS);
    gtk_grid_attach_next_to (frequency_box,
                             apptw->Recur_freq_cb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    orage_table_add_row (recur_table, frequency_label,
                         GTK_WIDGET (frequency_box), row++,
                         OTBL_EXPAND | OTBL_FILL, OTBL_FILL);

    /****************************** Recurrence ********************************/
    apptw->recurrence_frequency_box = GTK_STACK (gtk_stack_new ());
    gtk_stack_add_named (apptw->recurrence_frequency_box,
                         build_recurrence_box_none (), RECURRENCE_NONE);
    gtk_stack_add_named (apptw->recurrence_frequency_box,
                         align_box_contents (build_recurrence_box_daily (apptw)),
                         RECURRENCE_DAILY);
    gtk_stack_add_named (apptw->recurrence_frequency_box,
                         align_box_contents (build_recurrence_box_weekly (apptw)),
                         RECURRENCE_WEEKLY);
    gtk_stack_add_named (apptw->recurrence_frequency_box,
                         align_box_contents (build_recurrence_box_monthly (apptw)),
                         RECURRENCE_MONTHLY);
    gtk_stack_add_named (apptw->recurrence_frequency_box,
                         align_box_contents (build_recurrence_box_yearly (apptw)),
                         RECURRENCE_YEARLY);
    gtk_stack_add_named (apptw->recurrence_frequency_box,
                         align_box_contents (build_recurrence_box_hourly (apptw)),
                         RECURRENCE_HOURLY);
#if 0
    gtk_stack_set_transition_duration (apptw->recurrence_frequency_box, 200);
    gtk_stack_set_transition_type (pptw->recurrence_limit_box,
                                   GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);
#endif

    orage_table_add_row (recur_table, NULL,
                         GTK_WIDGET (apptw->recurrence_frequency_box), row++,
                         OTBL_EXPAND | OTBL_FILL, 0);

    /******************************* Limit ************************************/
    limit_label = gtk_label_new (_("Limit"));
    apptw->recurrence_limit_box = build_limits_box (apptw);
    orage_table_add_row (recur_table, limit_label, apptw->recurrence_limit_box,
                         row++, OTBL_EXPAND | OTBL_FILL, 0);

    /******************************* TODO base (only for TODOs) ***************/
    apptw->Recur_todo_base_label = gtk_label_new (_("TODO base"));
    apptw->Recur_todo_base_hbox = build_todo_box_cell (apptw);
    orage_table_add_row (recur_table, apptw->Recur_todo_base_label,
                         apptw->Recur_todo_base_hbox, row++,
                         OTBL_EXPAND | OTBL_FILL, 0);

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

    orage_table_add_row (recur_table, apptw->Recur_exception_label,
                         apptw->Recur_exception_hbox, ++row,
                         OTBL_EXPAND | OTBL_FILL, 0);

    /* calendars showing the action days */
    apptw->Recur_calendar_label = gtk_label_new(_("Action dates"));
    apptw->Recur_calendar_hbox = gtk_grid_new ();

    for (i = 0; i < NR_OF_RECUR_CALENDARS; i++)
    {
        apptw->Recur_calendar[i] = gtk_calendar_new ();
        calendar = GTK_CALENDAR (apptw->Recur_calendar[i]);

        gtk_calendar_set_display_options (calendar,
                                          GTK_CALENDAR_SHOW_HEADING |
                                          GTK_CALENDAR_SHOW_DAY_NAMES);

        gtk_calendar_select_day (calendar, 0);

        gtk_grid_attach_next_to (GTK_GRID (apptw->Recur_calendar_hbox),
                                 apptw->Recur_calendar[i], NULL,
                                 GTK_POS_RIGHT, 1, 1);
    }

    gtk_calendar_get_date (GTK_CALENDAR (apptw->Recur_calendar[0]),
                           &y, &m, NULL);
    for (i = 1; i < NR_OF_RECUR_CALENDARS; i++)
    {
        if (++m > 11)
        {
            m = 0;
            y++;
        }

        gtk_calendar_select_month (GTK_CALENDAR (apptw->Recur_calendar[i]),
                                   m, y);
    }

    orage_table_add_row (recur_table, apptw->Recur_calendar_label,
                         apptw->Recur_calendar_hbox, ++row,
                         OTBL_EXPAND | OTBL_FILL, 0);
}

static void enable_recurrence_page_signals (OrageAppointmentWindow *apptw)
{
    guint i;

    g_signal_connect (apptw->Recur_freq_cb, "changed",
                      G_CALLBACK (on_freq_combobox_changed_cb), apptw);

    for (i = 0; i < NR_OF_RECUR_CALENDARS; i++)
    {
        g_signal_connect (apptw->Recur_calendar[i], "month-changed",
                          G_CALLBACK (recur_month_changed_cb), apptw);

        g_signal_connect (apptw->Recur_calendar[i], "day_selected_double_click",
                      G_CALLBACK (recur_day_selected_double_click_cb), apptw);
    }

    g_signal_connect (apptw->Recur_limit_rb, "toggled",
                      G_CALLBACK (on_recur_limit_toggled_cb), apptw);
    g_signal_connect (apptw->Recur_count_rb, "toggled",
                      G_CALLBACK (on_recur_count_toggled_cb), apptw);
    g_signal_connect (apptw->Recur_until_rb, "toggled",
                      G_CALLBACK (on_recur_until_toggled_cb), apptw);
    g_signal_connect (apptw->Recur_count_spin, "value-changed",
                      G_CALLBACK (on_recur_spin_button_changed_cb), apptw);
    g_signal_connect (apptw->Recur_until_button, "clicked",
                      G_CALLBACK (on_recur_Date_button_clicked_cb), apptw);
    g_signal_connect (apptw->recurrence_daily_interval_spin, "value-changed",
                      G_CALLBACK (on_recur_spin_button_changed_cb), apptw);
    g_signal_connect (apptw->recurrence_hourly_interval_spin, "value-changed",
                      G_CALLBACK (on_recur_spin_button_changed_cb), apptw);

    g_signal_connect (apptw->recurrence_monthly_beginning_selector, "toggled",
                      G_CALLBACK (on_recur_monthly_begin_toggled_cb), apptw);
    g_signal_connect (apptw->recurrence_monthly_end_selector, "toggled",
                      G_CALLBACK (on_recur_monthly_end_toggled_cb), apptw);
    g_signal_connect (apptw->recurrence_monthly_every_selector, "toggled",
                      G_CALLBACK (on_recur_monthly_every_toggled_cb), apptw);

    g_signal_connect (apptw->recurrence_monthly_begin_spin, "value-changed",
                      G_CALLBACK (on_recur_spin_button_changed_cb), apptw);
    g_signal_connect (apptw->recurrence_monthly_end_spin, "value-changed",
                      G_CALLBACK (on_recur_spin_button_changed_cb), apptw);
    g_signal_connect (apptw->recurrence_monthly_week_selector, "changed",
                      G_CALLBACK (on_recur_combobox_changed_cb), apptw);
    g_signal_connect (apptw->recurrence_monthly_day_selector, "changed",
                      G_CALLBACK (on_recur_combobox_changed_cb), apptw);
    g_signal_connect (apptw->recurrence_weekly_interval_spin, "value-changed",
                      G_CALLBACK (on_recur_spin_button_changed_cb), apptw);
    g_signal_connect (apptw->recurecnce_yearly_week_selector, "changed",
                      G_CALLBACK (on_recur_combobox_changed_cb), apptw);
    g_signal_connect (apptw->recurecnce_yearly_day_selector, "changed",
                      G_CALLBACK (on_recur_combobox_changed_cb), apptw);
    g_signal_connect (apptw->recurecnce_yearly_month_selector, "changed",
                      G_CALLBACK (on_recur_combobox_changed_cb), apptw);

    for (i = 0; i <= 6; i++)
    {
        g_signal_connect (apptw->recurrence_weekly_byday[i], "clicked",
                          G_CALLBACK (on_app_recur_checkbutton_clicked_cb),
                          apptw);
    }
}

static void orage_appointment_window_constructed (GObject *object)
{
    OrageAppointmentWindow *self = (OrageAppointmentWindow *)object;

    fill_appt_window (self, self->action, self->par, self->appointment_time_2);
    enable_general_page_signals (self);
    enable_alarm_page_signals (self);
    enable_recurrence_page_signals (self);
    gtk_widget_show_all (GTK_WIDGET (self));
    reurrence_set_visible (self);
    type_hide_show (self);
    readonly_hide_show (self);
    gtk_widget_grab_focus (self->Title_entry);

    G_OBJECT_CLASS (orage_appointment_window_parent_class)->constructed (object);
}

static void orage_appointment_window_finalize (GObject *object)
{
    OrageAppointmentWindow *self = (OrageAppointmentWindow *)object;

    if (self->el)
    {
        /* Remove myself from event list appointment monitoring list. */
        self->el->apptw_list = g_list_remove (self->el->apptw_list, self);
    }
    else if (self->dw)
    {
        /* Remove myself from day list appointment monitoring list. */
        orage_week_window_remove_appointment_window (self->dw, self);
    }

    g_free (self->xf_uid);
    xfical_appt_free ((xfical_appt *)self->xf_appt);

    g_date_time_unref (self->appointment_time_2);
    g_free (self->par);
    G_OBJECT_CLASS (orage_appointment_window_parent_class)->finalize (object);
}

static void orage_appointment_window_get_property (GObject *object,
                                                    const guint prop_id,
                                                    GValue *value,
                                                    GParamSpec *pspec)
{
    const OrageAppointmentWindow *self = ORAGE_APPOINTMENT_WINDOW (object);

    switch (prop_id)
    {
        case PROP_APPOINTMENT_TIME:
            g_value_set_boxed (value, self->appointment_time_2);
            break;

        case PROP_APPOINTMENT_ACTION:
            g_value_set_uint (value, self->action);
            break;

        case PROP_APPOINTMENT_PAR:
            g_value_set_string (value, self->par);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void orage_appointment_window_set_property (GObject *object,
                                                   const guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec)
{
    const gchar *par;
    OrageAppointmentWindow *self = ORAGE_APPOINTMENT_WINDOW (object);

    switch (prop_id)
    {
        case PROP_APPOINTMENT_TIME:
            g_assert (self->appointment_time_2 == NULL);
            self->appointment_time_2 = (GDateTime *)g_value_dup_boxed (value);
            break;

        case PROP_APPOINTMENT_ACTION:
            self->action = g_value_get_uint (value);
            break;

        case PROP_APPOINTMENT_PAR:
            par = g_value_get_string (value);
            if (g_strcmp0 (par, self->par) != 0)
            {
                g_free (self->par);
                self->par = g_strdup (par);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void orage_appointment_window_class_init (OrageAppointmentWindowClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->constructed = orage_appointment_window_constructed;
    object_class->finalize = orage_appointment_window_finalize;
    object_class->get_property = orage_appointment_window_get_property;
    object_class->set_property = orage_appointment_window_set_property;

    properties[PROP_APPOINTMENT_TIME] =
            g_param_spec_boxed (APPOINTMENT_TIME_PROPERTY,
                                APPOINTMENT_TIME_PROPERTY,
                                "An GDateTime for appointment time",
                                G_TYPE_DATE_TIME,
                                G_PARAM_STATIC_STRINGS |
                                G_PARAM_READWRITE |
                                G_PARAM_CONSTRUCT_ONLY |
                                G_PARAM_EXPLICIT_NOTIFY);

    properties[PROP_APPOINTMENT_ACTION] =
            g_param_spec_uint (APPOINTMENT_ACTION_PROPERTY,
                               APPOINTMENT_ACTION_PROPERTY,
                               "Appointment window action",
                               NEW_APPT_WIN, COPY_APPT_WIN, NEW_APPT_WIN,
                               G_PARAM_STATIC_STRINGS |
                               G_PARAM_READWRITE |
                               G_PARAM_CONSTRUCT_ONLY |
                               G_PARAM_EXPLICIT_NOTIFY);

    properties[PROP_APPOINTMENT_PAR] =
            g_param_spec_string (APPOINTMENT_PARAMETER_PROPERTY,
                                 APPOINTMENT_PARAMETER_PROPERTY,
                                 "Appointment window parameter",
                                 NULL,
                                 G_PARAM_STATIC_STRINGS |
                                 G_PARAM_READWRITE |
                                 G_PARAM_CONSTRUCT_ONLY |
                                 G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void orage_appointment_window_init (OrageAppointmentWindow *self)
{
    self->xf_uid = NULL;
    self->appointment_time = g_date_time_new_now_local ();
    self->xf_appt = NULL;
    self->el = NULL;
    self->dw = NULL;
    self->appointment_changed = FALSE;
    self->accel_group = gtk_accel_group_new ();

    gtk_window_add_accel_group (GTK_WINDOW (self), self->accel_group);
    self->Vbox = gtk_grid_new ();

    gtk_container_add (GTK_CONTAINER (self), self->Vbox);

    build_menu (self);
    build_toolbar (self);

    /* ********** Here begins tabs ********** */
    self->Notebook = gtk_notebook_new ();
    gtk_grid_attach (GTK_GRID (self->Vbox), self->Notebook, 1, 3, 1, 1);
    gtk_container_set_border_width (GTK_CONTAINER (self->Notebook), 5);

    build_general_page (self);
    build_alarm_page (self);
    build_recurrence_page (self);

    g_signal_connect (self, "delete-event",
                      G_CALLBACK (on_appWindow_delete_event_cb), self);
    g_signal_connect (self->Notebook, "switch-page",
                      G_CALLBACK (on_notebook_page_switch), self);
}

static GtkWidget *orage_appointment_window_new_by_uid (const gchar *uid,
                                                       const appt_win_action action)
{
    GtkWidget *window;
    GDateTime *gdt;

    gdt = g_date_time_new_now_local ();
    window = g_object_new (ORAGE_TYPE_APPOINTMENT_WINDOW,
                           APPOINTMENT_TIME_PROPERTY, gdt,
                           APPOINTMENT_ACTION_PROPERTY, action,
                           APPOINTMENT_PARAMETER_PROPERTY, uid,
                           NULL);
    g_date_time_unref (gdt);

    return window;
}

GtkWidget *orage_appointment_window_new_now (void)
{
    GDateTime *gdt;
    GtkWidget *window;

    gdt = g_date_time_new_now_local ();
    window = orage_appointment_window_new (gdt);
    g_date_time_unref (gdt);

    return window;
}

GtkWidget *orage_appointment_window_new (GDateTime *gdt)
{
    return g_object_new (ORAGE_TYPE_APPOINTMENT_WINDOW,
                         APPOINTMENT_TIME_PROPERTY, gdt,
                         APPOINTMENT_ACTION_PROPERTY, NEW_APPT_WIN,
                         NULL);
}

GtkWidget *orage_appointment_window_new_copy (const gchar *uid)
{
    return orage_appointment_window_new_by_uid (uid, COPY_APPT_WIN);
}

GtkWidget *orage_appointment_window_new_update (const gchar *uid)
{
    return orage_appointment_window_new_by_uid (uid, UPDATE_APPT_WIN);
}

void orage_appointment_window_set_event_list (OrageAppointmentWindow *apptw,
                                              el_win *el)
{
    apptw->el = el;
}

void orage_appointment_window_set_day_window (OrageAppointmentWindow *apptw,
                                              gpointer dw)
{
    apptw->dw = ORAGE_WEEK_WINDOW (dw);
}
