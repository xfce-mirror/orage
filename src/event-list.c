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
#include <config.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <unistd.h>
#include <time.h>

#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "event-list.h"
#include "functions.h"
#include "ical-code.h"
#include "orage-appointment-window.h"
#include "orage-category.h"
#include "orage-i18n.h"
#include "orage-week-window.h"
#include "orage-window.h"
#include "parameters.h"
#include "reminder.h"

#ifdef HAVE_X11_TRAY_ICON
#include "tray_icon.h"
#endif

#define BORDER_SIZE 10
#define DATE_KEY "button-date"

enum {
    COL_TIME = 0
   ,COL_FLAGS
   ,COL_HEAD
   ,COL_UID
   ,COL_SORT
   ,CAL_CATEGORIES
   ,NUM_COLS
};

enum {
    DRAG_TARGET_STRING = 0
   ,DRAG_TARGET_COUNT
};
static const GtkTargetEntry drag_targets[] =
{
    { "STRING", 0, DRAG_TARGET_STRING }
};

static void start_appt_win (const gboolean copy_appt_win, el_win *el
        , GtkTreeModel *model, GtkTreeIter *iter, GtkTreePath *path)
{
    GtkWidget *apptw;
    gchar *uid = NULL, *flags = NULL;

    if (gtk_tree_model_get_iter(model, iter, path)) {
        gtk_tree_model_get(model, iter, COL_UID, &uid, -1);
#ifdef HAVE_ARCHIVE
        gtk_tree_model_get(model, iter, COL_FLAGS, &flags, -1);
        if (flags && flags[3] == 'A') {
            xfical_unarchive_uid(uid);
            /* note that file id changes after archive */
            uid[0]='O';
            refresh_el_win(el);
        }
        g_free(flags);
#endif
        if (copy_appt_win)
            apptw = orage_appointment_window_new_copy (uid);
        else
            apptw = orage_appointment_window_new_update (uid);

        g_free(uid);

        /* inform the appointment that we are interested in it */
        orage_appointment_window_set_event_list (
                ORAGE_APPOINTMENT_WINDOW (apptw), el);

        gtk_window_present (GTK_WINDOW (apptw));

        /* we started this, so keep track of it */
        el->apptw_list = g_list_prepend (el->apptw_list, apptw);
    }
}

static void editEvent(GtkTreeView *view, GtkTreePath *path
        , G_GNUC_UNUSED GtkTreeViewColumn *col, gpointer user_data)
{
    el_win *el = (el_win *)user_data;
    GtkTreeModel *model;
    GtkTreeIter   iter;

    model = gtk_tree_view_get_model(view);
    start_appt_win (FALSE, el, model, &iter, path);
}

static gint sortEvent_comp(GtkTreeModel *model
        , GtkTreeIter *i1, GtkTreeIter *i2, gpointer data)
{
    gint col = GPOINTER_TO_INT(data);
    gint ret;
    gchar *text1, *text2;

    gtk_tree_model_get(model, i1, col, &text1, -1);
    gtk_tree_model_get(model, i2, col, &text2, -1);
    ret = strcmp(text1, text2);
    g_free(text1);
    g_free(text2);
    return(ret);
}

static void append_time (gchar *result, GDateTime *gdt, const size_t len)
{
    gchar *ical_time;

    ical_time = g_date_time_format (gdt, "%R");
    g_snprintf (result, len, "%s ", ical_time);
    g_free (ical_time);
}

static gchar *format_time(el_win *el, xfical_appt *appt, GDateTime *gdt_par)
{
    gchar *result;
    const size_t result_len = 51;
    gchar *tmp;
    int i = 0;
    gboolean same_date;

    result = g_new0(gchar, result_len);

    if (el->page == EVENT_PAGE && el->days == 0) {
        /* special formatting for 1 day VEVENTS */
        if (appt->allDay == FALSE) { /* time part available */
            if (gdt_par && (orage_gdatetime_compare_date (appt->starttimecur, gdt_par) < 0))
                g_strlcpy (result, "+00:00 ", result_len);
            else
                append_time (&result[i], appt->starttimecur, result_len - i);
            i = g_strlcat (result, "- ", result_len);
            if ((gdt_par == NULL) || (orage_gdatetime_compare_date (gdt_par, appt->endtimecur) < 0))
                g_strlcat (result, "24:00+", result_len);
            else
                append_time (&result[i], appt->endtimecur, result_len - i);
        }
        else {/* date only appointment */
            g_strlcpy (result, _("All day"), result_len);
        }
    }
    else { /* normally show date and time */
        tmp = orage_gdatetime_to_i18_time (appt->starttimecur, TRUE);
        i = g_strlcpy(result, tmp, result_len);
        g_free (tmp);
        if (appt->allDay == FALSE) { /* time part available */
            result[i++] = ' ';
            append_time (&result[i], appt->starttimecur, result_len - i);
            i = g_strlcat (result, "- ", result_len);
            if (el->page == TODO_PAGE && !appt->use_due_time) {
                g_strlcat (result, "...", result_len);
            }
            else {
                same_date = !orage_gdatetime_compare_date (appt->starttimecur,
                                                           appt->endtimecur);
                if (!same_date) {
                    tmp = orage_gdatetime_to_i18_time (appt->endtimecur, TRUE);
                    i = g_strlcat (result, tmp, result_len);
                    g_free (tmp);
                    result[i++] = ' ';
                }
                append_time (&result[i], appt->endtimecur, result_len - i);
            }
        }
        else {/* date only */
            g_strlcat(result, " - ", result_len);
            if (el->page == TODO_PAGE && !appt->use_due_time) {
                g_strlcat(result, "...", result_len);
            }
            else {
                tmp = orage_gdatetime_to_i18_time (appt->endtimecur, TRUE);
                g_strlcat(result, tmp, result_len);
                g_free (tmp);
            }
        }
    }

    return(result);
}

static void flags_data_func (G_GNUC_UNUSED GtkTreeViewColumn *col,
                             GtkCellRenderer *rend,
                             GtkTreeModel *model,
                             GtkTreeIter *iter,
                             G_GNUC_UNUSED gpointer user_data)
{
    gchar *categories;
    GdkRGBA *color;

    gtk_tree_model_get(model, iter, CAL_CATEGORIES, &categories, -1);
    if ((color = orage_category_list_contains(categories)) == NULL)
    {
        g_object_set(rend
                 , "background-set",    FALSE
                 , NULL);
    }
    else
    {
        g_object_set(rend
                 , "background-rgba",   color
                 , "background-set",    TRUE
                 , NULL);
    }

    g_free(categories);
}

static void start_time_data_func (G_GNUC_UNUSED GtkTreeViewColumn *col,
                                  GtkCellRenderer *rend,
                                  GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  gpointer user_data)
{
    el_win *el = (el_win *)user_data;
    gchar *stime, *etime, *stime2;
    gchar start_time[17], end_time[17];
    gint len;
    GDateTime *gdt_start_time;
    GDateTime *gdt_end_time;
    GTimeZone *utc_tz;

    if (el->page == EVENT_PAGE) {
        if (!el->today || el->days != 0) { /* reset */
            g_object_set(rend
                     , "foreground-set",    FALSE
                     , "strikethrough-set", FALSE
                     , "weight-set",        FALSE
                     , NULL);
            return;
        }

        gtk_tree_model_get(model, iter, COL_TIME, &stime, -1);
        /* we need to remember the real address in case we increment it,
         * so that we can free the correct pointer */
        stime2 = stime;
        if (stime[0] == '+')
            stime++;
        etime = stime + 8; /* hh:mm - hh:mm */
        /* only add special highlight if we are on today (=start with time) */
        if (stime[2] != ':') {
            g_object_set(rend
                     , "foreground-set",    FALSE
                     , "strikethrough-set", FALSE
                     , "weight-set",        FALSE
                     , NULL);
        }
        else if (strncmp(etime, el->time_now, 5) < 0) { /* gone */
            g_object_set(rend
                     , "foreground-set",    FALSE
                     , "strikethrough",     TRUE
                     , "strikethrough-set", TRUE
                     , "weight",            PANGO_WEIGHT_LIGHT
                     , "weight-set",        TRUE
                     , NULL);
        }
        else if (strncmp(stime, el->time_now, 5) <= 0
              && strncmp(etime, el->time_now, 5) >= 0) { /* current */
            g_object_set(rend
                     , "foreground",        "Blue"
                     , "foreground-set",    TRUE
                     , "strikethrough-set", FALSE
                     , "weight",            PANGO_WEIGHT_BOLD
                     , "weight-set",        TRUE
                     , NULL);
        }
        else { /* future */
            g_object_set(rend
                     , "foreground-set",    FALSE
                     , "strikethrough-set", FALSE
                     , "weight",            PANGO_WEIGHT_BOLD
                     , "weight-set",        TRUE
                     , NULL);
        }
        g_free(stime2);
    }
    else if (el->page == TODO_PAGE) {
        gtk_tree_model_get(model, iter, COL_SORT, &stime, -1);
        if (stime[8] == 'T') { /* date+time */
            len = 15;
        }
        else { /* date only */
            len = 8;
        }
        strncpy(start_time, stime, len);
        start_time[len] = '\0';
        gdt_start_time = orage_icaltime_to_gdatetime (start_time);
        gtk_tree_model_get(model, iter, COL_TIME, &stime2, -1);
        if (g_str_has_suffix(stime2, "- ...")) /* no due time */
        {
            /* long in the future*/
            utc_tz = g_time_zone_new_utc ();
            gdt_end_time = g_date_time_new (utc_tz, 9999, 12, 31, 23, 59, 59);
            g_time_zone_unref (utc_tz);
        }
        else /* normal due time*/
        {
            strncpy(end_time, stime+len, len);
            gdt_end_time = orage_icaltime_to_gdatetime (end_time);
        }

        g_free (stime);
        g_free (stime2);

        if (g_date_time_difference (gdt_end_time, el->date_now) < 0) {
            /* gone */
            g_object_set(rend
                     , "foreground",        "Red"
                     , "foreground-set",    TRUE
                     , "strikethrough-set", FALSE
                     , "weight",            PANGO_WEIGHT_BOLD
                     , "weight-set",        TRUE
                     , NULL);
        }
        else if (g_date_time_difference (gdt_start_time, el->date_now) <= 0
              && g_date_time_difference (gdt_end_time, el->date_now) >= 0) {
            /* current */
            g_object_set(rend
                     , "foreground",        "Blue"
                     , "foreground-set",    TRUE
                     , "strikethrough-set", FALSE
                     , "weight",            PANGO_WEIGHT_BOLD
                     , "weight-set",        TRUE
                     , NULL);
        }
        else { /* future */
            g_object_set(rend
                     , "foreground-set",    FALSE
                     , "strikethrough-set", FALSE
                     , "weight",            PANGO_WEIGHT_LIGHT
                     , "weight-set",        TRUE
                     , NULL);
        }
        g_date_time_unref (gdt_start_time);
        g_date_time_unref (gdt_end_time);
    }
    else {
        g_object_set(rend
                 , "foreground-set",    FALSE
                 , "strikethrough-set", FALSE
                 , "weight-set",        FALSE
                 , NULL);
    }
}

static void add_el_row(el_win *el, xfical_appt *appt, GDateTime *gdt_par)
{
    GtkTreeIter     iter1;
    GtkListStore   *list1;
    gchar          *title = NULL, *tmp;
    gchar           flags[6]; 
    gchar          *stime;
    gchar          /* *s_sort,*/ *s_sort1;
    gchar          *tmp_note;
    guint           len = 50;
    gchar *s_time;
    gchar *e_time;

    stime = format_time (el, appt, gdt_par);
    if (appt->display_alarm_orage || appt->display_alarm_notify 
    ||  appt->sound_alarm || appt->procedure_alarm)
        if (appt->alarm_persistent)
            flags[0] = 'P';
        else
            flags[0] = 'A';
    else
        flags[0] = 'n';

    if (appt->freq == XFICAL_FREQ_NONE)
        flags[1] = 'n';
    else if (appt->freq == XFICAL_FREQ_DAILY)
        flags[1] = 'D';
    else if (appt->freq == XFICAL_FREQ_WEEKLY)
        flags[1] = 'W';
    else if (appt->freq == XFICAL_FREQ_MONTHLY)
        flags[1] = 'M';
    else if (appt->freq == XFICAL_FREQ_YEARLY)
        flags[1] = 'Y';
    else if (appt->freq == XFICAL_FREQ_HOURLY)
        flags[1] = 'H';
    else
        flags[1] = '?';

    if (appt->availability != 0)
        flags[2] = 'B';
    else
        flags[2] = 'f';

    flags[3] = appt->uid[0]; /* file type */

    if (appt->type == XFICAL_TYPE_EVENT)
        flags[4] = 'E';
    else if (appt->type == XFICAL_TYPE_TODO)
        flags[4] = 'T';
    else /* Journal */
        flags[4] = 'J';

    flags[5] = '\0';

    if (appt->title != NULL)
        title = orage_process_text_commands(appt->title);
    else if (appt->note != NULL) { 
    /* let's take len chars of the first line from the text */
        tmp_note = orage_process_text_commands(appt->note);
        if ((tmp = g_strstr_len(tmp_note, strlen(tmp_note), "\n")) != NULL) {
            /* there is line change. take text only up to that */
            if ((strlen(tmp_note)-strlen(tmp)) < len)
                len = strlen(tmp_note)-strlen(tmp);
        }
        title = g_strndup(tmp_note, len);
        g_free(tmp_note);
    }

    s_time = orage_gdatetime_to_icaltime (appt->starttimecur, appt->allDay);
    e_time = orage_gdatetime_to_icaltime (appt->endtimecur, appt->allDay);
    s_sort1 = g_strconcat (s_time, e_time, NULL);
    g_free (s_time);
    g_free (e_time);

#if 0
    s_sort = g_utf8_collate_key(s_sort1, -1);
#endif

    list1 = el->ListStore;
    gtk_list_store_append(list1, &iter1);
    gtk_list_store_set(list1, &iter1
            , COL_TIME,  stime
            , COL_FLAGS, flags
            , COL_HEAD,  title
            , COL_UID,   appt->uid
            , COL_SORT,  s_sort1
            , CAL_CATEGORIES,   appt->categories
            , -1);
    g_free(title);
    g_free(s_sort1);
    g_free(stime);
#if 0
    g_free(s_sort);
#endif
}

static void searh_rows(el_win *el, gchar *search_string, gchar *file_type)
{
    xfical_appt *appt;

    for (appt = xfical_appt_get_next_with_string(search_string, TRUE
                , file_type);
         appt;
         appt = xfical_appt_get_next_with_string(search_string, FALSE
                 , file_type)) {
        add_el_row(el, appt, NULL);
        xfical_appt_free(appt);
    }
}

static void search_data(el_win *el)
{
    gchar *search_string = NULL, file_type[8];
    gint i;

    search_string = g_utf8_strup(gtk_entry_get_text(
                (GtkEntry *)el->search_entry), -1);
    /* first search base orage file */
    if (!xfical_file_open(TRUE))
        return;
    g_strlcpy (file_type, "O00.", sizeof (file_type));
    searh_rows(el, search_string, file_type);
    /* then process all foreign files */
    for (i = 0; i < g_par.foreign_count; i++) {
        g_snprintf(file_type, sizeof (file_type), "F%02d.", i);
        searh_rows(el, search_string, file_type);
    }

#ifdef HAVE_ARCHIVE
    /* finally process always archive file also */
    if (xfical_archive_open()) {
        g_strlcpy (file_type, "A00.", sizeof (file_type));
        searh_rows(el, search_string, file_type);
        xfical_archive_close();
    }
#endif
    xfical_file_close(TRUE);
    g_free(search_string);
}

static void app_rows (el_win *el, GDateTime *a_day_gdt, GDateTime *gdt_par,
                      xfical_type ical_type, gchar *file_type)
{
    GList *appt_list=NULL, *tmp;
    xfical_appt *appt;

    if (ical_type == XFICAL_TYPE_EVENT && !el->only_first) {
        xfical_get_each_app_within_time (a_day_gdt, el->days+1
                , ical_type, file_type, &appt_list);
        for (tmp = g_list_first(appt_list);
             tmp != NULL;
             tmp = g_list_next(tmp)) {
            appt = (xfical_appt *)tmp->data;
            if (appt->priority < g_par.priority_list_limit) {
                add_el_row (el, appt, gdt_par);
            }
            xfical_appt_free(appt);
        }
        g_list_free(appt_list);
    }
    else {
        for (appt = xfical_appt_get_next_on_day (a_day_gdt, TRUE, el->days
                    , ical_type, file_type);
             appt;
             appt = xfical_appt_get_next_on_day (a_day_gdt, FALSE, el->days
                    , ical_type, file_type)) {
            add_el_row (el, appt, gdt_par);
            xfical_appt_free(appt);
        }
    }
}

static void app_data (el_win *el, GDateTime *a_day_gdt, GDateTime *gdt_par)
{
    xfical_type ical_type;
    gchar file_type[8];
    gint i;

    switch (el->page) {
        case EVENT_PAGE:
            ical_type = XFICAL_TYPE_EVENT;
            break;
        case TODO_PAGE:
            ical_type = XFICAL_TYPE_TODO;
            break;
        case JOURNAL_PAGE:
            ical_type = XFICAL_TYPE_JOURNAL;
            break;
        default:
            g_error ("wrong page in app_data (%d)", el->page);
    }

    /* first search base orage file */
    if (!xfical_file_open(TRUE))
        return;
    g_strlcpy (file_type, "O00.", sizeof (file_type));
    app_rows (el, a_day_gdt, gdt_par, ical_type, file_type);
    /* then process all foreign files */
    for (i = 0; i < g_par.foreign_count; i++) {
        g_snprintf(file_type, sizeof (file_type), "F%02d.", i);
        app_rows (el, a_day_gdt, gdt_par, ical_type, file_type);
    }

#ifdef HAVE_ARCHIVE
    /* finally process archive file for JOURNAL only */
    if (ical_type == XFICAL_TYPE_JOURNAL) {
        if (xfical_archive_open()) {
            g_strlcpy (file_type, "A00.", sizeof (file_type));
            app_rows (el, a_day_gdt, gdt_par, ical_type, file_type);
            xfical_archive_close();
        }
    }
#endif
    xfical_file_close(TRUE);
}

static void refresh_time_field(el_win *el)
{
    GtkCellRenderer *rend;
    GtkTreeViewColumn *col;

/* this is needed if we want to make time field smaller again */
    col = gtk_tree_view_get_column(GTK_TREE_VIEW(el->TreeView), 0);
    gtk_tree_view_remove_column(GTK_TREE_VIEW(el->TreeView), col);
    rend = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( _("Time"), rend
            , "text", COL_TIME, NULL);
    gtk_tree_view_column_set_cell_data_func(col, rend
            , start_time_data_func, el, NULL);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(el->TreeView), col, 0);
}

static void event_data(el_win *el)
{
    GDateTime *gdt_now;
    GDateTime *gdt_a_day;
    GDateTime *gdt_title;

    if (el->days == 0)
        refresh_time_field(el);
    el->days = gtk_spin_button_get_value(GTK_SPIN_BUTTON(el->event_spin));
#if 0
    el->only_first = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(el->event_only_first_checkbutton));
#endif
    el->show_old = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(el->event_show_old_checkbutton));
    gdt_title = g_object_get_data (G_OBJECT (el->Window), DATE_KEY);

    if (el->show_old && el->only_first) {
        /* just take any old enough date, so that all events fit in */
        gdt_a_day = g_date_time_new_local (1900, 1, 1, 0, 0, 0);
        el->days += orage_gdatetime_days_between (gdt_a_day, gdt_title);
    }
    else { /* we show starting from selected day */
        gdt_a_day = g_date_time_ref (gdt_title);
    }

    gdt_now = g_date_time_new_now_local ();
    g_snprintf (el->time_now, sizeof (el->time_now), "%02d:%02d",
                g_date_time_get_hour (gdt_now),
                g_date_time_get_minute (gdt_now));

    el->today = (orage_gdatetime_days_between (gdt_title, gdt_now) == 0);

    app_data (el, gdt_a_day, gdt_a_day);
    g_date_time_unref (gdt_a_day);
    g_date_time_unref (gdt_now);
}

static void todo_data(el_win *el)
{
    el->days = 0; /* not used */

    orage_gdatetime_unref (el->date_now);
    el->date_now = g_date_time_new_now_local ();
    app_data (el, el->date_now, NULL);
}

static void journal_data(el_win *el)
{
    GDateTime *gdt;

    el->days = 10*365; /* long enough time to get everything from future */
    gdt = g_object_get_data (G_OBJECT (el->journal_start_button), DATE_KEY);
    app_data (el, gdt, NULL);
}

void refresh_el_win(el_win *el)
{
    (void)orage_category_get_list (); /* Uses side effects. */
    if (el->Window && el->ListStore && el->TreeView) {
        gtk_list_store_clear(el->ListStore);
        el->page = gtk_notebook_get_current_page(GTK_NOTEBOOK(el->Notebook));
        switch (el->page) {
            case EVENT_PAGE:
                event_data(el);
                break;
            case TODO_PAGE:
                todo_data(el);
                break;
            case JOURNAL_PAGE:
                journal_data(el);
                break;
            case SEARCH_PAGE:
                search_data(el);
                break;
            default:
                g_warning ("%s: unknown tab", G_STRFUNC);
                break;
        }
    }
}

static gboolean upd_notebook(el_win *el)
{
    static gint prev_page = -1;
    gint cur_page;

    /* we only show the page IF it is different than the last
     * page changed with this method
     */
    cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(el->Notebook));
    if (cur_page != prev_page) {
        prev_page = cur_page;
        refresh_el_win(el);
    }
    return(FALSE); /* we do this only once */
}

static void on_notebook_page_switch (G_GNUC_UNUSED GtkNotebook *notebook,
                                     G_GNUC_UNUSED GtkWidget *page,
                                     G_GNUC_UNUSED guint page_num,
                                     gpointer user_data)
{
    el_win *el = (el_win *)user_data;

    /* we can't do refresh_el_win here directly since it is slow and
     * gtk_notebook_get_current_page points to old page at this time,
     * so we end up showing wrong page.
     * This timeout also makes it possible to avoid unnecessary 
     * refreshes when tab is change quickly; like with mouse roll
     */
    g_timeout_add(300, (GSourceFunc)upd_notebook, el);
}

static void on_Search_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    el_win *el = (el_win *)user_data;

    gtk_notebook_set_current_page(GTK_NOTEBOOK(el->Notebook), SEARCH_PAGE);
    refresh_el_win((el_win *)user_data);
}

static void on_View_search_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                        gpointer user_data)
{
    el_win *el = (el_win *)user_data;

    gtk_notebook_set_current_page(GTK_NOTEBOOK(el->Notebook), SEARCH_PAGE);
    refresh_el_win((el_win *)user_data);
}

static void set_el_data (el_win *el, GDateTime *gdt)
{
    gchar *title;

    title = orage_gdatetime_to_i18_time (gdt, TRUE);
    gtk_window_set_title(GTK_WINDOW(el->Window), title);
    g_free (title);

    g_date_time_ref (gdt);
    g_object_set_data_full (G_OBJECT (el->Window),
                            DATE_KEY, gdt, (GDestroyNotify)g_date_time_unref);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(el->Notebook), EVENT_PAGE);
    refresh_el_win(el);
}

static void set_el_data_from_cal(el_win *el)
{
    GDateTime *gdt;
    OrageApplication *app;
    OrageWindow *window;

    app = ORAGE_APPLICATION (g_application_get_default ());
    window = ORAGE_WINDOW (orage_application_get_window (app));
    gdt = orage_window_get_selected_date (window);
    set_el_data (el, gdt);
    g_date_time_unref (gdt);
}

static void duplicate_appointment(el_win *el)
{
    GtkTreeSelection *sel;
    GtkTreeModel     *model;
    GtkTreePath      *path;
    GtkTreeIter       iter;
    GList *list;
    gint  list_len;

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(el->TreeView));
    list = gtk_tree_selection_get_selected_rows(sel, &model);
    list_len = g_list_length(list);
    if (list_len > 0) {
        if (list_len > 1)
            g_warning ("Copy: too many rows selected");
        path = (GtkTreePath *)g_list_nth_data(list, 0);
        start_appt_win (TRUE, el, model, &iter, path);
    }
    else {
        orage_info_dialog(GTK_WINDOW(el->Window)
                , _("No rows have been selected.")
                , _("Click a row to select it and after that you can copy it.")
                );
    }

    g_list_foreach(list, (GFunc)(GCallback)gtk_tree_path_free, NULL);
    g_list_free(list);
}

static void on_Copy_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    duplicate_appointment((el_win *)user_data);
}

static void on_File_duplicate_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                           gpointer user_data)
{
    duplicate_appointment((el_win *)user_data);
}

static void close_window(el_win *el)
{
    OrageAppointmentWindow *apptw;
    GList *apptw_list;

    gtk_window_get_size(GTK_WINDOW(el->Window)
            , &g_par.el_size_x, &g_par.el_size_y);
    gtk_window_get_position(GTK_WINDOW(el->Window)
            , &g_par.el_pos_x, &g_par.el_pos_y);
    write_parameters();

    /* need to clean the appointment list and inform all appointments that
     * we are not interested anymore (= should not get updated) */
    apptw_list = el->apptw_list;
    for (apptw_list = g_list_first(apptw_list);
         apptw_list != NULL;
         apptw_list = g_list_next(apptw_list)) {
        apptw = ORAGE_APPOINTMENT_WINDOW (apptw_list->data);
        if (apptw) /* appointment window is still alive */
        {
            /* not interested anymore */
            orage_appointment_window_set_event_list  (apptw, NULL);
        }
        else
            g_warning ("%s: not null appt window", G_STRFUNC);
    }
    g_list_free(el->apptw_list);

    gtk_widget_destroy(el->Window); /* destroy the eventlist window */
    g_free(el);
    el = NULL;
}

static void on_Close_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    close_window((el_win *)user_data);
}

static void on_Dayview_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    el_win *el = (el_win *)user_data;
    GDateTime *gdt;

    gdt = g_object_get_data (G_OBJECT (el->Window), DATE_KEY);
    orage_week_window_build (gdt);
}

static void on_File_close_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                       gpointer user_data)
{
    close_window((el_win *)user_data);
}

static gboolean on_Window_delete_event (G_GNUC_UNUSED GtkWidget *w,
                                        G_GNUC_UNUSED GdkEvent *e,
                                        gpointer user_data)
{
    close_window((el_win *)user_data);

    return(FALSE);
}

static void on_Refresh_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    refresh_el_win((el_win*)user_data);
}

static void on_View_refresh_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                         gpointer user_data)
{
    refresh_el_win((el_win*)user_data);
}

static void changeSelectedDate (el_win *el, const gint day)
{
    GDateTime *gdt1;
    GDateTime *gdt2;
    OrageWindow *window;
    OrageApplication *app;

    gdt1 = g_object_get_data (G_OBJECT (el->Window), DATE_KEY);
    gdt2 = g_date_time_add_days (gdt1, day);
    app = ORAGE_APPLICATION (g_application_get_default ());
    window = ORAGE_WINDOW (orage_application_get_window (app));
    orage_window_select_date (window, gdt2);
    g_date_time_unref (gdt2);

    set_el_data_from_cal(el);
}

static void on_Previous_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    changeSelectedDate((el_win *)user_data, -1);
}

static void on_Go_previous_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                        gpointer user_data)
{
    changeSelectedDate((el_win *)user_data, -1);
}

static void go_to_today(el_win *el)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    OrageWindow *window = ORAGE_WINDOW (orage_application_get_window (app));
    orage_window_select_today (window);
    set_el_data_from_cal(el);
}

static void on_Today_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    go_to_today((el_win *)user_data);
}

static void on_Go_today_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                     gpointer user_data)
{
    go_to_today((el_win *)user_data);
}

static void on_Next_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    changeSelectedDate((el_win *)user_data, 1);
}

static void on_Go_next_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                    gpointer user_data)
{
    changeSelectedDate((el_win *)user_data, 1);
}

static void create_new_appointment(el_win *el)
{
    GDateTime *gdt;
    GtkWidget *appointment_window;

    gdt = g_object_get_data (G_OBJECT (el->Window), DATE_KEY);
    appointment_window = orage_appointment_window_new (gdt);
    /* inform the appointment that we are interested in it */
    orage_appointment_window_set_event_list (
            ORAGE_APPOINTMENT_WINDOW (appointment_window), el);
    gtk_window_present (GTK_WINDOW (appointment_window));
    /* we started this, so keep track of it */
    el->apptw_list = g_list_prepend (el->apptw_list, appointment_window);
}

static void on_File_newApp_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                        gpointer user_data)
{
    create_new_appointment((el_win *)user_data);
}

static void on_Create_toolbutton_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                             gpointer user_data)
{
    create_new_appointment((el_win *)user_data);
}

static void on_spin_changed (G_GNUC_UNUSED GtkSpinButton *b, gpointer user_data)
{
    refresh_el_win((el_win *)user_data);
}

static void on_only_first_clicked (G_GNUC_UNUSED GtkCheckButton *b,
                                   gpointer user_data)
{
    el_win *el = (el_win *)user_data;

    el->only_first = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(el->event_only_first_checkbutton));
    gtk_widget_set_sensitive(el->event_show_old_checkbutton, el->only_first);
    refresh_el_win((el_win *)user_data);
}

static void on_show_old_clicked (G_GNUC_UNUSED GtkCheckButton *b,
                                 gpointer user_data)
{
    refresh_el_win((el_win *)user_data);
}

static void delete_appointment(el_win *el)
{
    OrageApplication *app;
    gint result;
    GtkTreeSelection *sel;
    GtkTreeModel     *model;
    GtkTreePath      *path;
    GtkTreeIter       iter;
    GList *list;
    gint  list_len, i;
    gchar *uid = NULL, *flags = NULL;

    result = orage_warning_dialog(GTK_WINDOW(el->Window)
            , _("You will permanently remove all\nselected appointments.")
            , _("Do you want to continue?")
            , _("No, cancel the removal")
            , _("Yes, remove them"));

    if (result == GTK_RESPONSE_YES) {
        if (!xfical_file_open(TRUE))
            return;
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(el->TreeView));
        list = gtk_tree_selection_get_selected_rows(sel, &model);
        list_len = g_list_length(list);
        for (i = 0; i < list_len; i++) {
            path = (GtkTreePath *)g_list_nth_data(list, i);
            if (gtk_tree_model_get_iter(model, &iter, path)) {
                gtk_tree_model_get(model, &iter, COL_UID, &uid, -1);
#ifdef HAVE_ARCHIVE
                gtk_tree_model_get(model, &iter, COL_FLAGS, &flags, -1);
                if (flags && flags[3] == 'A') {
                    xfical_unarchive_uid(uid);
                    /* note that file id changes after archive */ 
                    uid[0]='O';
                    /* xfical_unarchive_uid closes the file */
                    if (!xfical_file_open(TRUE))
                        return;
                }
                g_free(flags);
#endif
                result = xfical_appt_del(uid);
                if (result)
                    g_message ("Removed: %s", uid);
                else
                    g_warning ("Removal failed: %s", uid);
                g_free(uid);
            }
        }
        xfical_file_close(TRUE);
        refresh_el_win(el);
        app = ORAGE_APPLICATION (g_application_get_default ());
        orage_window_mark_appointments (ORAGE_WINDOW (
            orage_application_get_window (app)));
        g_list_foreach(list, (GFunc)(GCallback)gtk_tree_path_free, NULL);
        g_list_free(list);
    }
}

static void on_Delete_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    delete_appointment((el_win *)user_data);
}

static void on_File_delete_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                        gpointer user_data)
{
    delete_appointment((el_win *)user_data);
}

static void on_journal_start_button_clicked(GtkWidget *button
        , gpointer *user_data)
{
    el_win *el = (el_win *)user_data;
    GtkWidget *selDate_dialog;

    selDate_dialog = gtk_dialog_new_with_buttons(
            _("Pick the date"), GTK_WINDOW(el->Window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("Today"), 1, "_OK", GTK_RESPONSE_ACCEPT, NULL);

    if (orage_date_button_clicked (button, selDate_dialog))
        refresh_el_win(el);
}

static void drag_data_get (GtkWidget *widget,
                           G_GNUC_UNUSED GdkDragContext *context,
                           GtkSelectionData *selection_data,
                           G_GNUC_UNUSED guint info,
                           G_GNUC_UNUSED guint itime,
                           G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeSelection *sel;
    GtkTreeModel     *model;
    GtkTreePath      *path;
    GtkTreeIter       iter;
    GList *list;
    gint  list_len, i;
    gchar *uid = NULL;
    GString *result = NULL;

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    list = gtk_tree_selection_get_selected_rows(sel, &model);
    list_len = g_list_length(list);
    for (i = 0; i < list_len; i++) {
        path = (GtkTreePath *)g_list_nth_data(list, i);
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            gtk_tree_model_get(model, &iter, COL_UID, &uid, -1);
            if (i == 0) { /* first */
                result = g_string_new(uid);
            }
            else {
                g_string_append(result, ",");
                g_string_append(result, uid);
            }
            g_free(uid);
        }
    }
    g_list_foreach(list, (GFunc)(GCallback)gtk_tree_path_free, NULL);
    g_list_free(list);
    if (result) {
        if (!gtk_selection_data_set_text(selection_data, result->str, -1))
            g_warning ("%s: failed", G_STRFUNC);
        g_string_free(result, TRUE);
    }
}

static void build_menu(el_win *el)
{
    el->Menubar = gtk_menu_bar_new();
    gtk_grid_attach_next_to (GTK_GRID(el->Vbox), el->Menubar, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    /********** File menu **********/
    el->File_menu = orage_menu_new(_("_File"), el->Menubar);
    el->File_menu_new = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-new", el->File_menu, el->accel_group);

    (void)orage_separator_menu_item_new(el->File_menu);

    el->File_menu_duplicate = orage_menu_item_new_with_mnemonic(_("D_uplicate")
            , el->File_menu);
    gtk_widget_add_accelerator(el->File_menu_duplicate
            , "activate", el->accel_group
            , GDK_KEY_d, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    (void)orage_separator_menu_item_new(el->File_menu);

    el->File_menu_delete = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-delete", el->File_menu, el->accel_group);

    (void)orage_separator_menu_item_new(el->File_menu);

    el->File_menu_close = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-close", el->File_menu, el->accel_group);

    /********** View menu **********/
    el->View_menu = orage_menu_new(_("_View"), el->Menubar);
    el->View_menu_refresh = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-refresh", el->View_menu, el->accel_group);
    gtk_widget_add_accelerator(el->View_menu_refresh
            , "activate", el->accel_group
            , GDK_KEY_r, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(el->View_menu_refresh
            , "activate", el->accel_group
            , GDK_KEY_Return, 0, 0);
    gtk_widget_add_accelerator(el->View_menu_refresh
            , "activate", el->accel_group
            , GDK_KEY_KP_Enter, 0, 0);

    (void)orage_separator_menu_item_new(el->View_menu);

    el->View_menu_search = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-find", el->View_menu, el->accel_group);

    /********** Go menu   **********/
    el->Go_menu = orage_menu_new(_("_Go"), el->Menubar);
    el->Go_menu_today = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-home", el->Go_menu, el->accel_group);
    gtk_widget_add_accelerator(el->Go_menu_today
            , "activate", el->accel_group
            , GDK_KEY_Home, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    el->Go_menu_prev = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-go-back", el->Go_menu, el->accel_group);
    gtk_widget_add_accelerator(el->Go_menu_prev
            , "activate", el->accel_group
            , GDK_KEY_Left, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    el->Go_menu_next = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-go-forward", el->Go_menu, el->accel_group);
    gtk_widget_add_accelerator(el->Go_menu_next
            , "activate", el->accel_group
            , GDK_KEY_Right, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

    g_signal_connect((gpointer)el->File_menu_new, "activate"
            , G_CALLBACK(on_File_newApp_activate_cb), el);
    g_signal_connect((gpointer)el->File_menu_duplicate, "activate"
            , G_CALLBACK(on_File_duplicate_activate_cb), el);
    g_signal_connect((gpointer)el->File_menu_delete, "activate"
            , G_CALLBACK(on_File_delete_activate_cb), el);
    g_signal_connect((gpointer)el->View_menu_refresh, "activate"
            , G_CALLBACK(on_View_refresh_activate_cb), el);
    g_signal_connect((gpointer)el->View_menu_search, "activate"
            , G_CALLBACK(on_View_search_activate_cb), el);
    g_signal_connect((gpointer)el->Go_menu_today, "activate"
            , G_CALLBACK(on_Go_today_activate_cb), el);
    g_signal_connect((gpointer)el->Go_menu_prev, "activate"
            , G_CALLBACK(on_Go_previous_activate_cb), el);
    g_signal_connect((gpointer)el->Go_menu_next, "activate"
            , G_CALLBACK(on_Go_next_activate_cb), el);
    g_signal_connect((gpointer)el->File_menu_close, "activate"
            , G_CALLBACK(on_File_close_activate_cb), el);
}

static void build_toolbar(el_win *el)
{
    gint i = 0;

    el->Toolbar = gtk_toolbar_new();
    gtk_grid_attach_next_to (GTK_GRID(el->Vbox), el->Toolbar, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    el->Create_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "document-new", _("New"), i++);
    el->Copy_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "edit-copy", _("Duplicate"), i++);
    el->Delete_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "edit-delete", _("Delete"), i++);

    (void)orage_toolbar_append_separator(el->Toolbar, i++);

    el->Previous_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "go-previous", _("Back"), i++);
    el->Today_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "go-home", _("Today"), i++);
    el->Next_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "go-next", _("Forward"), i++);

    (void)orage_toolbar_append_separator(el->Toolbar, i++);

    el->Refresh_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "view-refresh", _("Refresh"), i++);
    el->Search_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "edit-find", _("Find"), i++);

    (void)orage_toolbar_append_separator(el->Toolbar, i++);

    el->Close_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "window-close", _("Close"), i++);
    el->Dayview_toolbutton = orage_toolbar_append_button(el->Toolbar
            , "zoom-in", _("Dayview"), i++);

    g_signal_connect((gpointer)el->Create_toolbutton, "clicked"
            , G_CALLBACK(on_Create_toolbutton_clicked_cb), el);
    g_signal_connect((gpointer)el->Copy_toolbutton, "clicked"
            , G_CALLBACK(on_Copy_clicked), el);
    g_signal_connect((gpointer)el->Delete_toolbutton, "clicked"
            , G_CALLBACK(on_Delete_clicked), el);
    g_signal_connect((gpointer)el->Previous_toolbutton, "clicked"
            , G_CALLBACK(on_Previous_clicked), el);
    g_signal_connect((gpointer)el->Today_toolbutton, "clicked"
            , G_CALLBACK(on_Today_clicked), el);
    g_signal_connect((gpointer)el->Next_toolbutton, "clicked"
            , G_CALLBACK(on_Next_clicked), el);
    g_signal_connect((gpointer)el->Refresh_toolbutton, "clicked"
            , G_CALLBACK(on_Refresh_clicked), el);
    g_signal_connect((gpointer)el->Search_toolbutton, "clicked"
            , G_CALLBACK(on_Search_clicked), el);
    g_signal_connect((gpointer)el->Close_toolbutton, "clicked"
            , G_CALLBACK(on_Close_clicked), el);
    g_signal_connect((gpointer)el->Dayview_toolbutton, "clicked"
            , G_CALLBACK(on_Dayview_clicked), el);
}

static void build_event_tab(el_win *el)
{
    GtkWidget *label;
    GtkWidget *hbox;

    hbox = gtk_grid_new ();
    el->event_tab_label = gtk_label_new(_("Event"));
    /* we do not really need table, but it is efficient and easy way to
       do this. Using hboxes takes actually more memory */
    el->event_notebook_page = orage_table_new (BORDER_SIZE);

    label = gtk_label_new(_("Extra days to show:"));

    el->event_spin = gtk_spin_button_new_with_range(0, 99999, 1);
    g_object_set (el->event_spin, "margin-right", 2,
                                  "margin-left", 2,
                                   NULL);

    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(el->event_spin), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(el->event_spin)
            , (gdouble)el->days);
    gtk_grid_attach_next_to (GTK_GRID (hbox), el->event_spin, NULL,
                             GTK_POS_RIGHT, 1, 1);

    el->event_only_first_checkbutton =
            gtk_check_button_new_with_label(_("only first repeating"));
    g_object_set (el->event_only_first_checkbutton, "margin-right", 15,
                                                    "margin-left", 15,
                                                    NULL);
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(el->event_only_first_checkbutton)
                    , el->only_first);
    gtk_widget_set_tooltip_text(el->event_only_first_checkbutton
            , _("Check this if you only want to see the first repeating event. By default all are shown.\nNote that this also shows all urgencies.\nNote that repeating events may appear earlier in the list as the first occurrence only is listed."));
    gtk_grid_attach_next_to (GTK_GRID (hbox), el->event_only_first_checkbutton,
                             NULL, GTK_POS_RIGHT, 1, 1);

    el->event_show_old_checkbutton =
            gtk_check_button_new_with_label(_("also old"));
    g_object_set (el->event_show_old_checkbutton, "margin-right", 15,
                                                  "margin-left", 15,
                                                  NULL);
    gtk_widget_set_tooltip_text(el->event_show_old_checkbutton
            , _("Check this if you want to see old events also. This can only be selected after 'only first repeating' is enabled to avoid very long lists.\nNote that extra days selection still defines if newer appointments are listed."));
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(el->event_show_old_checkbutton), el->only_first);
    gtk_widget_set_sensitive(el->event_show_old_checkbutton, el->only_first);
    gtk_grid_attach_next_to (GTK_GRID (hbox), el->event_show_old_checkbutton,
                             NULL, GTK_POS_RIGHT, 1, 1);

    orage_table_add_row (el->event_notebook_page, label, hbox, 0, OTBL_FILL, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(el->Notebook)
            , el->event_notebook_page, el->event_tab_label);
    g_signal_connect((gpointer)el->event_spin, "value-changed"
            , G_CALLBACK(on_spin_changed), el);
    g_signal_connect((gpointer)el->event_only_first_checkbutton, "clicked"
            , G_CALLBACK(on_only_first_clicked), el);
    g_signal_connect((gpointer)el->event_show_old_checkbutton, "clicked"
            , G_CALLBACK(on_show_old_clicked), el);
}

static void build_todo_tab(el_win *el)
{
    el->todo_tab_label = gtk_label_new(_("Todo"));
    el->todo_notebook_page = gtk_grid_new ();

    gtk_notebook_append_page(GTK_NOTEBOOK(el->Notebook)
            , el->todo_notebook_page, el->todo_tab_label);
}

static void build_journal_tab(el_win *el)
{
    GtkWidget *label;
    GDateTime *gdt_local;
    GDateTime *gdt;
    gchar *sdate;

    el->journal_tab_label = gtk_label_new(_("Journal"));
    el->journal_notebook_page = orage_table_new (BORDER_SIZE);

    label = gtk_label_new(_("Journal entries starting from:"));
    el->journal_start_button = gtk_button_new();
    gdt_local = g_date_time_new_now_local ();
    gdt = g_date_time_add_years (gdt_local, -1);
    g_date_time_unref (gdt_local);

    sdate = orage_gdatetime_to_i18_time (gdt, TRUE);
    gtk_button_set_label (GTK_BUTTON (el->journal_start_button), sdate);
    g_object_set_data_full (G_OBJECT (el->journal_start_button),
                            DATE_KEY, gdt, (GDestroyNotify)g_date_time_unref);

    g_free (sdate);
    orage_table_add_row (el->journal_notebook_page, label,
                         el->journal_start_button, 0, OTBL_FILL, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(el->Notebook)
            , el->journal_notebook_page, el->journal_tab_label);
    g_signal_connect((gpointer)el->journal_start_button, "clicked"
            , G_CALLBACK(on_journal_start_button_clicked), el);
}

static void build_search_tab(el_win *el)
{
    GtkWidget *label;

    el->search_tab_label = gtk_label_new(_("Search"));
    el->search_notebook_page = orage_table_new (BORDER_SIZE);

    label = gtk_label_new(_("Search text "));
    el->search_entry = gtk_entry_new();
    orage_table_add_row (el->search_notebook_page, label, el->search_entry, 0,
                         OTBL_EXPAND | OTBL_FILL, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(el->Notebook)
            , el->search_notebook_page, el->search_tab_label);
}

static void build_notebook(el_win *el)
{
    el->Notebook = gtk_notebook_new();
    gtk_grid_attach_next_to (GTK_GRID(el->Vbox), el->Notebook, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    build_event_tab(el);
    build_todo_tab(el);
    build_journal_tab(el);
    build_search_tab(el);

    g_signal_connect((gpointer)el->Notebook, "switch-page"
            , G_CALLBACK(on_notebook_page_switch), el);
}

static void build_event_list(el_win *el)
{
    GtkCellRenderer *rend;
    GtkTreeViewColumn *col;

    /* Scrolled window */
    el->ScrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    g_object_set (el->ScrolledWindow, "vexpand", TRUE, NULL);
    gtk_grid_attach_next_to (GTK_GRID(el->Vbox), el->ScrolledWindow, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(el->ScrolledWindow)
            , GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* Tree view */
    el->ListStore = gtk_list_store_new(NUM_COLS
            , G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING
            , G_TYPE_STRING, G_TYPE_STRING);
    el->TreeView = gtk_tree_view_new();

    el->TreeSelection = 
            gtk_tree_view_get_selection(GTK_TREE_VIEW(el->TreeView)); 
    gtk_tree_selection_set_mode(el->TreeSelection, GTK_SELECTION_MULTIPLE);

    el->TreeSortable = GTK_TREE_SORTABLE(el->ListStore);
    gtk_tree_sortable_set_sort_func(el->TreeSortable, COL_SORT
            , sortEvent_comp, GINT_TO_POINTER(COL_SORT), NULL);
    gtk_tree_sortable_set_sort_column_id(el->TreeSortable
            , COL_SORT, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(el->TreeView)
            , GTK_TREE_MODEL(el->ListStore));

    gtk_container_add(GTK_CONTAINER(el->ScrolledWindow), el->TreeView);

    rend = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( _("Time"), rend
                , "text", COL_TIME
                , NULL);
    gtk_tree_view_column_set_cell_data_func(col, rend, start_time_data_func
                , el, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(el->TreeView), col);

    rend = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( _("Flags"), rend
                , "text", COL_FLAGS
                , NULL);
    gtk_tree_view_column_set_cell_data_func(col, rend, flags_data_func
                , el, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(el->TreeView), col);

    rend = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes( _("Title"), rend
                , "text", COL_HEAD
                , NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(el->TreeView), col);

    rend = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("uid", rend
                , "text", COL_UID
                , NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(el->TreeView), col);
    gtk_tree_view_column_set_visible(col, FALSE);

    rend = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("sort", rend
                , "text", COL_SORT
                , NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(el->TreeView), col);
    gtk_tree_view_column_set_visible(col, FALSE);

    rend = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("cat", rend
                , "text", CAL_CATEGORIES
                , NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(el->TreeView), col);
    gtk_tree_view_column_set_visible(col, FALSE);

    gtk_widget_set_tooltip_text(el->TreeView, _("Double click line to edit it.\n\nFlags in order:\n\t 1. Alarm: n=no alarm\n\t\t A=Alarm is set P=Persistent alarm is set\n\t 2. Recurrence: n=no recurrence\n\t\t H=Hourly D=Daily W=Weekly M=Monthly Y=Yearly\n\t 3. Type: f=free B=Busy\n\t 4. Located in file:\n\t\tO=Orage A=Archive F=Foreign\n\t 5. Appointment type:\n\t\tE=Event T=Todo J=Journal"));

    g_signal_connect(el->TreeView, "row-activated",
            G_CALLBACK(editEvent), el);
}

el_win *create_el_win (GDateTime *gdt)
{
    el_win *el;

    /* initialisation + main window + base vbox */
    el = g_new(el_win, 1);
    el->today = FALSE;
    el->days = g_par.el_days;
    el->only_first = g_par.el_only_first;
    el->show_old = el->only_first;
    el->time_now[0] = 0;
    el->date_now = g_date_time_new_now_local ();
    el->apptw_list = NULL;
    el->accel_group = gtk_accel_group_new();

    el->Window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (g_par.el_size_x || g_par.el_size_y)
        gtk_window_set_default_size(GTK_WINDOW(el->Window)
                , g_par.el_size_x, g_par.el_size_y);
    if (g_par.el_pos_x || g_par.el_pos_y)
        gtk_window_move(GTK_WINDOW(el->Window), g_par.el_pos_x, g_par.el_pos_y);
    gtk_window_add_accel_group(GTK_WINDOW(el->Window), el->accel_group);

    el->Vbox = gtk_grid_new ();
    gtk_container_add(GTK_CONTAINER(el->Window), el->Vbox);

    build_menu(el);
    build_toolbar(el);
    build_notebook(el);
    build_event_list(el);

    g_signal_connect((gpointer)el->Window, "delete_event"
            , G_CALLBACK(on_Window_delete_event), el);

    gtk_widget_show_all(el->Window);
    if (gdt == NULL)
        set_el_data_from_cal(el);
    else
        set_el_data (el, gdt);

    gtk_drag_source_set(el->TreeView, GDK_BUTTON1_MASK
            , drag_targets, DRAG_TARGET_COUNT, GDK_ACTION_COPY);
    gtk_drag_source_set_icon_name (el->TreeView, ORAGE_APP_ID);
    g_signal_connect(el->TreeView, "drag_data_get"
            , G_CALLBACK(drag_data_get), NULL);

    return(el);
}
