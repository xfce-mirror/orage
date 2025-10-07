/*
 * Copyright (c) 2021-2024 Erkki Moorits
 * Copyright (c) 2007-2013 Juha Kautto  (juha at xfce.org)
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
#include <string.h>
#include <time.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "orage-i18n.h"
#include "orage-css.h"
#include "functions.h"
#include "orage-time-utils.h"
#include "orage-week-window.h"
#include "ical-code.h"
#include "parameters.h"
#include "event-list.h"
#include "orage-appointment-window.h"
#include "orage-category.h"

#define MAX_DAYS 40

#define BUTTON_ROW 0
#define FULL_DAY_ROW (BUTTON_ROW + 1)
#define FIRST_HOUR_ROW (FULL_DAY_ROW + 1)
#define DATE_KEY "button-date"
#define ORAGE_WEEK_WINDOW_START_DATE_PROPERTY "day-window-start-date"

struct _OrageWeekWindow
{
    GtkWindow parent;

    GtkAccelGroup *accel_group;

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
#if 0
    GtkRequisition StartDate_button_req;
#endif
    GtkWidget *day_spin;

    GtkWidget *scroll_win;
    GtkWidget *dtable; /* day table */
    GtkWidget *header[MAX_DAYS];
    GtkWidget *element[24][MAX_DAYS];
    GtkWidget *line[24][MAX_DAYS];
    guint upd_timer;
    gdouble scroll_pos; /* remember the scroll position */

    gint days; /* how many days to show */

    GList *apptw_list; /* keep track of appointments being updated */
    GDateTime *a_day; /* Start date. FIXME: only one start date. */
    GDateTime *start_date; /* FIXME: another start date. */
};

G_DEFINE_TYPE (OrageWeekWindow, orage_week_window, GTK_TYPE_WINDOW)

enum
{
    PROP_0,
    PROP_START_DATE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = {NULL,};

static void set_scroll_position (const OrageWeekWindow *dw)
{
    GtkAdjustment *v_adj;

    v_adj = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(dw->scroll_win));
    if (dw->scroll_pos > 0) /* we have old value */
        gtk_adjustment_set_value(v_adj, dw->scroll_pos);
    else if (dw->scroll_pos < 0)
    {
        /* default: let's try to start roughly from line 8 = 8 o'clock */
        gtk_adjustment_set_value(v_adj, gtk_adjustment_get_upper (v_adj) / 3);
    }
}

static gboolean scroll_position_timer (const gpointer user_data)
{
    set_scroll_position (ORAGE_WEEK_WINDOW (user_data));

    return(FALSE); /* only once */
}

static void get_scroll_position (OrageWeekWindow *dw)
{
    GtkAdjustment *v_adj;

    v_adj = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(dw->scroll_win));
    dw->scroll_pos = gtk_adjustment_get_value(v_adj);
}

static GtkWidget *build_separator_bar (const gchar *widget_name)
{
    GtkWidget *bar = gtk_label_new ("");

    g_object_set (bar, "hexpand", FALSE,
                       "vexpand", TRUE,
                       "margin-left", 1,
                       "margin-right", 1, NULL);

    gtk_widget_set_name (bar, widget_name);

    return bar;
}

static GtkWidget *build_line (const GtkWidget *hour_line)
{
    const gboolean first = (hour_line == NULL);
    const gchar *widget_name = first ? ORAGE_WEEK_WINDOW_SEPARATOR_BAR
                                     : ORAGE_WEEK_WINDOW_OCCUPIED_HOUR_LINE;

    return build_separator_bar (widget_name);
}

static void close_window (gpointer user_data)
{
    OrageAppointmentWindow *apptw;
    GList *apptw_list;
    OrageWeekWindow *window = ORAGE_WEEK_WINDOW (user_data);

    gtk_window_get_size (GTK_WINDOW (window),
                         &g_par.dw_size_x,
                         &g_par.dw_size_y);

    gtk_window_get_position (GTK_WINDOW (window),
                             &g_par.dw_pos_x,
                             &g_par.dw_pos_y);
    write_parameters();

    /* need to clean the appointment list and inform all appointments that
     * we are not interested anymore (= should not get updated) */
    apptw_list = window->apptw_list;
    for (apptw_list = g_list_first(apptw_list);
         apptw_list != NULL;
         apptw_list = g_list_next(apptw_list)) {
        apptw = ORAGE_APPOINTMENT_WINDOW (apptw_list->data);
        if (apptw) /* appointment window is still alive */
        {
            /* not interested anymore */
            orage_appointment_window_set_day_window (apptw, NULL);
        }
        else
            g_warning ("%s: not null appt window", G_STRFUNC);
    }

    gtk_widget_destroy (GTK_WIDGET (window));
}

static gboolean on_Window_delete_event (G_GNUC_UNUSED GtkWidget *w,
                                        G_GNUC_UNUSED GdkEvent *e,
                                        gpointer user_data)
{
    close_window (user_data);

    return(FALSE);
}

static void on_File_close_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                       gpointer user_data)
{
    close_window (user_data);
}

static void on_Close_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    close_window (user_data);
}

static void create_new_appointment (OrageWeekWindow *dw)
{
    GDateTime *gdt;
    GtkWidget *appointment_window;

    gdt = g_object_get_data (G_OBJECT (dw->StartDate_button), DATE_KEY);
    appointment_window = orage_appointment_window_new (gdt);
    /* inform the appointment that we are interested in it */
    orage_appointment_window_set_day_window (
            ORAGE_APPOINTMENT_WINDOW (appointment_window), dw);
    gtk_window_present (GTK_WINDOW (appointment_window));
    /* we started this, so keep track of it */
    dw->apptw_list = g_list_prepend (dw->apptw_list, appointment_window);
}

static void on_File_newApp_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                        gpointer user_data)
{
    create_new_appointment (ORAGE_WEEK_WINDOW (user_data));
}

static void on_Create_toolbutton_clicked_cb (G_GNUC_UNUSED GtkButton *mi,
                                             gpointer user_data)
{
    create_new_appointment (ORAGE_WEEK_WINDOW (user_data));
}

static void on_View_refresh_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                         gpointer user_data)
{
    orage_week_window_refresh (ORAGE_WEEK_WINDOW (user_data));
}

static void on_Refresh_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    orage_week_window_refresh (ORAGE_WEEK_WINDOW (user_data));
}

static void changeSelectedDate (OrageWeekWindow *dw, const gint day)
{
    gchar *label;
    GDateTime *gdt_o;
    GDateTime *gdt_m;

    gdt_o = g_object_get_data (G_OBJECT (dw->StartDate_button), DATE_KEY);
    gdt_m = g_date_time_add_days (gdt_o, day);
    label = orage_gdatetime_to_i18_time (gdt_m, TRUE);
    gtk_button_set_label (GTK_BUTTON (dw->StartDate_button), label);
    g_object_set_data_full (G_OBJECT (dw->StartDate_button),
                            DATE_KEY, gdt_m, (GDestroyNotify)g_date_time_unref);
    g_free (label);
    orage_week_window_refresh (dw);
}

static void go_to_today (OrageWeekWindow *dw)
{
    GDateTime *gdt;
    gchar *today;

    gdt = g_date_time_new_now_local ();
    today = orage_gdatetime_to_i18_time (gdt, TRUE);
    gtk_button_set_label (GTK_BUTTON (dw->StartDate_button), today);
    g_object_set_data_full (G_OBJECT (dw->StartDate_button),
                            DATE_KEY, gdt, (GDestroyNotify)g_date_time_unref);
    g_free (today);
    orage_week_window_refresh (dw);
}

static void on_Today_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    go_to_today (ORAGE_WEEK_WINDOW (user_data));
}

static void on_Go_today_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                     gpointer user_data)
{
    go_to_today (ORAGE_WEEK_WINDOW (user_data));
}

static void on_Go_previous_week_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                             gpointer user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), -7);
}

static void on_Go_previous_day_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                            gpointer user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), -1);
}

static void on_Previous_day_clicked (G_GNUC_UNUSED GtkButton *b,
                                     gpointer user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), -1);
}

static void on_Previous_week_clicked (G_GNUC_UNUSED GtkButton *b,
                                      gpointer user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), -7);
}

static void on_Go_next_day_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                        gpointer user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), 1);
}

static void on_Go_next_week_activate_cb (G_GNUC_UNUSED GtkMenuItem *mi,
                                         gpointer user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), 7);
}

static void on_Next_day_clicked (G_GNUC_UNUSED GtkButton *b, gpointer user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), 1);
}

static void on_Next_week_clicked (G_GNUC_UNUSED GtkButton *b,
                                  gpointer user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), 7);
}

static void build_menu (OrageWeekWindow *dw)
{
    dw->Menubar = gtk_menu_bar_new();
    gtk_grid_attach (GTK_GRID (dw->Vbox), dw->Menubar, 0, 0, 1, 1);

    /********** File menu **********/
    dw->File_menu = orage_menu_new(_("_File"), dw->Menubar);
    dw->File_menu_new = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-new", dw->File_menu, dw->accel_group);

    (void)orage_separator_menu_item_new(dw->File_menu);

    dw->File_menu_close = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-close", dw->File_menu, dw->accel_group);

    /********** View menu **********/
    dw->View_menu = orage_menu_new(_("_View"), dw->Menubar);
    dw->View_menu_refresh = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-refresh", dw->View_menu, dw->accel_group);
    gtk_widget_add_accelerator(dw->View_menu_refresh
            , "activate", dw->accel_group
            , GDK_KEY_r, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(dw->View_menu_refresh
            , "activate", dw->accel_group
            , GDK_KEY_Return, 0, 0);
    gtk_widget_add_accelerator(dw->View_menu_refresh
            , "activate", dw->accel_group
            , GDK_KEY_KP_Enter, 0, 0);

    /********** Go menu   **********/
    dw->Go_menu = orage_menu_new(_("_Go"), dw->Menubar);

    dw->Go_menu_today = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-home", dw->Go_menu, dw->accel_group);
    gtk_widget_add_accelerator(dw->Go_menu_today
            , "activate", dw->accel_group
            , GDK_KEY_Home, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(dw->Go_menu_today
            , "activate", dw->accel_group
            , GDK_KEY_KP_Home, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

    dw->Go_menu_prev_week = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-go-up", dw->Go_menu, dw->accel_group);
    gtk_widget_add_accelerator(dw->Go_menu_prev_week
            , "activate", dw->accel_group
            , GDK_KEY_Page_Up, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(dw->Go_menu_prev_week
            , "activate", dw->accel_group
            , GDK_KEY_KP_Page_Up, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

    dw->Go_menu_prev_day = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-go-back", dw->Go_menu, dw->accel_group);
    gtk_widget_add_accelerator(dw->Go_menu_prev_day
            , "activate", dw->accel_group
            , GDK_KEY_Left, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(dw->Go_menu_prev_day
            , "activate", dw->accel_group
            , GDK_KEY_KP_Left, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

    dw->Go_menu_next_day = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-go-forward", dw->Go_menu, dw->accel_group);
    gtk_widget_add_accelerator(dw->Go_menu_next_day
            , "activate", dw->accel_group
            , GDK_KEY_Right, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(dw->Go_menu_next_day
            , "activate", dw->accel_group
            , GDK_KEY_KP_Right, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

    dw->Go_menu_next_week = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-go-down", dw->Go_menu, dw->accel_group);
    gtk_widget_add_accelerator(dw->Go_menu_next_week
            , "activate", dw->accel_group
            , GDK_KEY_Page_Down, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(dw->Go_menu_next_week
            , "activate", dw->accel_group
            , GDK_KEY_KP_Page_Down, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);

    g_signal_connect((gpointer)dw->File_menu_new, "activate"
            , G_CALLBACK(on_File_newApp_activate_cb), dw);
    g_signal_connect((gpointer)dw->File_menu_close, "activate"
            , G_CALLBACK(on_File_close_activate_cb), dw);
    g_signal_connect((gpointer)dw->View_menu_refresh, "activate"
            , G_CALLBACK(on_View_refresh_activate_cb), dw);
    g_signal_connect((gpointer)dw->Go_menu_today, "activate"
            , G_CALLBACK(on_Go_today_activate_cb), dw);
    g_signal_connect((gpointer)dw->Go_menu_prev_week, "activate"
            , G_CALLBACK(on_Go_previous_week_activate_cb), dw);
    g_signal_connect((gpointer)dw->Go_menu_prev_day, "activate"
            , G_CALLBACK(on_Go_previous_day_activate_cb), dw);
    g_signal_connect((gpointer)dw->Go_menu_next_day, "activate"
            , G_CALLBACK(on_Go_next_day_activate_cb), dw);
    g_signal_connect((gpointer)dw->Go_menu_next_week, "activate"
            , G_CALLBACK(on_Go_next_week_activate_cb), dw);
}

static void build_toolbar (OrageWeekWindow *dw)
{
    int i = 0;

    dw->Toolbar = gtk_toolbar_new();
    gtk_grid_attach (GTK_GRID (dw->Vbox), dw->Toolbar, 0, 1, 1, 1);

    dw->Create_toolbutton = orage_toolbar_append_button(dw->Toolbar
            , "document-new", _("New"), i++);

    (void)orage_toolbar_append_separator(dw->Toolbar, i++);

    dw->Previous_week_toolbutton = orage_toolbar_append_button(dw->Toolbar
            , "go-up", _("Back one week"), i++);
    dw->Previous_day_toolbutton = orage_toolbar_append_button(dw->Toolbar
            , "go-previous", _("Back one day"), i++);
    dw->Today_toolbutton = orage_toolbar_append_button(dw->Toolbar
            , "go-home", _("Today"), i++);
    dw->Next_day_toolbutton = orage_toolbar_append_button(dw->Toolbar
            , "go-next", _("Forward one day"), i++);
    dw->Next_week_toolbutton = orage_toolbar_append_button(dw->Toolbar
            , "go-down", _("Forward one week"), i++);

    (void)orage_toolbar_append_separator(dw->Toolbar, i++);

    dw->Refresh_toolbutton = orage_toolbar_append_button(dw->Toolbar
            , "view-refresh", _("Refresh"), i++);

    (void)orage_toolbar_append_separator(dw->Toolbar, i++);

    dw->Close_toolbutton = orage_toolbar_append_button(dw->Toolbar
            , "window-close", _("Close"), i++);

    g_signal_connect((gpointer)dw->Create_toolbutton, "clicked"
            , G_CALLBACK(on_Create_toolbutton_clicked_cb), dw);
    g_signal_connect((gpointer)dw->Previous_week_toolbutton, "clicked"
            , G_CALLBACK(on_Previous_week_clicked), dw);
    g_signal_connect((gpointer)dw->Previous_day_toolbutton, "clicked"
            , G_CALLBACK(on_Previous_day_clicked), dw);
    g_signal_connect((gpointer)dw->Today_toolbutton, "clicked"
            , G_CALLBACK(on_Today_clicked), dw);
    g_signal_connect((gpointer)dw->Next_day_toolbutton, "clicked"
            , G_CALLBACK(on_Next_day_clicked), dw);
    g_signal_connect((gpointer)dw->Next_week_toolbutton, "clicked"
            , G_CALLBACK(on_Next_week_clicked), dw);
    g_signal_connect((gpointer)dw->Refresh_toolbutton, "clicked"
            , G_CALLBACK(on_Refresh_clicked), dw);
    g_signal_connect((gpointer)dw->Close_toolbutton, "clicked"
             , G_CALLBACK(on_Close_clicked), dw);
}

static gboolean upd_day_view (OrageWeekWindow *dw)
{
    static guint day_cnt=-1;
    guint day_cnt_n;

    /* we only need to do this if it is really a new day count. We may get
     * many of these while spin button is changing day count and it is enough
     * to show only the last one, which is visible */
    day_cnt_n = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dw->day_spin));
    if (day_cnt != day_cnt_n) { /* need really do it */
        orage_week_window_refresh (dw);
        day_cnt = day_cnt_n;
    }
    dw->upd_timer = 0;
    return(FALSE); /* we do this only once */
}

static void on_spin_changed (G_GNUC_UNUSED GtkSpinButton *b,
                             gpointer *user_data)
{
    OrageWeekWindow *dw = ORAGE_WEEK_WINDOW (user_data);

    /* refresh_day_win is rather heavy (=slow), so doing it here 
     * is not a good idea. We can't keep up with repeated quick presses 
     * if we do the whole thing here. So let's throw it to background 
     * and do it later. */
    if (dw->upd_timer) {
        g_source_remove(dw->upd_timer);       
    }
    dw->upd_timer = g_timeout_add(500, (GSourceFunc)upd_day_view, dw);
}

static void on_Date_button_clicked_cb(GtkWidget *button, gpointer *user_data)
{
    OrageWeekWindow *window = ORAGE_WEEK_WINDOW (user_data);
    GtkWidget *selDate_dialog;
    selDate_dialog = gtk_dialog_new_with_buttons (
        _("Pick the date"), GTK_WINDOW (window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        _("Today"), 1, _("_OK"), GTK_RESPONSE_ACCEPT, NULL);

    if (orage_date_button_clicked (button, selDate_dialog))
        orage_week_window_refresh (window);
}

static void header_button_clicked_cb (GtkWidget *button,
                                      G_GNUC_UNUSED gpointer *user_data)
{
    GDateTime *gdt;
    gdt = g_object_get_data (G_OBJECT (button), DATE_KEY);
    (void)create_el_win (gdt);
}

static void on_button_press_event_cb(GtkWidget *widget
        , GdkEventButton *event, gpointer *user_data)
{
    GtkWidget *appointment_window;
    gchar *uid;
    OrageWeekWindow *dw = ORAGE_WEEK_WINDOW (user_data);

    if (event->type == GDK_2BUTTON_PRESS) {
        uid = g_object_get_data(G_OBJECT(widget), "UID");
        appointment_window = orage_appointment_window_new_update (uid);

        /* inform the appointment that we are interested in it */
        orage_appointment_window_set_day_window (
            ORAGE_APPOINTMENT_WINDOW (appointment_window), dw);

        gtk_window_present (GTK_WINDOW (appointment_window));

        /* we started this, so keep track of it */
        dw->apptw_list = g_list_prepend(dw->apptw_list, appointment_window);
    }
}

static void on_arrow_up_press_event_cb (G_GNUC_UNUSED GtkWidget *widget,
                                        G_GNUC_UNUSED GdkEventButton *event,
                                        gpointer *user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), -7);
}

static void on_arrow_left_press_event_cb (G_GNUC_UNUSED GtkWidget *widget,
                                          G_GNUC_UNUSED GdkEventButton *event,
                                          gpointer *user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), -1);
}

static void on_arrow_right_press_event_cb (G_GNUC_UNUSED GtkWidget *widget,
                                           G_GNUC_UNUSED GdkEventButton *event,
                                           gpointer *user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), 1);
}

static void on_arrow_down_press_event_cb (G_GNUC_UNUSED GtkWidget *widget,
                                          G_GNUC_UNUSED GdkEventButton *event,
                                          gpointer *user_data)
{
    changeSelectedDate (ORAGE_WEEK_WINDOW (user_data), 7);
}

static gchar *get_row_css_name (const gint row)
{
    const gint row_mod = row % 2;

    if (row_mod == 0)
        return ORAGE_WEEK_WINDOW_EVEN_HOURS;
    else
        return ORAGE_WEEK_WINDOW_ODD_HOURS;
}

static void add_row (OrageWeekWindow *dw, const xfical_appt *appt)
{
    gint row, start_row, end_row, days;
    gint col, start_col, end_col, first_col, last_col;
    gchar *tip, *start_date, *end_date, *tmp_title, *tip_title;
    gchar *tmp_note, *tip_note;
    GtkWidget *ev, *lab, *hb;
    GDateTime *gdt_start;
    GDateTime *gdt_end;
    GDateTime *gdt_first;
    gint start_hour;
    gint end_hour;

    /* First clarify timings */
    gdt_start = g_date_time_ref (appt->starttimecur);
    gdt_end   = g_date_time_ref (appt->endtimecur);
    gdt_first = g_date_time_ref (dw->a_day);

    start_col = orage_gdatetime_days_between (gdt_first, gdt_start) + 1;
    end_col   = orage_gdatetime_days_between (gdt_first, gdt_end) + 1;
    days      = orage_gdatetime_days_between (gdt_start, gdt_end);

    g_date_time_unref (gdt_first);

    if (start_col > dw->days)
    {
        /* Can happen if timezones pass date change. This does not fit, so we
         * just skip it.
         */
        goto add_row_cleanup;
    }

    if (start_col < 1) {
        col = 1;
        row = 0;
    }
    else {
        col = start_col;
        row = g_date_time_get_hour (gdt_start);
    }
    if (end_col < 1)
    {
        /* Can happen if timezones pass date change. This does not fit, so we
         * just skip it.
         */
        goto add_row_cleanup;
    }
    if (end_col > dw->days) { /* can happen if timezones pass date change */
        end_col = days;
    }

    /* then add the appointment */
    tmp_title = orage_process_text_commands(
            appt->title ? appt->title : _("Unknown"));
    tip_title = g_markup_printf_escaped("<b> %s </b>", tmp_title);
    tmp_note = orage_process_text_commands(
            appt->note ? appt->note : "");
    tmp_note = orage_limit_text(tmp_note, 50, 10);
    tip_note = g_markup_escape_text(tmp_note, strlen(tmp_note));
    g_free(tmp_note);
    ev = gtk_event_box_new();
    lab = gtk_label_new(tmp_title);
    gtk_container_add(GTK_CONTAINER(ev), lab);

    if (appt->allDay) { /* whole day event */
        gtk_widget_set_name (ev, ORAGE_WEEK_WINDOW_ALL_DAY_EVENT);
        if (dw->header[col] == NULL) { /* first data */
            hb = gtk_grid_new ();
            dw->header[col] = hb;
        }
        else
            hb = dw->header[col];

        start_date = orage_gdatetime_to_i18_time (gdt_start, TRUE);
        if (days == 0)
            tip = g_strdup_printf("%s\n%s\n%s"
                    , tip_title, start_date, tip_note);
        else {
            end_date = orage_gdatetime_to_i18_time (gdt_end, TRUE);
            tip = g_strdup_printf("%s\n%s - %s\n%s"
                    , tip_title, start_date, end_date, tip_note);
            g_free(end_date);
        }
        g_free(start_date);
    }
    else {
        gtk_widget_set_name (ev, get_row_css_name (row));

        if (dw->element[row][col] == NULL) {
            hb = gtk_grid_new ();
            dw->element[row][col] = hb;
        }
        else
            hb = dw->element[row][col];

        if (days == 0)
        {
            tip = g_strdup_printf ("%s\n%02d:%02d-%02d:%02d\n%s", tip_title,
                    g_date_time_get_hour (gdt_start),
                    g_date_time_get_minute (gdt_start),
                    g_date_time_get_hour (gdt_end),
                    g_date_time_get_minute (gdt_end),
                    tip_note);
        }
        else {
            start_date = orage_gdatetime_to_i18_time (gdt_start, TRUE);
            end_date = orage_gdatetime_to_i18_time (gdt_end, TRUE);
            tip = g_strdup_printf ("%s\n%s %02d:%02d - %s %02d:%02d\n%s",
                    tip_title, start_date,
                    g_date_time_get_hour (gdt_start),
                    g_date_time_get_minute (gdt_start),
                    end_date,
                    g_date_time_get_hour (gdt_end),
                    g_date_time_get_minute (gdt_end),
                    tip_note);
            g_free(start_date);
            g_free(end_date);
        }
    }
    gtk_widget_set_tooltip_markup(ev, tip);
    g_object_set (ev, "expand", TRUE,
                      "margin-left", 1, "margin-right", 1, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hb), ev, NULL, GTK_POS_BOTTOM, 1, 1);
    g_object_set_data_full (G_OBJECT(ev), "UID", g_strdup (appt->uid), g_free);
    g_signal_connect (ev, "button-press-event",
                      G_CALLBACK (on_button_press_event_cb), dw);
    g_free(tmp_title);
    g_free(tip);
    g_free(tip_title);
    g_free(tip_note);

    /* and finally draw the line to show how long the appointment is,
     * but only if it is Busy type event (=availability != 0)
     * and it is not whole day event */
    if (appt->availability && !appt->allDay) {
        first_col = (start_col < 1) ? 1 : start_col;
        last_col = (end_col > dw->days) ? dw->days : end_col;
        start_hour = g_date_time_get_hour (gdt_start);
        end_hour = g_date_time_get_hour (gdt_end);

        for (col = first_col; col <= last_col; col++)
        {
            start_row = (col == start_col) ? start_hour : 0;
            end_row = (col == end_col) ? end_hour : 23;

            for (row = start_row; row <= end_row; row++)
                dw->line[row][col] = build_line (dw->line[row][col]);
        }
    }

add_row_cleanup:
    g_date_time_unref (gdt_start);
    g_date_time_unref (gdt_end);
}

static void app_rows (OrageWeekWindow *dw,
                      const xfical_type ical_type,
                      const gchar *file_type)
{
    GList *appt_list=NULL, *tmp;
    xfical_appt *appt;

    xfical_get_each_app_within_time (dw->a_day, dw->days, ical_type, file_type,
                                     &appt_list);
    for (tmp = g_list_first(appt_list);
         tmp != NULL;
         tmp = g_list_next(tmp)) {
        appt = (xfical_appt *)tmp->data;
        if (appt->priority < g_par.priority_list_limit) { 
            add_row(dw, appt);
        }
        xfical_appt_free(appt);
    }
    g_list_free(appt_list);
}

static void app_data (OrageWeekWindow *dw)
{
    xfical_type ical_type;
    gchar file_type[8];
    gint i;
    GDateTime *a_day;

    ical_type = XFICAL_TYPE_EVENT;
    a_day = g_object_get_data (G_OBJECT (dw->StartDate_button), DATE_KEY);
    g_date_time_unref (dw->a_day);
    dw->a_day = g_date_time_ref (a_day);
    dw->days = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dw->day_spin));

    /* first search base orage file */
    if (!xfical_file_open(TRUE))
        return;
    (void)g_strlcpy (file_type, "O00.", sizeof (file_type));
    app_rows(dw, ical_type, file_type);

    /* then process all foreign files */
    for (i = 0; i < g_par.foreign_count; i++) {
        (void)g_snprintf(file_type, sizeof (file_type), "F%02d.", i);
        app_rows(dw, ical_type, file_type);
    }

    xfical_file_close(TRUE);
}

static void fill_days (OrageWeekWindow *dw, const gint days)
{
    const gint days_n1 = days + 1;
    gint row, col;
    GtkWidget *ev, *hb;
    GtkWidget *marker;

    /* first clear the structure */
    for (col = 1; col < days_n1; col++) {
        dw->header[col] = NULL;
        for (row = 0; row < 24; row++) {
            dw->element[row][col] = NULL;
            dw->line[row][col] = build_line (NULL);
        }
    }

    app_data(dw);

    for (col = 1; col < days_n1; col++) {
        hb = gtk_grid_new ();
        marker = build_line (NULL);
        gtk_grid_attach (GTK_GRID (hb), marker, 0, 0, 1, 1);
        /* check if we have full day events and put them to header */
        if (dw->header[col])
            ev = dw->header[col];
        else
            ev = gtk_event_box_new ();

        g_object_set (ev, "hexpand", TRUE,
                          "halign", GTK_ALIGN_FILL, NULL);
        gtk_widget_set_name (ev, ORAGE_WEEK_WINDOW_ALL_DAY_EVENT);

        gtk_grid_attach_next_to (GTK_GRID (hb), ev,
                                 marker, GTK_POS_RIGHT, 1, 1);

        gtk_grid_attach (GTK_GRID (dw->dtable), hb, col, FULL_DAY_ROW, 1, 1);

        /* check rows */
        for (row = 0; row < 24; row++) {
            hb = gtk_grid_new ();

            marker = dw->line[row][col];
            gtk_grid_attach (GTK_GRID (hb), marker, 0, 0, 1, 1);

            if (dw->element[row][col])
                ev = dw->element[row][col];
            else
            {
                ev = gtk_event_box_new ();
                gtk_widget_set_name (ev, get_row_css_name (row));
            }

            g_object_set (ev, "hexpand", TRUE,
                              "halign", GTK_ALIGN_FILL, NULL);

            gtk_grid_attach_next_to (GTK_GRID (hb), ev,
                                     marker, GTK_POS_RIGHT, 1, 1);
            gtk_grid_attach (GTK_GRID (dw->dtable), hb,
                             col, row + FIRST_HOUR_ROW, 1, 1);
        }
    }
}

static void build_day_view_header (OrageWeekWindow *dw)
{
    GtkWidget *start_label;
    GtkWidget *no_days_label;
    GtkWidget *grid;
    gchar *first_date;
    int diff_to_weeks_first_day;
    GDateTime *gdt;

    grid = gtk_grid_new ();
    g_object_set (grid, "margin", 10, NULL);
    gtk_grid_attach (GTK_GRID (dw->Vbox), grid, 0, 2, 1, 1);

    start_label = gtk_label_new (_("Start"));
    g_object_set (start_label, "margin-left", 10, "margin-right", 10, NULL);
    gtk_grid_attach (GTK_GRID (grid), start_label, 0, 0, 1, 1);

    /* start date button */
    dw->StartDate_button = gtk_button_new();
    gtk_grid_attach_next_to (GTK_GRID (grid), dw->StartDate_button, start_label,
                             GTK_POS_RIGHT, 1, 1);
    no_days_label = gtk_label_new (_("Number of days to show"));
    g_object_set (no_days_label, "margin-left", 40, "margin-right", 10, NULL);
    gtk_grid_attach_next_to (GTK_GRID (grid), no_days_label,
                             dw->StartDate_button, GTK_POS_RIGHT, 1, 1);

    /* show days spin = how many days to show */
    dw->day_spin = gtk_spin_button_new_with_range(1, MAX_DAYS, 1);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(dw->day_spin), TRUE);
    gtk_grid_attach_next_to (GTK_GRID (grid), dw->day_spin, no_days_label,
                             GTK_POS_RIGHT, 1, 1);

    /* initial values */
    if (g_par.dw_week_mode) { /* we want to start form week start day */
        diff_to_weeks_first_day = g_date_time_get_day_of_week (dw->start_date)
                                - (g_par.ical_weekstartday + 1);
        if (diff_to_weeks_first_day == 0) {
            /* we are on week start day */
            gdt = g_date_time_ref (dw->start_date);
        }
        else {
            gdt = g_date_time_add_days (dw->start_date,
                                        -1 * diff_to_weeks_first_day);
        }
    }
    else
        gdt = g_date_time_ref (dw->start_date);

    first_date = orage_gdatetime_to_i18_time (gdt, TRUE);
    gtk_button_set_label (GTK_BUTTON(dw->StartDate_button), first_date);
    g_object_set_data_full (G_OBJECT (dw->StartDate_button),
                            DATE_KEY, gdt, (GDestroyNotify)g_date_time_unref);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dw->day_spin), 7);

    g_signal_connect (dw->day_spin, "value-changed",
                      G_CALLBACK (on_spin_changed), dw);
    g_signal_connect (dw->StartDate_button, "clicked",
                      G_CALLBACK (on_Date_button_clicked_cb), dw);

    g_free (first_date);
}

static void fill_hour (OrageWeekWindow *dw,
                       const gint col, const gint row,
                       const gchar *text)
{
    GtkWidget *name, *ev;

    ev = gtk_event_box_new();
    name = gtk_label_new(text);
    g_object_set (name, "vexpand", TRUE, NULL);
    gtk_container_add (GTK_CONTAINER (ev), name);
    gtk_widget_set_name (ev, get_row_css_name (row));
    gtk_grid_attach (GTK_GRID(dw->dtable), ev, col, row + FIRST_HOUR_ROW, 1, 1);
}

static void fill_hour_arrow (OrageWeekWindow *dw, const gint col)
{
    GtkWidget *arrow, *ev;

    ev = gtk_event_box_new();

    if (g_par.dw_week_mode) {
        if (col == 0) {
            arrow = gtk_image_new_from_icon_name ("pan-up-symbolic",
                                                  GTK_ICON_SIZE_MENU);
            g_signal_connect((gpointer)ev, "button-press-event"
                    , G_CALLBACK(on_arrow_up_press_event_cb), dw);
        }
        else {
            arrow = gtk_image_new_from_icon_name ("pan-down-symbolic",
                                                  GTK_ICON_SIZE_MENU);
            g_signal_connect((gpointer)ev, "button-press-event"
                    , G_CALLBACK(on_arrow_down_press_event_cb), dw);
        }
    }
    else {
        if (col == 0) {
            arrow = gtk_image_new_from_icon_name ("pan-end-symbolic",
                                                  GTK_ICON_SIZE_MENU);
            g_signal_connect((gpointer)ev, "button-press-event"
                    , G_CALLBACK(on_arrow_left_press_event_cb), dw);
        }
        else {
            arrow = gtk_image_new_from_icon_name ("pan-start-symbolic",
                                                  GTK_ICON_SIZE_MENU);
            g_signal_connect((gpointer)ev, "button-press-event"
                    , G_CALLBACK(on_arrow_right_press_event_cb), dw);
        }
    }

    gtk_container_add(GTK_CONTAINER(ev), arrow);
    gtk_grid_attach (GTK_GRID (dw->dtable), ev, col, FULL_DAY_ROW, 1, 1);
}

static gboolean is_same_date (GDateTime *gdt0, GDateTime *gdt1)
{
    if (g_date_time_get_year (gdt0) != g_date_time_get_year (gdt1))
        return FALSE;
    else if ((g_date_time_get_day_of_year (gdt0) !=
              g_date_time_get_day_of_year (gdt1)))
    {
        return FALSE;
    }
    else
        return TRUE;
}

static GtkWidget *build_scroll_window (void)
{
    GtkWidget *scroll_win;

    scroll_win = gtk_scrolled_window_new (NULL, NULL);
    g_object_set (scroll_win, "expand", TRUE, NULL);

    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll_win),
                                         GTK_SHADOW_NONE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_win),
                                    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (scroll_win),
                                       GTK_CORNER_TOP_LEFT);

    return scroll_win;
}

/* row 1= day header buttons
 * row 2= full day events after the buttons */
static void build_day_view_table (OrageWeekWindow *dw)
{
    gint days;   /* number of days to show */
    gint days_n1;
    gint i, sunday;
    GtkWidget *label, *button;
    char text[5+1];
    gchar *date;
    GDateTime *gdt0;
    GDateTime *gdt_today;

    (void)orage_category_get_list (); /* Uses side effects. */
    days = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dw->day_spin));
    days_n1 = days + 1;
    gdt0 = g_date_time_ref (
            g_object_get_data (G_OBJECT (dw->StartDate_button), DATE_KEY));

    /****** header of day table = days columns ******/
    dw->scroll_win = build_scroll_window ();

    gtk_grid_attach (GTK_GRID (dw->Vbox), dw->scroll_win, 0, 3, 1, 1);

    dw->dtable = gtk_grid_new ();

    sunday = g_date_time_get_day_of_week (gdt0);
    if (sunday) /* index to next sunday */
        sunday = 7-sunday;

    gdt_today = g_date_time_new_now_local ();
    fill_hour_arrow(dw, 0);
    for (i = 1; i < days_n1; i++)
    {
        date = orage_gdatetime_to_i18_time (gdt0, TRUE);
        button = gtk_button_new_with_label (date);
        g_object_set_data_full (G_OBJECT (button), DATE_KEY, gdt0,
                                (GDestroyNotify)g_date_time_unref);
        g_free (date);

        if (is_same_date (gdt_today, gdt0))
            gtk_widget_set_name (button, ORAGE_WEEK_WINDOW_TODAY);

        if ((i - 1) % 7 == sunday)
        {
            label = gtk_bin_get_child (GTK_BIN(button));
            gtk_widget_set_name (label, ORAGE_WEEK_WINDOW_SUNDAY);
        }

        g_signal_connect (button, "clicked",
                          G_CALLBACK (header_button_clicked_cb), dw);
        g_object_set (button, "margin-left", 3, NULL);
        gtk_grid_attach (GTK_GRID (dw->dtable), button, i, BUTTON_ROW, 1, 1);

        /* gdt0 can be safely reassigned, current value is unrefrenced by
         * button.
         */
        gdt0 = g_date_time_add_days (gdt0, 1);
    }

    fill_hour_arrow (dw, days_n1);
    g_date_time_unref (gdt_today);
    g_date_time_unref (gdt0);

    /* GtkGrid does not implement GtkScrollable, but 'gtk_container_add'
     * intelligenty accounts it and wraps it GtkScrollable.
     */
    gtk_container_add (GTK_CONTAINER(dw->scroll_win), dw->dtable);

    /* hours column = hour rows */
    for (i = 0; i < 24; i++) {
        (void)g_snprintf (text, sizeof (text), "%02d", i);
        /* ev is needed for background colour */
        fill_hour(dw, 0, i, text);
        fill_hour (dw, days_n1, i, text);
    }
    fill_days(dw, days);
}

void orage_week_window_refresh (OrageWeekWindow *dw)
{
    get_scroll_position(dw);
    gtk_widget_destroy(dw->scroll_win);
    build_day_view_table(dw);
    gtk_widget_show_all(dw->scroll_win);
    /* I was not able to get this work without the timer. Ugly yes, but
     * it works and does not hurt - much */
    g_timeout_add(100, (GSourceFunc)scroll_position_timer, (gpointer)dw);
}

static void orage_week_window_constructed (GObject *object)
{
    OrageWeekWindow *self = (OrageWeekWindow *)object;

    if (g_par.dw_size_x || g_par.dw_size_y)
    {
        gtk_window_set_default_size (GTK_WINDOW (self),
                                     g_par.dw_size_x, g_par.dw_size_y);
    }

    if (g_par.dw_pos_x || g_par.dw_pos_y)
        gtk_window_move (GTK_WINDOW (self), g_par.dw_pos_x, g_par.dw_pos_y);

    build_menu (self);
    build_toolbar (self);
    build_day_view_header (self);
    build_day_view_table (self);
    gtk_widget_show_all (GTK_WIDGET (self));
    set_scroll_position (self);

    G_OBJECT_CLASS (orage_week_window_parent_class)->constructed (object);
}

static void orage_week_window_finalize (GObject *object)
{
    OrageWeekWindow *self = (OrageWeekWindow *)object;

    g_list_free (self->apptw_list);
    g_date_time_unref (self->a_day);
    g_date_time_unref (self->start_date);

    G_OBJECT_CLASS (orage_week_window_parent_class)->finalize (object);
}

static void orage_week_window_get_property (GObject *object,
                                            const guint prop_id, GValue *value,
                                            GParamSpec *pspec)
{
    const OrageWeekWindow *self = ORAGE_WEEK_WINDOW (object);

    switch (prop_id)
    {
        case PROP_START_DATE:
            g_value_set_boxed (value, self->start_date);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void orage_week_window_set_property (GObject *object,
                                            const guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
    OrageWeekWindow *self = ORAGE_WEEK_WINDOW (object);

    switch (prop_id)
    {
        case PROP_START_DATE:
            g_assert (self->start_date == NULL);
            self->start_date = (GDateTime *)g_value_dup_boxed (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void orage_week_window_class_init (OrageWeekWindowClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->constructed = orage_week_window_constructed;
    object_class->finalize = orage_week_window_finalize;
    object_class->get_property = orage_week_window_get_property;
    object_class->set_property = orage_week_window_set_property;

    properties[PROP_START_DATE] =
            g_param_spec_boxed (ORAGE_WEEK_WINDOW_START_DATE_PROPERTY,
                                ORAGE_WEEK_WINDOW_START_DATE_PROPERTY,
                                "An GDateTime for week window start date",
                                G_TYPE_DATE_TIME,
                                G_PARAM_STATIC_STRINGS |
                                G_PARAM_READWRITE |
                                G_PARAM_CONSTRUCT_ONLY |
                                G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void orage_week_window_init (OrageWeekWindow *self)
{
    self->scroll_pos = -1; /* not set */
    self->accel_group = gtk_accel_group_new ();
    self->a_day = g_date_time_new_now_local ();
    self->Vbox = gtk_grid_new ();

    gtk_widget_set_name (GTK_WIDGET (self), "orage-week-window");
    gtk_window_set_title (GTK_WINDOW (self), _("Orage - day view"));
    gtk_window_add_accel_group (GTK_WINDOW (self), self->accel_group);
    gtk_container_add (GTK_CONTAINER (self), self->Vbox);
}

OrageWeekWindow *orage_week_window_new (GDateTime *date)
{
    OrageWeekWindow *window;

    window = g_object_new (ORAGE_WEEK_WINDOW_TYPE,
                           "type", GTK_WINDOW_TOPLEVEL,
                           ORAGE_WEEK_WINDOW_START_DATE_PROPERTY, date,
                           NULL);

    return window;
}

void orage_week_window_build (GDateTime *date)
{
    OrageWeekWindow *window;

    window = orage_week_window_new (date);

    g_signal_connect (window, "delete_event",
                      G_CALLBACK (on_Window_delete_event), window);
}

void orage_week_window_remove_appointment_window (OrageWeekWindow *self,
                                                  gconstpointer apptw)
{
    self->apptw_list = g_list_remove (self->apptw_list, apptw);
}
