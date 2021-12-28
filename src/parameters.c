/*      Orage - Calendar and alarm handler
 *
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
       Free Software Foundation
       51 Franklin Street, 5th Floor
       Boston, MA 02110-1301 USA

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>
#include <locale.h>

#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
#include <langinfo.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "orage-i18n.h"
#include "functions.h"
#include "tray_icon.h"
#include "ical-code.h"
#include "timezone_selection.h"
#include "parameters.h"
#include "parameters_internal.h"
#include "mainbox.h"

gboolean check_wakeup(gpointer user_data); /* in main.c*/

static gboolean is_running = FALSE;

/* Return the first day of the week, where 0=monday, 6=sunday.
 *     Borrowed from GTK+:s Calendar Widget, but modified
 *     to return 0..6 mon..sun, which is what libical uses */
static int get_first_weekday_from_locale(void)
{
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
    union { unsigned int word; char *string; } langinfo;
    int week_1stday = 0;
    int first_weekday = 1;
    unsigned int week_origin;

    setlocale(LC_TIME, "");
    langinfo.string = nl_langinfo(_NL_TIME_FIRST_WEEKDAY);
    first_weekday = langinfo.string[0];
    langinfo.string = nl_langinfo(_NL_TIME_WEEK_1STDAY);
    week_origin = langinfo.word;
    if (week_origin == 19971130) /* Sunday */
        week_1stday = 0;
    else if (week_origin == 19971201) /* Monday */
        week_1stday = 1;
    else
        g_warning ("unknown value of _NL_TIME_WEEK_1STDAY.");

    return((week_1stday + first_weekday - 2 + 7) % 7);
#else
    g_warning ("Can not find first weekday. Using default: Monday=0. If this "
               "is wrong guess. please set undocumented parameter: "
               "Ical week start day (Sunday=6)");
    return(0);
#endif
}

/* 0 = monday, ..., 6 = sunday */
static gint get_first_weekday (OrageRc *orc)
{
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
#if 0
    /* Original code. */
    return orage_rc_get_int (orc, "Ical week start day",
                             get_first_weekday_from_locale ());
#else
    /* TODO: This should be valid code for Linux, check how this part work on
     * Linux.
     */
    const gint first_week_day = orage_rc_get_int(orc, "Ical week start day",-1);

    return (first_week_day != -1) ? first_week_day
                                  : get_first_weekday_from_locale ();
#endif
#else
    const gint first_week_day = orage_rc_get_int(orc, "Ical week start day",-1);

    return (first_week_day == -1) ? get_first_weekday_from_locale ()
                                  : first_week_day;
#endif
}

static void dialog_response(GtkWidget *dialog, gint response_id
        , gpointer user_data)
{
    Itf *itf = (Itf *)user_data;
    gchar *helpdoc;
    GError *error = NULL;

    if (response_id == GTK_RESPONSE_HELP) {
        /* Needs to be in " to keep # */
        helpdoc = g_strconcat("exo-open \"file://", PACKAGE_DATA_DIR
                , G_DIR_SEPARATOR_S, "orage"
                , G_DIR_SEPARATOR_S, "doc"
                , G_DIR_SEPARATOR_S, "C"
                , G_DIR_SEPARATOR_S, "orage.html#orage-preferences-window\""
                , NULL);
        if (!orage_exec(helpdoc, FALSE, &error)) {
            g_message ("%s failed: %s. Trying firefox", helpdoc
                    , error->message);
            g_clear_error(&error);
            g_free(helpdoc);
            helpdoc = g_strconcat("firefox \"file://", PACKAGE_DATA_DIR
                    , G_DIR_SEPARATOR_S, "orage"
                    , G_DIR_SEPARATOR_S, "doc"
                    , G_DIR_SEPARATOR_S, "C"
                    , G_DIR_SEPARATOR_S, "orage.html#orage-preferences-window\""
                    , NULL);
            if (!orage_exec(helpdoc, FALSE, &error)) {
                g_warning ("start of %s failed: %s", helpdoc, error->message);
                g_clear_error(&error);
            }
        }
        g_free(helpdoc);
    }
    else { /* delete signal or close response */
        write_parameters();
        is_running = FALSE;
        gtk_widget_destroy(dialog);
        g_free(itf);
    }
}

static void sound_application_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;
    
    if (g_par.sound_application)
        g_free(g_par.sound_application);
    g_par.sound_application = g_strdup(gtk_entry_get_text(
            GTK_ENTRY(itf->sound_application_entry)));

    (void)dialog;
}

static void set_border(void)
{
    gtk_window_set_decorated(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow)
            , g_par.show_borders);
}

static void borders_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_borders = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_borders_checkbutton));
    set_border();

    (void)dialog;
}

static void set_menu(void)
{
    if (g_par.show_menu)
        gtk_widget_show(((CalWin *)g_par.xfcal)->mMenubar);
    else
        gtk_widget_hide(((CalWin *)g_par.xfcal)->mMenubar);
}

static void menu_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_menu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_menu_checkbutton));
    set_menu();

    (void)dialog;
}

static void set_calendar(void)
{
    gtk_calendar_set_display_options(
            GTK_CALENDAR(((CalWin *)g_par.xfcal)->mCalendar)
                    , (g_par.show_heading ? GTK_CALENDAR_SHOW_HEADING : 0)
                    | (g_par.show_day_names ? GTK_CALENDAR_SHOW_DAY_NAMES : 0)
                    | (g_par.show_weeks ? GTK_CALENDAR_SHOW_WEEK_NUMBERS : 0));
}

static void heading_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_heading = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_heading_checkbutton));
    set_calendar();

    (void)dialog;
}

static void days_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_day_names = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_day_names_checkbutton));
    set_calendar();

    (void)dialog;
}

static void weeks_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_weeks = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_weeks_checkbutton));
    set_calendar();

    (void)dialog;
}

static void todos_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_todos = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_todos_checkbutton));
    if (g_par.show_todos)
        build_mainbox_todo_box();
    else {
        gtk_widget_hide(((CalWin *)g_par.xfcal)->mTodo_vbox);
        /* hide the whole area if also event box does not exist */
        if (!g_par.show_event_days)
            gtk_window_resize(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow)
                    , g_par.size_x, 1);
    }

    (void)dialog;
}

static void show_events_spin_changed(GtkSpinButton *sb, gpointer user_data)
{
    g_par.show_event_days = gtk_spin_button_get_value(sb);
    if (g_par.show_event_days)
        build_mainbox_event_box();
    else {
        gtk_widget_hide(((CalWin *)g_par.xfcal)->mEvent_vbox);
        /* hide the whole area if also todo box does not exist */
        if (!g_par.show_todos)
            gtk_window_resize(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow)
                    , g_par.size_x, 1);
    }

    (void)user_data;
}

static void set_stick(void)
{
    if (g_par.set_stick)
        gtk_window_stick(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow));
    else
        gtk_window_unstick(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow));
}

static void stick_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.set_stick = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->set_stick_checkbutton));
    set_stick();

    (void)dialog;
}

static void set_ontop(void)
{
    gtk_window_set_keep_above(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow)
            , g_par.set_ontop);
}

static void ontop_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.set_ontop = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->set_ontop_checkbutton));
    set_ontop();

    (void)dialog;
}

static void set_taskbar(void)
{
    gtk_window_set_skip_taskbar_hint(
            GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow), !g_par.show_taskbar);
}

static void taskbar_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_taskbar = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_taskbar_checkbutton));
    set_taskbar();

    (void)dialog;
}

static void set_pager(void)
{
    gtk_window_set_skip_pager_hint(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow)
            , !g_par.show_pager);
}

static void pager_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_pager = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_pager_checkbutton));
    set_pager();

    (void)dialog;
}

static void set_systray(void)
{
    GdkPixbuf *orage_logo;
    GtkStatusIcon *status_icon = (GtkStatusIcon *)g_par.trayIcon;

    if (!(status_icon && orage_status_icon_is_embedded (status_icon)))
    {
        orage_logo = orage_create_icon(FALSE, 0);
        status_icon = create_TrayIcon (orage_logo);
        g_par.trayIcon = status_icon;
        g_object_unref(orage_logo);
    }

    orage_status_icon_set_visible (status_icon, g_par.show_systray);
}

static void systray_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_systray = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_systray_checkbutton));
    set_systray();

    (void)dialog;
}

static void start_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.start_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->visibility_show_radiobutton));
    g_par.start_minimized = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->visibility_minimized_radiobutton));

    (void)dialog;
}

static void show_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_days = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->click_to_show_days_radiobutton));

    (void)dialog;
}

static void foreign_alarm_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.use_foreign_display_alarm_notify = 
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->foreign_alarm_notification_radiobutton));

    (void)dialog;
}

static void dw_week_mode_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.dw_week_mode = 
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->dw_week_mode_week_radiobutton));

    (void)dialog;
}

static void sound_application_open_button_clicked(GtkButton *button
        , gpointer user_data)
{
    Itf *itf = (Itf *)user_data;
    GtkWidget *file_chooser;
    GtkWidget *open_button;
    gchar *s; /* to avoid timing problems when updating entry */

    /* Create file chooser */
    file_chooser = gtk_file_chooser_dialog_new(_("Select a file...")
            , GTK_WINDOW(itf->orage_dialog), GTK_FILE_CHOOSER_ACTION_OPEN
            , "_Cancel", GTK_RESPONSE_CANCEL
            , NULL);

    open_button = orage_util_image_button ("document-open", _("_Open"));
    gtk_widget_set_can_default (open_button, TRUE);
    gtk_dialog_add_action_widget (GTK_DIALOG (file_chooser), open_button,
                                  GTK_RESPONSE_ACCEPT);

    /* Set sound application search path */
    if (g_par.sound_application == NULL || strlen(g_par.sound_application) == 0
    ||  g_par.sound_application[0] != '/'
    || ! gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_chooser)
                , g_par.sound_application))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser)
                , "/");

   if (gtk_dialog_run(GTK_DIALOG(file_chooser)) == GTK_RESPONSE_ACCEPT) {
           g_par.sound_application = gtk_file_chooser_get_filename(
                GTK_FILE_CHOOSER(file_chooser));
        if (g_par.sound_application) {
            s = g_strdup(g_par.sound_application);
            gtk_entry_set_text(GTK_ENTRY(itf->sound_application_entry)
                    , (const gchar *)s);
            g_free(s);
        }
    }
    gtk_widget_destroy(file_chooser);

    (void)button;
}

static void timezone_button_clicked(GtkButton *button, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    if (!ORAGE_STR_EXISTS(g_par.local_timezone)) {
        g_warning("timezone pressed: local timezone missing");
        g_par.local_timezone = g_strdup("UTC");
    }
    if (orage_timezone_button_clicked(button, GTK_WINDOW(itf->orage_dialog)
            , &g_par.local_timezone, TRUE, g_par.local_timezone))
        xfical_set_local_timezone(FALSE);
}

#ifdef HAVE_ARCHIVE
static void archive_threshold_spin_changed(GtkSpinButton *sb
        , gpointer user_data)
{
    g_par.archive_limit = gtk_spin_button_get_value(sb);

    (void)user_data;
}
#endif

static void select_day_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.select_always_today = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(itf->select_day_today_radiobutton));

    (void)dialog;
}

static void el_extra_days_spin_changed(GtkSpinButton *sb, gpointer user_data)
{
    g_par.el_days = gtk_spin_button_get_value(sb);

    (void)user_data;
}

static void el_only_first_checkbutton_clicked(GtkCheckButton *cb
        , gpointer user_data)
{
    g_par.el_only_first = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));

    (void)user_data;
}

/* start monitoring lost seconds due to hibernate or suspend */
static void set_wakeup_timer(void)
{
    if (g_par.wakeup_timer) { /* need to stop it if running */
        g_source_remove(g_par.wakeup_timer);
        g_par.wakeup_timer=0;
    }
    if (g_par.use_wakeup_timer) {
        check_wakeup(&g_par); /* init */
        g_par.wakeup_timer = 
                g_timeout_add_seconds(ORAGE_WAKEUP_TIMER_PERIOD
                        , (GSourceFunc)check_wakeup, NULL);
    }
}

static void use_wakeup_timer_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.use_wakeup_timer = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(itf->use_wakeup_timer_checkbutton));
    set_wakeup_timer();

    (void)dialog;
}

static void always_quit_changed(GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.close_means_quit = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(itf->always_quit_checkbutton));

    (void)dialog;
}

static void create_parameter_dialog_main_setup_tab(Itf *dialog)
{
    GtkWidget *hbox, *vbox, *label;

    dialog->setup_vbox = gtk_grid_new ();
    dialog->setup_tab = orage_create_framebox_with_content (
            NULL, GTK_SHADOW_NONE, dialog->setup_vbox);
    /* orage_create_framebox_with_content set 'margin-top' to 5, so we have to
     * change top margin AFTER calling orage_create_framebox_with_content.
     */
    g_object_set (dialog->setup_vbox, "row-spacing", 10,
                                      "margin-top", 10,
                                      NULL);
    dialog->setup_tab_label = gtk_label_new(_("Main settings"));
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook)
          , dialog->setup_tab, dialog->setup_tab_label);

    /* Choose a timezone to be used in appointments */
    vbox = gtk_grid_new ();
    dialog->timezone_frame = orage_create_framebox_with_content (_("Timezone")
            , GTK_SHADOW_NONE, vbox);
    gtk_grid_attach_next_to (GTK_GRID (dialog->setup_vbox),
                             dialog->timezone_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->timezone_button = gtk_button_new();
    if (!ORAGE_STR_EXISTS(g_par.local_timezone)) {
        g_warning("parameters: local timezone missing");
        g_par.local_timezone = g_strdup("UTC");
    }
    gtk_button_set_label(GTK_BUTTON(dialog->timezone_button)
            , _(g_par.local_timezone));
    g_object_set (dialog->timezone_button,
                  "margin-top", 5, "margin-bottom", 5, "margin-left", 5,
                  "hexpand", TRUE,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (vbox),
                             dialog->timezone_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(dialog->timezone_button
            , _("You should always define your local timezone."));
    g_signal_connect(G_OBJECT(dialog->timezone_button), "clicked"
            , G_CALLBACK(timezone_button_clicked), dialog);

#ifdef HAVE_ARCHIVE
    /* Choose archiving threshold */
    hbox = gtk_grid_new ();
    dialog->archive_threshold_frame = 
            orage_create_framebox_with_content(_("Archive threshold (months)")
                    , GTK_SHADOW_NONE, hbox);
    gtk_grid_attach_next_to (GTK_GRID (dialog->setup_vbox),
                             dialog->archive_threshold_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    dialog->archive_threshold_spin = gtk_spin_button_new_with_range(0, 12, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->archive_threshold_spin)
            , g_par.archive_limit);
    g_object_set (dialog->archive_threshold_spin, "margin-left", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->archive_threshold_spin,
                             NULL, GTK_POS_RIGHT, 1, 1);
    label = gtk_label_new(_("(0 = no archiving)"));
    g_object_set (label, "margin-left", 10, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(dialog->archive_threshold_spin
            , _("Archiving is used to save time and space when handling events."));
    g_signal_connect(G_OBJECT(dialog->archive_threshold_spin), "value-changed"
            , G_CALLBACK(archive_threshold_spin_changed), dialog);
#endif

    /* Choose a sound application for reminders */
    hbox = gtk_grid_new ();
    dialog->sound_application_frame = 
            orage_create_framebox_with_content(_("Sound command"),
                                               GTK_SHADOW_NONE, hbox);
    gtk_grid_attach_next_to (GTK_GRID (dialog->setup_vbox),
                             dialog->sound_application_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    dialog->sound_application_entry = gtk_entry_new();
    g_object_set (dialog->sound_application_entry,
                  "margin-left", 5,
                  "hexpand", TRUE,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), dialog->sound_application_entry,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_entry_set_text(GTK_ENTRY(dialog->sound_application_entry)
            , (const gchar *)g_par.sound_application);

    dialog->sound_application_open_button = 
            orage_util_image_button ("document-open", _("_Open"));

    g_object_set (dialog->sound_application_open_button,
                  "margin-left", 10,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->sound_application_open_button,
                             NULL, GTK_POS_RIGHT, 1, 1);

    gtk_widget_set_tooltip_text(dialog->sound_application_entry
            , _("This command is given to shell to make sound in alarms."));
    g_signal_connect(G_OBJECT(dialog->sound_application_open_button), "clicked"
            , G_CALLBACK(sound_application_open_button_clicked), dialog);
    g_signal_connect(G_OBJECT(dialog->sound_application_entry), "changed"
            , G_CALLBACK(sound_application_changed), dialog);
}

static void table_add_row(GtkWidget *table, GtkWidget *data1, GtkWidget *data2,
                          const guint row)
{
    if (data1)
        gtk_grid_attach (GTK_GRID (table), data1, 0, row, 1, 1);

    if (data2)
        gtk_grid_attach (GTK_GRID (table), data2, 1, row, 1, 1);
}

static void create_parameter_dialog_calendar_setup_tab(Itf *dialog)
{
    GtkWidget *hbox, *vbox, *label, *table;
    gint row;

    dialog->calendar_vbox = gtk_grid_new ();
    dialog->calendar_tab = 
        orage_create_framebox_with_content (NULL,
                           GTK_SHADOW_NONE, dialog->calendar_vbox);
    /* orage_create_framebox_with_content set 'margin-top' to 5, so we have to
     * change top margin AFTER calling orage_create_framebox_with_content.
     */
    g_object_set (dialog->calendar_vbox, "row-spacing", 10,
                                         "margin-top", 10, NULL);
    dialog->calendar_tab_label = gtk_label_new(_("Calendar window"));
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook)
          , dialog->calendar_tab, dialog->calendar_tab_label);

    /***** calendar borders and menu and other calendar visual options *****/
    table = gtk_grid_new ();
    dialog->mode_frame = 
            orage_create_framebox_with_content(_("Calendar visual details")
                    , GTK_SHADOW_NONE, table);
    gtk_grid_attach_next_to (GTK_GRID (dialog->calendar_vbox),
                             dialog->mode_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->show_borders_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show borders"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_borders_checkbutton), g_par.show_borders);

    dialog->show_menu_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show menu"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_menu_checkbutton), g_par.show_menu);

    table_add_row(table, dialog->show_borders_checkbutton
            , dialog->show_menu_checkbutton, row = 0);

    dialog->show_day_names_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show day names"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_day_names_checkbutton), g_par.show_day_names);

    dialog->show_weeks_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show week numbers"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_weeks_checkbutton), g_par.show_weeks);

    table_add_row(table, dialog->show_day_names_checkbutton
            , dialog->show_weeks_checkbutton, ++row);

    dialog->show_heading_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show month and year"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_heading_checkbutton), g_par.show_heading);

    table_add_row(table, dialog->show_heading_checkbutton, NULL, ++row);

    g_signal_connect(G_OBJECT(dialog->show_borders_checkbutton), "toggled"
            , G_CALLBACK(borders_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_menu_checkbutton), "toggled"
            , G_CALLBACK(menu_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_heading_checkbutton), "toggled"
            , G_CALLBACK(heading_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_day_names_checkbutton), "toggled"
            , G_CALLBACK(days_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_weeks_checkbutton), "toggled"
            , G_CALLBACK(weeks_changed), dialog);

    /***** calendar info boxes (under the calendar) *****/
    vbox = gtk_grid_new ();
    dialog->info_frame = 
            orage_create_framebox_with_content(_("Calendar info boxes"),
                                               GTK_SHADOW_NONE, vbox);
    gtk_grid_attach_next_to (GTK_GRID (dialog->calendar_vbox),
                             dialog->info_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->show_todos_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show todo list"));
    gtk_grid_attach_next_to (GTK_GRID (vbox), dialog->show_todos_checkbutton,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_todos_checkbutton), g_par.show_todos);

    hbox = gtk_grid_new ();
    label = gtk_label_new(_("Number of days to show in event window"));
    g_object_set (label, "margin-left", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    dialog->show_events_spin = gtk_spin_button_new_with_range(0, 31, 1);
    g_object_set (dialog->show_events_spin, "margin-left", 10, NULL);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->show_events_spin)
            , g_par.show_event_days);
    gtk_widget_set_tooltip_text(dialog->show_events_spin
            , _("0 = do not show event list at all"));
    gtk_grid_attach_next_to (GTK_GRID (hbox), dialog->show_events_spin, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect(G_OBJECT(dialog->show_todos_checkbutton), "toggled"
            , G_CALLBACK(todos_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_events_spin), "value-changed"
            , G_CALLBACK(show_events_spin_changed), dialog);

    /***** Where calendar appears = exists = is visible *****/
    table = gtk_grid_new ();
    dialog->appearance_frame = 
            orage_create_framebox_with_content (
            _("Calendar visibility"), GTK_SHADOW_NONE, table);
    gtk_grid_attach_next_to (GTK_GRID (dialog->calendar_vbox),
                             dialog->appearance_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->set_stick_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show on all desktops"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->set_stick_checkbutton), g_par.set_stick);

    dialog->set_ontop_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Keep on top"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->set_ontop_checkbutton), g_par.set_ontop);

    table_add_row(table, dialog->set_stick_checkbutton
            , dialog->set_ontop_checkbutton, row = 0);

    dialog->show_taskbar_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show in taskbar"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_taskbar_checkbutton), g_par.show_taskbar);

    dialog->show_pager_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show in pager"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_pager_checkbutton), g_par.show_pager);

    table_add_row(table, dialog->show_taskbar_checkbutton
            , dialog->show_pager_checkbutton, ++row);

    dialog->show_systray_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show in systray"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_systray_checkbutton), g_par.show_systray);

    table_add_row(table, dialog->show_systray_checkbutton, NULL, ++row);

    g_signal_connect(G_OBJECT(dialog->set_stick_checkbutton), "toggled"
            , G_CALLBACK(stick_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->set_ontop_checkbutton), "toggled"
            , G_CALLBACK(ontop_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_taskbar_checkbutton), "toggled"
            , G_CALLBACK(taskbar_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_pager_checkbutton), "toggled"
            , G_CALLBACK(pager_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_systray_checkbutton), "toggled"
            , G_CALLBACK(systray_changed), dialog);

    /***** how to show when started (show/hide/minimize) *****/
    dialog->visibility_radiobutton_group = NULL;
    hbox = gtk_grid_new ();
    g_object_set (hbox, "column-homogeneous", TRUE, NULL);
    dialog->visibility_frame = orage_create_framebox_with_content(
            _("Calendar start") , GTK_SHADOW_NONE, hbox);
    gtk_grid_attach_next_to (GTK_GRID (dialog->calendar_vbox),
                             dialog->visibility_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->visibility_show_radiobutton = gtk_radio_button_new_with_mnemonic(
            NULL, _("Show"));
    g_object_set (dialog->visibility_show_radiobutton,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->visibility_show_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->visibility_show_radiobutton)
            , dialog->visibility_radiobutton_group);
    dialog->visibility_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->visibility_show_radiobutton));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->visibility_show_radiobutton), g_par.start_visible);

    dialog->visibility_hide_radiobutton = gtk_radio_button_new_with_mnemonic(
            NULL, _("Hide"));
    g_object_set (dialog->visibility_hide_radiobutton,
                  "hexpand", TRUE,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->visibility_hide_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->visibility_hide_radiobutton)
            , dialog->visibility_radiobutton_group);
    dialog->visibility_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->visibility_hide_radiobutton));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->visibility_hide_radiobutton), !g_par.start_visible);

    dialog->visibility_minimized_radiobutton = 
            gtk_radio_button_new_with_mnemonic(NULL, _("Minimized"));
    g_object_set (dialog->visibility_minimized_radiobutton,
                  "hexpand", TRUE,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->visibility_minimized_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->visibility_minimized_radiobutton)
            , dialog->visibility_radiobutton_group);
    dialog->visibility_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->visibility_minimized_radiobutton));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->visibility_minimized_radiobutton), g_par.start_minimized);

    g_signal_connect(G_OBJECT(dialog->visibility_show_radiobutton), "toggled"
            , G_CALLBACK(start_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->visibility_minimized_radiobutton)
            , "toggled", G_CALLBACK(start_changed), dialog);

    /****** On Calendar Window Open ******/
    dialog->select_day_radiobutton_group = NULL;
    hbox = gtk_grid_new ();
    g_object_set (hbox, "column-homogeneous", TRUE, NULL);
    dialog->select_day_frame = orage_create_framebox_with_content(
            _("On calendar window open"), GTK_SHADOW_NONE, hbox);
    gtk_grid_attach_next_to (GTK_GRID (dialog->calendar_vbox),
                             dialog->select_day_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->select_day_today_radiobutton =
            gtk_radio_button_new_with_mnemonic(NULL, _("Select today's date"));
    g_object_set (dialog->select_day_today_radiobutton,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->select_day_today_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->select_day_today_radiobutton)
            , dialog->select_day_radiobutton_group);
    dialog->select_day_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->select_day_today_radiobutton));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->select_day_today_radiobutton), g_par.select_always_today);

    dialog->select_day_old_radiobutton =
            gtk_radio_button_new_with_mnemonic(NULL
                    , _("Select previously selected date"));
    g_object_set (dialog->select_day_old_radiobutton,
                  "hexpand", TRUE,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->select_day_old_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->select_day_old_radiobutton)
            , dialog->select_day_radiobutton_group);
    dialog->select_day_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->select_day_old_radiobutton));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->select_day_old_radiobutton), !g_par.select_always_today);

    g_signal_connect(G_OBJECT(dialog->select_day_today_radiobutton), "toggled"
            , G_CALLBACK(select_day_changed), dialog);

    /***** Start event list or day window from main calendar *****/
    dialog->click_to_show_radiobutton_group = NULL;
    hbox = gtk_grid_new ();
    g_object_set (hbox, "column-homogeneous", TRUE, NULL);
    dialog->click_to_show_frame = orage_create_framebox_with_content(
            _("Calendar day double click shows"), GTK_SHADOW_NONE, hbox);
    gtk_grid_attach_next_to (GTK_GRID (dialog->calendar_vbox),
                             dialog->click_to_show_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->click_to_show_days_radiobutton =
            gtk_radio_button_new_with_mnemonic(NULL, _("Days view"));
    g_object_set (dialog->click_to_show_days_radiobutton,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->click_to_show_days_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->click_to_show_days_radiobutton)
            , dialog->click_to_show_radiobutton_group);
    dialog->click_to_show_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->click_to_show_days_radiobutton));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->click_to_show_days_radiobutton), g_par.show_days);

    dialog->click_to_show_events_radiobutton =
            gtk_radio_button_new_with_mnemonic(NULL, _("Event list"));
    g_object_set (dialog->click_to_show_events_radiobutton,
                  "hexpand", TRUE,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->click_to_show_events_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->click_to_show_events_radiobutton)
            , dialog->click_to_show_radiobutton_group);
    dialog->click_to_show_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->click_to_show_events_radiobutton));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->click_to_show_events_radiobutton), !g_par.show_days);

    g_signal_connect(G_OBJECT(dialog->click_to_show_days_radiobutton), "toggled"
            , G_CALLBACK(show_changed), dialog);
}

static void create_parameter_dialog_extra_setup_tab(Itf *dialog)
{
    GtkWidget *hbox, *vbox, *label;

    dialog->extra_vbox = gtk_grid_new ();
    dialog->extra_tab = 
            orage_create_framebox_with_content (
            NULL, GTK_SHADOW_NONE, dialog->extra_vbox);
    /* orage_create_framebox_with_content set 'margin-top' to 5, so we have to
     * change top margin AFTER calling orage_create_framebox_with_content.
     */
    g_object_set (dialog->extra_vbox, "row-spacing", 10,
                                      "margin-top", 10, NULL);
    dialog->extra_tab_label = gtk_label_new(_("Extra settings"));
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook)
          , dialog->extra_tab, dialog->extra_tab_label);

    /***** Eventlist window extra days and only first *****/
    vbox = gtk_grid_new ();
    dialog->el_extra_days_frame = orage_create_framebox_with_content(
            _("Event list window"), GTK_SHADOW_NONE, vbox);

    gtk_grid_attach_next_to (GTK_GRID (dialog->extra_vbox),
                             dialog->el_extra_days_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    hbox = gtk_grid_new ();
    label = gtk_label_new(_("Number of extra days to show in event list"));
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    dialog->el_extra_days_spin = gtk_spin_button_new_with_range(0, 99999, 1);
    g_object_set (dialog->el_extra_days_spin, "margin-left", 10, NULL);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(dialog->el_extra_days_spin), TRUE);
    gtk_grid_attach_next_to (GTK_GRID (hbox), dialog->el_extra_days_spin, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->el_extra_days_spin)
            , g_par.el_days);
    gtk_widget_set_tooltip_text(dialog->el_extra_days_spin
            , _("This is just the default value, you can change it in the actual eventlist window."));
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect(G_OBJECT(dialog->el_extra_days_spin), "value-changed"
            , G_CALLBACK(el_extra_days_spin_changed), dialog);

    hbox = gtk_grid_new ();
    dialog->el_only_first_checkbutton = gtk_check_button_new_with_label(
            _("Show only first repeating event"));
    gtk_grid_attach_next_to (GTK_GRID (hbox), dialog->el_only_first_checkbutton,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(dialog->el_only_first_checkbutton)
                    , g_par.el_only_first);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect(G_OBJECT(dialog->el_only_first_checkbutton), "clicked"
            , G_CALLBACK(el_only_first_checkbutton_clicked), dialog);


    /***** Default start day in day view window *****/
    dialog->dw_week_mode_radiobutton_group = NULL;
    hbox = gtk_grid_new ();
    g_object_set (hbox, "column-homogeneous", TRUE, NULL);
    dialog->dw_week_mode_frame = orage_create_framebox_with_content(
            _("Day view window default first day"), GTK_SHADOW_NONE, hbox);

    gtk_grid_attach_next_to (GTK_GRID (dialog->extra_vbox),
                             dialog->dw_week_mode_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->dw_week_mode_week_radiobutton =
            gtk_radio_button_new_with_mnemonic(NULL, _("First day of week"));
    g_object_set (dialog->dw_week_mode_week_radiobutton,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->dw_week_mode_week_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->dw_week_mode_week_radiobutton)
            , dialog->dw_week_mode_radiobutton_group);
    dialog->dw_week_mode_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->dw_week_mode_week_radiobutton));
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(dialog->dw_week_mode_week_radiobutton)
            , g_par.dw_week_mode);

    dialog->dw_week_mode_day_radiobutton =
            gtk_radio_button_new_with_mnemonic(NULL, _("Selected day"));
    g_object_set (dialog->dw_week_mode_day_radiobutton,
                  "hexpand", TRUE,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->dw_week_mode_day_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->dw_week_mode_day_radiobutton)
            , dialog->dw_week_mode_radiobutton_group);
    dialog->dw_week_mode_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->dw_week_mode_day_radiobutton));
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(dialog->dw_week_mode_day_radiobutton)
            , !g_par.dw_week_mode);

    g_signal_connect(G_OBJECT(dialog->dw_week_mode_day_radiobutton)
            , "toggled", G_CALLBACK(dw_week_mode_changed), dialog);

    /***** use wakeup timer *****/
    hbox = gtk_grid_new ();
    dialog->use_wakeup_timer_frame = orage_create_framebox_with_content(
            _("Use wakeup timer"), GTK_SHADOW_NONE, hbox);

    gtk_grid_attach_next_to (GTK_GRID (dialog->extra_vbox),
                             dialog->use_wakeup_timer_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    dialog->use_wakeup_timer_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("Use wakeup timer"));
    g_object_set (dialog->use_wakeup_timer_checkbutton, "margin", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->use_wakeup_timer_checkbutton,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->use_wakeup_timer_checkbutton), g_par.use_wakeup_timer);
    gtk_widget_set_tooltip_text(dialog->use_wakeup_timer_checkbutton
            , _("Use this timer if Orage has problems waking up properly after suspend or hibernate. (For example tray icon not refreshed or alarms not firing.)"));

    g_signal_connect(G_OBJECT(dialog->use_wakeup_timer_checkbutton), "toggled"
            , G_CALLBACK(use_wakeup_timer_changed), dialog);

    /***** Default Display alarm for Foreign files *****/
    dialog->foreign_alarm_radiobutton_group = NULL;
    hbox = gtk_grid_new ();
    g_object_set (hbox, "column-homogeneous", TRUE, NULL);
    dialog->foreign_alarm_frame = orage_create_framebox_with_content(
            _("Foreign file default visual alarm"), GTK_SHADOW_NONE, hbox);

    gtk_grid_attach_next_to (GTK_GRID (dialog->extra_vbox),
                             dialog->foreign_alarm_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->foreign_alarm_orage_radiobutton =
            gtk_radio_button_new_with_mnemonic(NULL, _("Orage window"));
    g_object_set (dialog->foreign_alarm_orage_radiobutton,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->foreign_alarm_orage_radiobutton, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->foreign_alarm_orage_radiobutton)
            , dialog->foreign_alarm_radiobutton_group);
    dialog->foreign_alarm_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->foreign_alarm_orage_radiobutton));
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(dialog->foreign_alarm_orage_radiobutton)
            , !g_par.use_foreign_display_alarm_notify);

    dialog->foreign_alarm_notification_radiobutton =
            gtk_radio_button_new_with_mnemonic(NULL, _("Notify notification"));
    g_object_set (dialog->foreign_alarm_notification_radiobutton,
                  "hexpand", TRUE,
                  "halign", GTK_ALIGN_FILL,
                  NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->foreign_alarm_notification_radiobutton,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_radio_button_set_group(
            GTK_RADIO_BUTTON(dialog->foreign_alarm_notification_radiobutton)
            , dialog->foreign_alarm_radiobutton_group);
    dialog->foreign_alarm_radiobutton_group = gtk_radio_button_get_group(
            GTK_RADIO_BUTTON(dialog->foreign_alarm_notification_radiobutton));
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(dialog->foreign_alarm_notification_radiobutton)
            , g_par.use_foreign_display_alarm_notify);

    g_signal_connect(G_OBJECT(dialog->foreign_alarm_notification_radiobutton)
            , "toggled", G_CALLBACK(foreign_alarm_changed), dialog);

    /***** always quit *****/
    hbox = gtk_grid_new ();
    dialog->always_quit_frame = orage_create_framebox_with_content(
            _("Always quit when asked to close"), GTK_SHADOW_NONE, hbox);

    gtk_grid_attach_next_to (GTK_GRID (dialog->extra_vbox),
                             dialog->always_quit_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    dialog->always_quit_checkbutton = 
            gtk_check_button_new_with_mnemonic(_("Always quit"));
    g_object_set (dialog->always_quit_checkbutton, "margin", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), dialog->always_quit_checkbutton,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->always_quit_checkbutton), g_par.close_means_quit);
    gtk_widget_set_tooltip_text(dialog->always_quit_checkbutton
            , _("By default Orage stays open in the background when asked to close. This option changes Orage to quit and never stay in background when it is asked to close."));

    g_signal_connect(G_OBJECT(dialog->always_quit_checkbutton), "toggled"
            , G_CALLBACK(always_quit_changed), dialog);
}

static Itf *create_parameter_dialog(void)
{
    Itf *dialog;
    GdkPixbuf *orage_logo;

    dialog = g_new(Itf, 1);

    dialog->orage_dialog = gtk_dialog_new();
    gtk_window_set_default_size(GTK_WINDOW(dialog->orage_dialog), 300, 350);
    gtk_window_set_title(GTK_WINDOW(dialog->orage_dialog)
            , _("Orage Preferences"));
    gtk_window_set_position(GTK_WINDOW(dialog->orage_dialog)
            , GTK_WIN_POS_CENTER);
    gtk_window_set_modal(GTK_WINDOW(dialog->orage_dialog), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(dialog->orage_dialog), TRUE);
    orage_logo = orage_create_icon(FALSE, 48);
    gtk_window_set_icon(GTK_WINDOW(dialog->orage_dialog), orage_logo);
    g_object_unref(orage_logo);

    dialog->dialog_vbox1 =
            gtk_dialog_get_content_area (GTK_DIALOG(dialog->orage_dialog));

    dialog->notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(dialog->dialog_vbox1), dialog->notebook);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->notebook), 5);

    create_parameter_dialog_main_setup_tab(dialog);
    create_parameter_dialog_calendar_setup_tab(dialog);
    create_parameter_dialog_extra_setup_tab(dialog);

    /* the rest */
    dialog->help_button = orage_util_image_button ("help-browser", _("_Help"));
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog->orage_dialog)
            , dialog->help_button, GTK_RESPONSE_HELP);
    dialog->close_button = orage_util_image_button ("window-close",_("_Close"));
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog->orage_dialog)
            , dialog->close_button, GTK_RESPONSE_CLOSE);
    gtk_widget_set_can_default(dialog->close_button, TRUE);

    g_signal_connect(G_OBJECT(dialog->orage_dialog), "response"
            , G_CALLBACK(dialog_response), dialog);

    gtk_widget_show_all(dialog->orage_dialog);
    /*
    gdk_x11_window_set_user_time(GTK_WIDGET(dialog->orage_dialog)->window, 
            gdk_x11_get_server_time(GTK_WIDGET(dialog->orage_dialog)->window));
            */

    return(dialog);
}

static OrageRc *orage_parameters_file_open(gboolean read_only)
{
    gchar *fpath;
    OrageRc *orc;

    fpath = orage_config_file_location(ORAGE_PAR_DIR_FILE);
    if ((orc = orage_rc_file_open(fpath, read_only)) == NULL) {
        g_warning ("orage_parameters_file_open: Parameter file open failed.(%s)", fpath);
    }
    g_free(fpath);

    return(orc);
}

/* let's try to find the timezone name by comparing this file to
 * timezone files from the default location. This does not work
 * always, but is the best trial we can do */
static void init_dtz_check_dir(gchar *tz_dirname, gchar *tz_local, gint len)
{
    gint tz_offset = strlen("/usr/share/zoneinfo/");
    gsize tz_len;         /* file lengths */
    GDir *tz_dir;         /* location of timezone data /usr/share/zoneinfo */
    const gchar *tz_file; /* filename containing timezone data */
    gchar *tz_fullfile;   /* full filename */
    gchar *tz_data;       /* contents of the timezone data file */
    GError *error = NULL;

    if ((tz_dir = g_dir_open(tz_dirname, 0, NULL))) {
        while ((tz_file = g_dir_read_name(tz_dir)) 
                && g_par.local_timezone == NULL) {
            tz_fullfile = g_strconcat(tz_dirname, "/", tz_file, NULL);
            if (g_file_test(tz_fullfile, G_FILE_TEST_IS_SYMLINK))
                ; /* we do not accept these, just continue searching */
            else if (g_file_test(tz_fullfile, G_FILE_TEST_IS_DIR)) {
                init_dtz_check_dir(tz_fullfile, tz_local, len);
            }
            else if (g_file_get_contents(tz_fullfile, &tz_data, &tz_len
                    , &error)) {
                if (len == (int)tz_len && !memcmp(tz_local, tz_data, len)) {
                    /* this is a match (length is tested first since that 
                     * test is quick and it eliminates most) */
                    g_par.local_timezone = g_strdup(tz_fullfile+tz_offset);
                }
                g_free(tz_data);
            }
            else { /* we should never come here */
                g_warning("init_default_timezone: can not read (%s) %s"
                        , tz_fullfile, error->message);
                g_error_free(error);
                error = NULL;
            }
            /* if we found a candidate, test that libical knows it */
            if (g_par.local_timezone && !xfical_set_local_timezone(TRUE)) { 
                /* candidate is not known by libical, remove it */
                g_free(g_par.local_timezone);
                g_par.local_timezone = NULL;
            }
            g_free(tz_fullfile);
        } /* while */
        g_dir_close(tz_dir);
    }
}

static void init_default_timezone(void)
{
    gsize len;            /* file lengths */
    gchar *tz_local;      /* local timezone data */

    g_free(g_par.local_timezone);
    g_par.local_timezone = NULL;
    g_message(_("First Orage start. Searching default timezone."));
    /* debian, ubuntu stores the timezone name into /etc/timezone */
    if (g_file_get_contents("/etc/timezone", &g_par.local_timezone
                , &len, NULL)) {
        /* success! let's check it */
        if (len > 2) /* get rid of the \n at the end */
            g_par.local_timezone[len-1] = 0;
            /* we have a candidate, test that libical knows it */
        if (!xfical_set_local_timezone(TRUE)) { 
            g_free(g_par.local_timezone);
            g_par.local_timezone = NULL;
        }
    }
    /* redhat, gentoo, etc. stores the timezone data into /etc/localtime */
    else if (g_file_get_contents("/etc/localtime", &tz_local, &len, NULL)) {
        init_dtz_check_dir("/usr/share/zoneinfo", tz_local, len);
        g_free(tz_local);
    }
    if (ORAGE_STR_EXISTS(g_par.local_timezone))
        g_message(_("Default timezone set to %s."), g_par.local_timezone);
    else {
        g_par.local_timezone = g_strdup("UTC");
        g_message(_("Default timezone not found, please, set it manually."));
    }
}

void read_parameters(void)
{
    gchar *fpath;
    OrageRc *orc;
    gint i;
    gchar f_par[100];

    orc = orage_parameters_file_open(TRUE);

    orage_rc_set_group(orc, "PARAMETERS");
    g_par.local_timezone = orage_rc_get_str(orc, "Timezone", "not found");
    if (!strcmp(g_par.local_timezone, "not found")) { 
        init_default_timezone();
    }
#ifdef HAVE_ARCHIVE
    g_par.archive_limit = orage_rc_get_int(orc, "Archive limit", 0);
    fpath = orage_data_file_location(ORAGE_ARC_DIR_FILE);
    g_par.archive_file = orage_rc_get_str(orc, "Archive file", fpath);
    g_free(fpath);
#endif
    fpath = orage_data_file_location(ORAGE_APP_DIR_FILE);
    g_par.orage_file = orage_rc_get_str(orc, "Orage file", fpath);
    g_free(fpath);
    g_par.sound_application=orage_rc_get_str(orc, "Sound application", "play");
    g_par.pos_x = orage_rc_get_int(orc, "Main window X", 0);
    g_par.pos_y = orage_rc_get_int(orc, "Main window Y", 0);
    g_par.size_x = orage_rc_get_int(orc, "Main window size X", 0);
    g_par.size_y = orage_rc_get_int(orc, "Main window size Y", 0);
    g_par.el_pos_x = orage_rc_get_int(orc, "Eventlist window pos X", 0);
    g_par.el_pos_y = orage_rc_get_int(orc, "Eventlist window pos Y", 0);
    g_par.el_size_x = orage_rc_get_int(orc, "Eventlist window X", 500);
    g_par.el_size_y = orage_rc_get_int(orc, "Eventlist window Y", 350);
    g_par.el_days = orage_rc_get_int(orc, "Eventlist extra days", 0);
    g_par.el_only_first = orage_rc_get_bool(orc, "Eventlist only first", FALSE);
    g_par.dw_pos_x = orage_rc_get_int(orc, "Dayview window pos X", 0);
    g_par.dw_pos_y = orage_rc_get_int(orc, "Dayview window pos Y", 0);
    g_par.dw_size_x = orage_rc_get_int(orc, "Dayview window X", 690);
    g_par.dw_size_y = orage_rc_get_int(orc, "Dayview window Y", 390);
    g_par.dw_week_mode = orage_rc_get_bool(orc, "Dayview week mode", TRUE);
    g_par.show_menu = orage_rc_get_bool(orc, "Show Main Window Menu", TRUE);
    g_par.select_always_today = 
            orage_rc_get_bool(orc, "Select Always Today", FALSE);
    g_par.show_borders = orage_rc_get_bool(orc, "Show borders", TRUE);
    g_par.show_heading = orage_rc_get_bool(orc, "Show heading", TRUE);
    g_par.show_day_names = orage_rc_get_bool(orc, "Show day names", TRUE);
    g_par.show_weeks = orage_rc_get_bool(orc, "Show weeks", TRUE);
    g_par.show_todos = orage_rc_get_bool(orc, "Show todos", TRUE);
    g_par.show_event_days = orage_rc_get_int(orc, "Show event days", 1);
    g_par.show_pager = orage_rc_get_bool(orc, "Show in pager", TRUE);
    g_par.show_systray = orage_rc_get_bool(orc, "Show in systray", TRUE);
    g_par.show_taskbar = orage_rc_get_bool(orc, "Show in taskbar", TRUE);
    g_par.start_visible = orage_rc_get_bool(orc, "Start visible", TRUE);
    g_par.start_minimized = orage_rc_get_bool(orc, "Start minimized", FALSE);
    g_par.set_stick = orage_rc_get_bool(orc, "Set sticked", TRUE);
    g_par.set_ontop = orage_rc_get_bool(orc, "Set ontop", FALSE);
    g_par.own_icon_file = orage_rc_get_str(orc, "Own icon file"
            , PACKAGE_DATA_DIR "/icons/hicolor/160x160/apps/orage.xpm");
    g_par.own_icon_row1_data = orage_rc_get_str(orc
            , "Own icon row1 data", "%a");
    g_par.own_icon_row2_data = orage_rc_get_str(orc
            , "Own icon row2 data", "%d");
    g_par.own_icon_row3_data = orage_rc_get_str(orc
            , "Own icon row3 data", "%b");
    g_par.ical_weekstartday = get_first_weekday (orc);
    g_par.show_days = orage_rc_get_bool(orc, "Show days", FALSE);
    g_par.foreign_count = orage_rc_get_int(orc, "Foreign file count", 0);
    for (i = 0; i < g_par.foreign_count; i++) {
        g_snprintf(f_par, sizeof (f_par), "Foreign file %02d name", i);
        g_par.foreign_data[i].file = orage_rc_get_str(orc, f_par, NULL);
        g_snprintf(f_par, sizeof (f_par), "Foreign file %02d read-only", i);
        g_par.foreign_data[i].read_only = orage_rc_get_bool(orc, f_par, TRUE);
        g_snprintf(f_par, sizeof (f_par), "Foreign file %02d visible name", i);
        g_par.foreign_data[i].name = orage_rc_get_str(orc, f_par, g_par.foreign_data[i].file);
    }
    g_par.use_foreign_display_alarm_notify = orage_rc_get_bool(orc
            , "Use notify foreign alarm", FALSE);
    g_par.priority_list_limit = orage_rc_get_int(orc, "Priority list limit", 8);
    g_par.use_wakeup_timer = orage_rc_get_bool(orc, "Use wakeup timer", TRUE);
    g_par.close_means_quit = orage_rc_get_bool(orc, "Always quit", FALSE);
    g_par.file_close_delay = orage_rc_get_int(orc, "File close delay", 600);

    orage_rc_file_close(orc);
}

void write_parameters(void)
{
    OrageRc *orc;
    gint i;
    gchar f_par[50];

    orc = orage_parameters_file_open(FALSE);

    orage_rc_set_group(orc, "PARAMETERS");
    orage_rc_put_str(orc, "Timezone", g_par.local_timezone);
#ifdef HAVE_ARCHIVE
    orage_rc_put_int(orc, "Archive limit", g_par.archive_limit);
    orage_rc_put_str(orc, "Archive file", g_par.archive_file);
#endif
    orage_rc_put_str(orc, "Orage file", g_par.orage_file);
    orage_rc_put_str(orc, "Sound application", g_par.sound_application);
    gtk_window_get_size(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow)
            , &g_par.size_x, &g_par.size_y);
    gtk_window_get_position(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow)
            , &g_par.pos_x, &g_par.pos_y);
    orage_rc_put_int(orc, "Main window X", g_par.pos_x);
    orage_rc_put_int(orc, "Main window Y", g_par.pos_y);
    orage_rc_put_int(orc, "Main window size X", g_par.size_x);
    orage_rc_put_int(orc, "Main window size Y", g_par.size_y);
    orage_rc_put_int(orc, "Eventlist window pos X", g_par.el_pos_x);
    orage_rc_put_int(orc, "Eventlist window pos Y", g_par.el_pos_y);
    orage_rc_put_int(orc, "Eventlist window X", g_par.el_size_x);
    orage_rc_put_int(orc, "Eventlist window Y", g_par.el_size_y);
    orage_rc_put_int(orc, "Eventlist extra days", g_par.el_days);
    orage_rc_put_bool(orc, "Eventlist only first", g_par.el_only_first);
    orage_rc_put_int(orc, "Dayview window pos X", g_par.dw_pos_x);
    orage_rc_put_int(orc, "Dayview window pos Y", g_par.dw_pos_y);
    orage_rc_put_int(orc, "Dayview window X", g_par.dw_size_x);
    orage_rc_put_int(orc, "Dayview window Y", g_par.dw_size_y);
    orage_rc_put_bool(orc, "Dayview week mode", g_par.dw_week_mode);
    orage_rc_put_bool(orc, "Show Main Window Menu", g_par.show_menu);
    orage_rc_put_bool(orc, "Select Always Today", g_par.select_always_today);
    orage_rc_put_bool(orc, "Show borders", g_par.show_borders);
    orage_rc_put_bool(orc, "Show heading", g_par.show_heading);
    orage_rc_put_bool(orc, "Show day names", g_par.show_day_names);
    orage_rc_put_bool(orc, "Show weeks", g_par.show_weeks);
    orage_rc_put_bool(orc, "Show todos", g_par.show_todos);
    orage_rc_put_int(orc, "Show event days", g_par.show_event_days);
    orage_rc_put_bool(orc, "Show in pager", g_par.show_pager);
    orage_rc_put_bool(orc, "Show in systray", g_par.show_systray);
    orage_rc_put_bool(orc, "Show in taskbar", g_par.show_taskbar);
    orage_rc_put_bool(orc, "Start visible", g_par.start_visible);
    orage_rc_put_bool(orc, "Start minimized", g_par.start_minimized);
    orage_rc_put_bool(orc, "Set sticked", g_par.set_stick);
    orage_rc_put_bool(orc, "Set ontop", g_par.set_ontop);
    orage_rc_put_str(orc, "Own icon file", g_par.own_icon_file);
    orage_rc_put_str(orc, "Own icon row1 data", g_par.own_icon_row1_data);
    orage_rc_put_str(orc, "Own icon row2 data", g_par.own_icon_row2_data);
    orage_rc_put_str(orc, "Own icon row3 data", g_par.own_icon_row3_data);
    /* we write this with X so that we do not read it back unless
     * it is manually changed. It should need changes really seldom. */
    orage_rc_put_int(orc, "XIcal week start day", g_par.ical_weekstartday);
    orage_rc_put_bool(orc, "Show days", g_par.show_days);
    orage_rc_put_int(orc, "Foreign file count", g_par.foreign_count);
    /* add what we have and remove the rest */
    for (i = 0; i < g_par.foreign_count;  i++) {
        g_snprintf(f_par, sizeof (f_par), "Foreign file %02d name", i);
        orage_rc_put_str(orc, f_par, g_par.foreign_data[i].file);
        g_snprintf(f_par, sizeof (f_par), "Foreign file %02d read-only", i);
        orage_rc_put_bool(orc, f_par, g_par.foreign_data[i].read_only);
        g_snprintf(f_par, sizeof (f_par), "Foreign file %02d visible name", i);
        orage_rc_put_str(orc, f_par, g_par.foreign_data[i].name);
    }
    for (i = g_par.foreign_count; i < 10;  i++) {
        g_snprintf(f_par, sizeof (f_par), "Foreign file %02d name", i);
        if (!orage_rc_exists_item(orc, f_par))
            break; /* it is in order, so we know that the rest are missing */
        orage_rc_del_item(orc, f_par);
        g_snprintf(f_par, sizeof (f_par), "Foreign file %02d read-only", i);
        orage_rc_del_item(orc, f_par);
        g_snprintf(f_par, sizeof (f_par), "Foreign file %02d visible name", i);
        orage_rc_del_item(orc, f_par);
    }
    orage_rc_put_bool(orc, "Use notify foreign alarm"
            , g_par.use_foreign_display_alarm_notify);
    orage_rc_put_int(orc, "Priority list limit", g_par.priority_list_limit);
    orage_rc_put_bool(orc, "Use wakeup timer", g_par.use_wakeup_timer);
    orage_rc_put_bool(orc, "Always quit", g_par.close_means_quit);
    orage_rc_put_int(orc, "File close delay", g_par.file_close_delay);

    orage_rc_file_close(orc);
}

void show_parameters(void)
{
    static Itf *dialog = NULL;

    if (is_running) {
        gtk_window_present(GTK_WINDOW(dialog->orage_dialog));
    }
    else {
        is_running = TRUE;
        dialog = create_parameter_dialog();
    }
}

void set_parameters(void)
{
    set_menu();
    set_border();
    set_taskbar();
    set_pager();
    set_calendar();
    /*
    set_systray();
    */
    set_stick();
    set_ontop();
    set_wakeup_timer();
    xfical_set_local_timezone(FALSE);
}
