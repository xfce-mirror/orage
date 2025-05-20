/*
 * Copyright (c) 2021-2025 Erkki Moorits
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "event-list.h"
#include "functions.h"
#include "ical-code.h"
#include "interface.h"
#include "orage-about.h"
#include "orage-appointment-window.h"
#include "orage-css.h"
#include "orage-i18n.h"
#include "orage-week-window.h"
#include "orage-window.h"
#include "orage-window-classic.h"
#include "parameters.h"

#ifdef ENABLE_SYNC
#include "orage-application.h"
#include "orage-task-runner.h"
#endif

#define FORMAT_BOLD "<b> %s </b>"

struct _OrageWindowClassic
{
    GtkApplicationWindow __parent__;

    GtkAccelGroup *mAccel_group;

    GtkWidget *mCalendar;
    
    GtkWidget *main_box;
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

    GtkWidget *mTodo_vbox;
    GtkWidget *mTodo_rows_vbox;

    GtkWidget *mEvent_vbox;
    GtkWidget *mEvent_rows_vbox;
};

static void orage_window_classic_interface_init (OrageWindowInterface *interface);

G_DEFINE_TYPE_WITH_CODE (OrageWindowClassic, orage_window_classic, GTK_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (ORAGE_WINDOW_TYPE, orage_window_classic_interface_init))

static void orage_window_classic_restore_geometry (OrageWindowClassic *window);

static guint month_change_timer=0;

void orage_window_classic_mark_appointments (OrageWindow *window)
{
    if (window == NULL)
        return;

    if (!xfical_file_open(TRUE))
        return;

    xfical_mark_calendar (orage_window_classic_get_calendar (window));
    xfical_file_close(TRUE);
}

static void mFile_newApp_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                      gpointer user_data)
{
    GDateTime *gdt;
    GtkWidget *appointment_window;
    OrageWindow *window = ORAGE_WINDOW (user_data);

    /* cal has always a day selected here, so it is safe to read it */
    gdt = orage_cal_to_gdatetime (
        orage_window_classic_get_calendar (window), 1, 1);
    appointment_window = orage_appointment_window_new (gdt);
    gtk_window_present (GTK_WINDOW (appointment_window));
    g_date_time_unref (gdt);
}

#ifdef ENABLE_SYNC
static void mFile_refresh_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                       G_GNUC_UNUSED gpointer user_data)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());

    orage_task_runner_trigger (orage_application_get_sync (app));
}
#endif

static void mFile_interface_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                         G_GNUC_UNUSED gpointer user_data)
{
    orage_external_interface ();
}

static void mFile_close_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                     G_GNUC_UNUSED gpointer user_data)
{
    orage_application_close (ORAGE_APPLICATION (g_application_get_default ()));
}

static void mFile_quit_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                    G_GNUC_UNUSED gpointer user_data)
{
    g_application_quit (G_APPLICATION (g_application_get_default ()));
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
    GDateTime *gdt;
    OrageWindow *window = ORAGE_WINDOW (user_data);

    gdt = orage_cal_to_gdatetime (
        orage_window_classic_get_calendar (window), 1, 1);
    orage_week_window_build (gdt);
    g_date_time_unref (gdt);
}

static void mView_selectToday_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                           gpointer user_data)
{
    OrageWindow *window = ORAGE_WINDOW (user_data);

    orage_select_today (orage_window_classic_get_calendar (window));
}

static void mHelp_help_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                    G_GNUC_UNUSED gpointer user_data)
{
    orage_open_help_page ();
}

static void mHelp_about_activate_cb (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                     gpointer user_data)
{
    orage_show_about (user_data);
}

static void orage_window_classic_post_init_cb (OrageWindowClassic *window)
{
    union
    {
        gpointer ptr;
        GCallback callback;
    }
    func_ptr;
    guint rc;

    func_ptr.callback = (GCallback)orage_window_classic_post_init_cb;
    rc = g_signal_handlers_disconnect_by_func (window, func_ptr.ptr, NULL);

    g_debug ("%s: %d handlers disconnected", G_STRFUNC, rc);

    orage_window_classic_restore_geometry (window);
}

static void mCalendar_day_selected_double_click_cb (GtkCalendar *calendar,
                                                    G_GNUC_UNUSED gpointer user_data)
{
    GDateTime *gdt;

    if (g_par.show_days)
    {
        gdt = orage_cal_to_gdatetime (calendar, 1, 1);
        orage_week_window_build (gdt);
        g_date_time_unref (gdt);
    }
    else
        (void)create_el_win(NULL);
}

static gboolean upd_calendar (gpointer user_data)
{
    orage_window_classic_mark_appointments (ORAGE_WINDOW (user_data));
    month_change_timer = 0;

    return(FALSE); /* we do this only once */
}

static void mCalendar_month_changed_cb (GtkCalendar *calendar,
                                        gpointer user_data)
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
    month_change_timer = g_timeout_add (400, upd_calendar, user_data);
}

static void orage_window_classic_restore_geometry (OrageWindowClassic *window)
{
    GtkWindow *gwin = GTK_WINDOW (window);

    gtk_window_set_position (gwin, GTK_WIN_POS_NONE);

    if (g_par.size_x || g_par.size_y)
        gtk_window_set_default_size (gwin, g_par.size_x, g_par.size_y);

    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move (gwin, g_par.pos_x, g_par.pos_y);
}

static void build_menu (OrageWindowClassic *window)
{
    window->mMenubar = gtk_menu_bar_new();
    gtk_grid_attach_next_to (GTK_GRID (window->main_box), window->mMenubar,
                             NULL, GTK_POS_BOTTOM, 1, 1);

    /* File menu */
    window->mFile_menu = orage_menu_new (_("_File"), window->mMenubar);

    window->mFile_newApp = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-new", window->mFile_menu, window->mAccel_group);

    (void)orage_separator_menu_item_new (window->mFile_menu);

#ifdef ENABLE_SYNC
    window->mFile_refresh = orage_image_menu_item_for_parent_new_from_stock (
            "gtk-refresh", window->mFile_menu, window->mAccel_group);
#endif

    window->mFile_interface =  orage_menu_item_new_with_mnemonic (
            _("_Exchange data"), window->mFile_menu);

    (void)orage_separator_menu_item_new (window->mFile_menu);

    window->mFile_close = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-close", window->mFile_menu, window->mAccel_group);
    window->mFile_quit = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-quit", window->mFile_menu, window->mAccel_group);

    /* Edit menu */
    window->mEdit_menu = orage_menu_new (_("_Edit"), window->mMenubar);

    window->mEdit_preferences =
        orage_image_menu_item_for_parent_new_from_stock (
            "gtk-preferences", window->mEdit_menu, window->mAccel_group);

    /* View menu */
    window->mView_menu = orage_menu_new(_("_View"), window->mMenubar);

    window->mView_ViewSelectedDate = orage_menu_item_new_with_mnemonic (
            _("View selected _date"), window->mView_menu);

    window->mView_ViewSelectedWeek = orage_menu_item_new_with_mnemonic (
            _("View selected _week"), window->mView_menu);

    (void)orage_separator_menu_item_new (window->mView_menu);

    window->mView_selectToday = orage_menu_item_new_with_mnemonic (
            _("Select _Today"), window->mView_menu);

    /* Help menu */
    window->mHelp_menu = orage_menu_new (_("_Help"), window->mMenubar);
    window->mHelp_help = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-help", window->mHelp_menu, window->mAccel_group);
    window->mHelp_about = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-about", window->mHelp_menu, window->mAccel_group);

    gtk_widget_show_all (window->mMenubar);

    /* Signals */
    g_signal_connect (window->mFile_newApp, "activate",
                      G_CALLBACK (mFile_newApp_activate_cb), window);
#ifdef ENABLE_SYNC
    g_signal_connect (window->mFile_refresh, "activate",
                      G_CALLBACK (mFile_refresh_activate_cb), NULL);
#endif
    g_signal_connect (window->mFile_interface, "activate",
                      G_CALLBACK (mFile_interface_activate_cb), NULL);
    g_signal_connect (window->mFile_close, "activate",
                      G_CALLBACK (mFile_close_activate_cb), NULL);
    g_signal_connect (window->mFile_quit, "activate",
                      G_CALLBACK (mFile_quit_activate_cb), NULL);
    g_signal_connect (window->mEdit_preferences, "activate",
                      G_CALLBACK (mEdit_preferences_activate_cb), NULL);
    g_signal_connect (window->mView_ViewSelectedDate, "activate",
                      G_CALLBACK (mView_ViewSelectedDate_activate_cb), NULL);
    g_signal_connect (window->mView_ViewSelectedWeek, "activate",
                      G_CALLBACK (mView_ViewSelectedWeek_activate_cb), window);
    g_signal_connect (window->mView_selectToday, "activate",
                      G_CALLBACK(mView_selectToday_activate_cb), window);
    g_signal_connect (window->mHelp_help, "activate",
                      G_CALLBACK (mHelp_help_activate_cb), NULL);
    g_signal_connect (window->mHelp_about, "activate",
                      G_CALLBACK (mHelp_about_activate_cb), window);
}

static void todo_clicked (GtkWidget *widget, GdkEventButton *event,
                          G_GNUC_UNUSED gpointer *user_data)
{
    gchar *uid;
    GtkWidget *appointment_window;

    if (event->type==GDK_2BUTTON_PRESS) {
        uid = g_object_get_data(G_OBJECT(widget), "UID");
        appointment_window = orage_appointment_window_new_update (uid);
        gtk_window_present (GTK_WINDOW (appointment_window));
    }
}

static void add_info_row(xfical_appt *appt, GtkGrid *parentBox,
                         const gboolean todo)
{
    GtkWidget *ev, *label;
    gchar *tip, *tmp, *tmp_title, *tmp_note;
    gchar *tip_title, *tip_location, *tip_note;
    char  *s_time, *s_timeonly, *e_time, *c_time, *na;
    GDateTime *today;
    GDateTime *gdt_end_time;

    /***** add data into the vbox *****/
    ev = gtk_event_box_new();
    tmp_title = appt->title
            ? orage_process_text_commands(appt->title)
            : g_strdup(_("No title defined"));
    s_time = orage_gdatetime_to_i18_time (appt->starttimecur, appt->allDay);
    today = g_date_time_new_now_local ();
    if (todo) {
        e_time = appt->use_due_time ?
            orage_gdatetime_to_i18_time(appt->endtimecur, appt->allDay) :
            g_strdup (s_time);
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
                         "margin-start", 5,
                         "hexpand", TRUE,
                         "halign", GTK_ALIGN_FILL,
                         NULL);

    gtk_container_add(GTK_CONTAINER(ev), label);
    gtk_grid_attach_next_to (parentBox, ev, NULL, GTK_POS_BOTTOM, 1, 1);
    g_object_set_data_full(G_OBJECT(ev), "UID", g_strdup(appt->uid), g_free);
    g_signal_connect (ev, "button-press-event", G_CALLBACK (todo_clicked),
                      NULL);

    /***** set color *****/
    if (todo) {
        if (appt->use_due_time)
            gdt_end_time = g_date_time_ref (appt->endtimecur);
        else
            gdt_end_time = g_date_time_new_local (9999, 12, 31, 23, 59, 59);

        if (g_date_time_compare (gdt_end_time, today) < 0) /* gone */
            gtk_widget_set_name (label, ORAGE_TODO_COMPLETED);
        else if ((g_date_time_compare (appt->starttimecur, today) <= 0)
                 && (g_date_time_compare (gdt_end_time, today) >= 0))
        {
            gtk_widget_set_name (label, ORAGE_TODO_ACTUAL_NOW);
        }

        g_date_time_unref (gdt_end_time);
    }

    /***** set tooltip hint *****/
    tip_title = g_markup_printf_escaped(FORMAT_BOLD, tmp_title);
    if (appt->location) {
        tmp = g_markup_printf_escaped(FORMAT_BOLD, appt->location);
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

        tip = g_strdup_printf(
            _("Title: %s\n%s Start:\t%s\n Due:\t%s\n Done:\t%s%s"),
            tip_title, tip_location, s_time, e_time, c_time, tip_note);

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

    for (appt = xfical_appt_get_next_on_day(gdt, TRUE, 0
                , ical_type , file_type);
         appt;
         appt = xfical_appt_get_next_on_day(gdt, FALSE, 0
                , ical_type , file_type)) {
        *list = g_list_prepend(*list, appt);
    }
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
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    OrageWindowClassic *window = ORAGE_WINDOW_CLASSIC (ORAGE_WINDOW_CLASSIC (
        orage_application_get_window (app)));
    gboolean todo;

    todo = (pbox == window->mTodo_rows_vbox) ? TRUE : FALSE;
    if (appt->priority < g_par.priority_list_limit)
        add_info_row(appt, box, todo);
    xfical_appt_free(appt);
}

static void create_mainbox_todo_info (OrageWindowClassic *window)
{
    GtkScrolledWindow *sw;
    GtkWidget *todo_label;

    window->mTodo_vbox = gtk_grid_new ();
    g_object_set (window->mTodo_vbox, "vexpand", TRUE,
                                      "valign", GTK_ALIGN_FILL,
                                      NULL);
    gtk_grid_attach_next_to (GTK_GRID (window->main_box), window->mTodo_vbox,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    todo_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (todo_label), _("<b>To do:</b>"));
    gtk_grid_attach_next_to (GTK_GRID (window->mTodo_vbox), todo_label,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    g_object_set (todo_label, "xalign", 0.0, "yalign", 0.5, NULL);
    sw = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
    gtk_scrolled_window_set_policy (sw, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (sw, GTK_SHADOW_NONE);
    g_object_set (sw, "vexpand", TRUE, NULL);
    gtk_grid_attach_next_to (GTK_GRID (window->mTodo_vbox),
                             GTK_WIDGET (sw), NULL, GTK_POS_BOTTOM, 1, 1);
    window->mTodo_rows_vbox = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER (sw), window->mTodo_rows_vbox);
}

static void create_mainbox_event_info_box (OrageWindowClassic *window)
{
    GtkScrolledWindow *sw;
    GtkWidget *event_label;
    gchar *tmp, *tmp2, *tmp3;
    GDateTime *gdt;
    GDateTime *gdt_tmp;

    gdt = orage_cal_to_gdatetime (GTK_CALENDAR (window->mCalendar), 1, 1);

    window->mEvent_vbox = gtk_grid_new ();
    g_object_set (window->mEvent_vbox, "vexpand", TRUE,
                                       "valign", GTK_ALIGN_FILL,
                                       NULL);
    gtk_grid_attach_next_to (GTK_GRID (window->main_box), window->mEvent_vbox,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    event_label = gtk_label_new (NULL);
    if (g_par.show_event_days) {
    /* bug 7836: we call this routine also with 0 = no event data at all */
        if (g_par.show_event_days == 1) {
            tmp2 = orage_gdatetime_to_i18_time (gdt, TRUE);
            tmp = g_strdup_printf(_("<b>Events for %s:</b>"), tmp2);
            g_free(tmp2);
        }
        else {
            tmp2 = orage_gdatetime_to_i18_time (gdt, TRUE);

            gdt_tmp = gdt;
            gdt = g_date_time_add_days (gdt_tmp, g_par.show_event_days - 1);
            g_date_time_unref (gdt_tmp);
            tmp3 = orage_gdatetime_to_i18_time (gdt, TRUE);
            tmp = g_strdup_printf(_("<b>Events for %s - %s:</b>"), tmp2, tmp3);
            g_free(tmp2);
            g_free(tmp3);
        }
        gtk_label_set_markup (GTK_LABEL (event_label), tmp);
        g_free(tmp);
    }

    g_date_time_unref (gdt);
    g_object_set (event_label, "xalign", 0.0, "yalign", 0.5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (window->mEvent_vbox),
                             event_label, NULL, GTK_POS_BOTTOM, 1, 1);
    sw = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
    gtk_scrolled_window_set_policy (sw, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (sw, GTK_SHADOW_NONE);
    g_object_set (sw, "expand", TRUE, NULL);
    gtk_grid_attach_next_to (GTK_GRID (window->mEvent_vbox),
                             GTK_WIDGET (sw), NULL, GTK_POS_BOTTOM, 1, 1);
    window->mEvent_rows_vbox = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER (sw), window->mEvent_rows_vbox);
}

static void build_mainbox_todo_info (OrageWindowClassic *window)
{
    GDateTime *gdt;
    xfical_type ical_type;
    gchar file_type[8];
    gint i;
    GList *todo_list=NULL;

    g_return_if_fail (window != NULL);

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
        gtk_widget_destroy (window->mTodo_vbox);
        create_mainbox_todo_info (window);
        todo_list = g_list_sort(todo_list, todo_order);
        g_list_foreach (todo_list, info_process, window->mTodo_rows_vbox);
        g_list_free(todo_list);
        todo_list = NULL;
        gtk_widget_show_all (window->mTodo_vbox);
    }
    else {
        gtk_widget_hide (window->mTodo_vbox);
    }
}

static void build_mainbox_event_info (OrageWindowClassic *window)
{
    xfical_type ical_type;
    gchar file_type[8];
    gint i;
    GList *event_list=NULL;
    GDateTime *gdt;

    g_return_if_fail (window != NULL);

    if (g_par.show_event_days) {
        gdt = orage_cal_to_gdatetime (GTK_CALENDAR (window->mCalendar), 1, 1);
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
        gtk_widget_destroy (window->mEvent_vbox);
        create_mainbox_event_info_box (window);
        event_list = g_list_sort(event_list, event_order);
        g_list_foreach (event_list, info_process, window->mEvent_rows_vbox);
        g_list_free(event_list);
        event_list = NULL;
        gtk_widget_show_all(window->mEvent_vbox);
    }
    else
        gtk_widget_hide(window->mEvent_vbox);
}

static void mCalendar_day_selected_cb (G_GNUC_UNUSED GtkCalendar *calendar,
                                       gpointer user_data)
{
    /* rebuild the info for the selected date */
    orage_window_classic_build_events (ORAGE_WINDOW_CLASSIC (user_data));
}

void orage_window_classic_build_events (OrageWindowClassic *window)
{
    if (!xfical_file_open(TRUE))
        return;
    build_mainbox_event_info (window);
    xfical_file_close(TRUE);
}

void orage_window_classic_build_todo (OrageWindowClassic *window)
{
    if (!xfical_file_open (TRUE))
        return;
    build_mainbox_todo_info (window);
    xfical_file_close (TRUE);
}

static void orage_window_classic_class_init (OrageWindowClassicClass *klass)
{
}

static void orage_window_classic_interface_init (OrageWindowInterface *iface)
{
    iface->get_calendar = orage_window_classic_get_calendar;
    iface->raise = orage_window_classic_raise;
}

static void orage_window_classic_init (OrageWindowClassic *self)
{
    gtk_widget_set_name (GTK_WIDGET (self), "OrageWindowClassic");

    self->main_box = gtk_grid_new ();
    gtk_container_add (GTK_CONTAINER (self), self->main_box);
    gtk_widget_show (self->main_box);

    self->mAccel_group = gtk_accel_group_new ();

    /* Build the menu */
    build_menu (self);

    /* Build the calendar */
    self->mCalendar = gtk_calendar_new ();
    g_object_set (self->mCalendar, "hexpand", TRUE,
                                   "halign", GTK_ALIGN_FILL,
                                   NULL);
    gtk_grid_attach_next_to (GTK_GRID (self->main_box), self->mCalendar, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    gtk_widget_show (self->mCalendar);

    /* Build the Info boxes */
    create_mainbox_todo_info (self);
    create_mainbox_event_info_box (self);

    gtk_window_add_accel_group (GTK_WINDOW (self), self->mAccel_group);

    /* Signals */
    g_signal_connect (self, "notify::application",
                      G_CALLBACK (orage_window_classic_post_init_cb), NULL);
    g_signal_connect (self->mCalendar, "day_selected_double_click",
                      G_CALLBACK (mCalendar_day_selected_double_click_cb),
                      NULL);
    g_signal_connect (self->mCalendar, "day_selected",
                      G_CALLBACK (mCalendar_day_selected_cb), self);
    g_signal_connect (self->mCalendar, "month-changed",
                      G_CALLBACK (mCalendar_month_changed_cb), self);
}

GtkWidget *orage_window_classic_new (OrageApplication *application)
{
    return g_object_new (ORAGE_WINDOW_CLASSIC_TYPE,
                         "application", GTK_APPLICATION (application),
                         NULL);
}

void orage_window_classic_show_menubar (OrageWindowClassic *window,
                                        const gboolean show)
{
    if (show)
        gtk_widget_show (window->mMenubar);
    else
        gtk_widget_hide (window->mMenubar);
}

void orage_window_classic_hide_todo (OrageWindowClassic *window)
{
    gtk_widget_hide (window->mTodo_vbox);
}

void orage_window_classic_hide_event (OrageWindowClassic *window)
{
    gtk_widget_hide (window->mEvent_vbox);
}

GtkCalendar *orage_window_classic_get_calendar (OrageWindow *window)
{
    return GTK_CALENDAR (ORAGE_WINDOW_CLASSIC (window)->mCalendar);
}

void orage_window_classic_build_info (OrageWindowClassic *window)
{
    build_mainbox_todo_info (window);
    build_mainbox_event_info (window);
}

void orage_window_classic_month_changed (OrageWindow *window)
{
    mCalendar_month_changed_cb (orage_window_classic_get_calendar (window),
                                window);
}

void orage_window_classic_raise (OrageWindow *window)
{
    GtkWindow *gtk_window = GTK_WINDOW (window);

    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move (gtk_window, g_par.pos_x, g_par.pos_y);

    if (g_par.select_always_today)
        orage_select_today (orage_window_classic_get_calendar (window));

    if (g_par.set_stick)
        gtk_window_stick (gtk_window);

    gtk_window_set_keep_above (gtk_window, g_par.set_ontop);
    gtk_window_present (gtk_window);
}
