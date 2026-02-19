/*
 * Copyright (c) 2021-2026 Erkki Moorits
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>
#include <locale.h>
#include <inttypes.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "functions.h"
#include "ical-code.h"
#include "orage-application.h"
#include "orage-i18n.h"
#include "orage-rc-file.h"
#include "orage-sync-edit-dialog.h"
#include "orage-sync-ext-command.h"
#include "orage-task-runner.h"
#include "orage-time-utils.h"
#include "orage-window.h"
#include "parameters.h"
#include "parameters_internal.h"
#include "reminder.h"
#include "timezone_selection.h"

#ifdef HAVE_X11_TRAY_ICON
#include <gdk/gdkx.h>
#include "tray_icon.h"
#endif

#ifndef DEFAULT_SOUND_COMMAND
#define DEFAULT_SOUND_COMMAND "play"
#endif

#define ORAGE_WAKEUP_TIMER_PERIOD 60

#define MAIN_WINDOW_PANED_POSITION "Main window paned position"
#define USE_NEW_UI "Use new UI"
#define SYNC_SOURCE_COUNT "Sync source count"
#define SYNC_DESCRIPTION "Sync %02d description"
#define SYNC_COMMAND "Sync %02d command"
#define SYNC_PERIOD "Sync %02d period"

static void fill_sync_entries (gpointer data, gpointer user_data);
static void orage_sync_task_remove (const orage_task_runner_conf *conf);
static gint orage_sync_task_add (const gchar *description,
                                 const gchar *command,
                                 const uint period);

static void orage_sync_task_change (const orage_task_runner_conf *conf,
                                    const gchar *description,
                                    const gchar *command,
                                    const guint period);

static Itf *global_itf = NULL;

global_parameters g_par;

/* 0 = monday, ..., 6 = sunday */
static gint get_first_weekday (OrageRc *orc)
{
    gint first_week_day;

    first_week_day = orage_rc_get_int (orc, "Ical week start day", -1);

    if (first_week_day == -1)
        first_week_day = orage_get_first_weekday ();

    return first_week_day;
}

static GSList *get_sync_entries_list (void)
{
    GSList *list = NULL;
    guint i;

    for (i = 0; i < (guint)g_par.sync_source_count; i++)
        list = g_slist_append (list, &g_par.sync_conf[i]);

    return list;
}

static void dialog_response(GtkWidget *dialog, gint response_id
        , gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    if (response_id == GTK_RESPONSE_HELP)
        orage_open_help_page ();
    else { /* delete signal or close response */
        write_parameters();
        gtk_widget_destroy(dialog);
        g_free(itf);
        global_itf = NULL;
    }
}

static void sound_application_changed (G_GNUC_UNUSED GtkWidget *dialog,
                                       gpointer user_data)
{
    Itf *itf = (Itf *)user_data;
    
    if (g_par.sound_application)
        g_free(g_par.sound_application);
    g_par.sound_application = g_strdup(gtk_entry_get_text(
            GTK_ENTRY(itf->sound_application_entry)));
}

static void set_border (void)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    gtk_window_set_decorated (GTK_WINDOW (orage_application_get_window (app)),
                              g_par.show_borders);
}

static void ui_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.use_new_ui = gtk_toggle_button_get_active (
        GTK_TOGGLE_BUTTON (itf->use_new_ui_checkbutton));
}

static void borders_changed (G_GNUC_UNUSED GtkWidget *dialog,
                             gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_borders = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_borders_checkbutton));
    set_border();
}

static void set_menu(void)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    OrageWindow *window = ORAGE_WINDOW (orage_application_get_window (app));

    orage_window_show_menubar (window, g_par.show_menu);
}

static void menu_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_menu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_menu_checkbutton));
    set_menu();
}

static void set_calendar(void)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    OrageWindow *window = ORAGE_WINDOW (orage_application_get_window (app));
    guint options = (g_par.show_heading ? ORAGE_WINDOW_SHOW_CALENDAR_HEADING : 0)
                  | (g_par.show_day_names ? ORAGE_WINDOW_SHOW_DAY_NAMES : 0)
                  | (g_par.show_weeks ? ORAGE_WINDOW_SHOW_WEEK_NUMBERS : 0);
    orage_window_set_calendar_options (window, options);
}

static void heading_changed (G_GNUC_UNUSED GtkWidget *dialog,
                             gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_heading = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_heading_checkbutton));
    set_calendar();
}

static void days_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_day_names = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_day_names_checkbutton));
    set_calendar();
}

static void weeks_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_weeks = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_weeks_checkbutton));
    set_calendar();
}

static void todos_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    OrageWindow *window = ORAGE_WINDOW (orage_application_get_window (app));
    Itf *itf = (Itf *)user_data;

    g_par.show_todos = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_todos_checkbutton));
    if (g_par.show_todos)
        orage_window_build_todo (window);
    else {
        orage_window_hide_todo (window);
        /* hide the whole area if also event box does not exist */
        if (!g_par.show_event_days)
            gtk_window_resize (GTK_WINDOW (window), g_par.size_x, 1);
    }
}

static void show_events_spin_changed (GtkSpinButton *sb,
                                      G_GNUC_UNUSED gpointer user_data)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    OrageWindow *window = ORAGE_WINDOW (orage_application_get_window (app));

    g_par.show_event_days = gtk_spin_button_get_value(sb);
    if (g_par.show_event_days)
        orage_window_build_events (window);
    else {
        orage_window_hide_event (window);
        /* hide the whole area if also todo box does not exist */
        if (!g_par.show_todos)
            gtk_window_resize (GTK_WINDOW (window), g_par.size_x, 1);
    }
}

static void set_stick(void)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    GtkWindow *window = GTK_WINDOW (orage_application_get_window (app));

    if (g_par.set_stick)
        gtk_window_stick (window);
    else
        gtk_window_unstick (window);
}

static void stick_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.set_stick = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->set_stick_checkbutton));
    set_stick();
}

static void set_ontop(void)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    gtk_window_set_keep_above (GTK_WINDOW (orage_application_get_window (app)),
                               g_par.set_ontop);
}

static void ontop_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.set_ontop = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->set_ontop_checkbutton));
    set_ontop();
}

static void set_taskbar(void)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (orage_application_get_window (app)),
                                      !g_par.show_taskbar);
}

static void taskbar_changed (G_GNUC_UNUSED GtkWidget *dialog,
                             gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_taskbar = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_taskbar_checkbutton));
    set_taskbar();
}

static void set_pager(void)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());
    gtk_window_set_skip_pager_hint (GTK_WINDOW (orage_application_get_window (app)),
                                    !g_par.show_pager);
}

static void pager_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_pager = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_pager_checkbutton));
    set_pager();
}

#ifdef HAVE_X11_TRAY_ICON
static void set_systray(void)
{
    GtkStatusIcon *status_icon = (GtkStatusIcon *)g_par.trayIcon;

    if (!(status_icon && orage_status_icon_is_embedded (status_icon)))
    {
        status_icon = orage_create_trayicon ();
        g_par.trayIcon = status_icon;
    }

    orage_status_icon_set_visible (status_icon, g_par.show_systray);
}

static void systray_changed (G_GNUC_UNUSED GtkWidget *dialog,
                             gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_systray = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->show_systray_checkbutton));
    set_systray();
}
#endif

static void start_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.start_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->visibility_show_radiobutton));
    g_par.start_minimized = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->visibility_minimized_radiobutton));
}

static void show_changed (G_GNUC_UNUSED GtkWidget *dialog, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.show_days = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->click_to_show_days_radiobutton));
}

static void foreign_alarm_changed (G_GNUC_UNUSED GtkWidget *dialog,
                                   gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.use_foreign_display_alarm_notify = 
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->foreign_alarm_notification_radiobutton));
}

static void dw_week_mode_changed (G_GNUC_UNUSED GtkWidget *dialog,
                                  gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.dw_week_mode = 
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            itf->dw_week_mode_week_radiobutton));
}

static void sound_application_open_button_clicked (
    G_GNUC_UNUSED GtkButton *button, gpointer user_data)
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
}

static void timezone_button_clicked(GtkButton *button, gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    if (!ORAGE_STR_EXISTS(g_par.local_timezone)) {
        g_warning ("local timezone not set, defaulting to 'UTC'");
        g_par.local_timezone = g_strdup("UTC");
    }
    if (orage_timezone_button_clicked(button, GTK_WINDOW(itf->orage_dialog)
            , &g_par.local_timezone, TRUE, g_par.local_timezone))
        xfical_set_local_timezone(FALSE);
}

#ifdef HAVE_ARCHIVE
static void archive_threshold_spin_changed(GtkSpinButton *sb
        , G_GNUC_UNUSED gpointer user_data)
{
    g_par.archive_limit = gtk_spin_button_get_value(sb);
}
#endif

static void select_day_changed (G_GNUC_UNUSED GtkWidget *dialog,
                                gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.select_always_today = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(itf->select_day_today_radiobutton));
}

static void el_extra_days_spin_changed (GtkSpinButton *sb,
                                        G_GNUC_UNUSED gpointer user_data)
{
    g_par.el_days = gtk_spin_button_get_value(sb);
}

static void el_only_first_checkbutton_clicked(GtkCheckButton *cb
        , G_GNUC_UNUSED gpointer user_data)
{
    g_par.el_only_first = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cb));
}

/* This function monitors that we do not loose time.  It checks if longer time
   than expected wakeup time has passed and fixes timings if that is the case.
   This is needed since sometimes hibernate and suspend does not do a good job
   in firing Orage timers and timing gets wrong.
*/
static gboolean check_wakeup(gpointer user_data)
{
    static time_t tt_prev=0;
    time_t tt_new=0;

    tt_new = time(NULL);
    if (tt_new - tt_prev > ORAGE_WAKEUP_TIMER_PERIOD * 2) {
        /* we very rarely come here. */
        /* user_data is normally NULL, but first call it has some value,
           which means that this is init call */
        if (!user_data) { /* normal timer call */
            g_message ("wakeup timer refreshing");
            alarm_read();
            /* It is quite possible that day did not change,
               but we need to reset timers */
            orage_day_change(&tt_prev);
        }
        else {
            g_debug ("wakeup timer init %" PRIiMAX, (intmax_t)tt_prev);
        }
    }
    tt_prev = tt_new;
    return(TRUE);
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

static void use_wakeup_timer_changed (G_GNUC_UNUSED GtkWidget *dialog,
                                      gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.use_wakeup_timer = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(itf->use_wakeup_timer_checkbutton));
    set_wakeup_timer();
}

static void always_quit_changed (G_GNUC_UNUSED GtkWidget *dialog,
                                 gpointer user_data)
{
    Itf *itf = (Itf *)user_data;

    g_par.close_means_quit = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(itf->always_quit_checkbutton));
}

static void refresh_sync_entries (Itf *dialog)
{
    GSList *list;

    if (dialog->sync_scrolled_window)
        gtk_widget_destroy (dialog->sync_scrolled_window);

    dialog->sync_entries_list = gtk_grid_new ();

    g_object_set (dialog->sync_entries_list, "hexpand", TRUE,
                                             "halign", GTK_ALIGN_FILL,
                                             "vexpand", TRUE,
                                             "valign", GTK_ALIGN_FILL,
                                             "margin-top", 10,
                                             "row-spacing", 5,
                                             "column-spacing", 5,
                                             NULL);

    list = get_sync_entries_list ();
    g_slist_foreach (list, fill_sync_entries, dialog);
    g_slist_free (list);

    dialog->sync_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (dialog->sync_scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (dialog->sync_scrolled_window),
                       dialog->sync_entries_list);
    gtk_grid_attach_next_to (GTK_GRID (dialog->sync_vbox),
                             dialog->sync_scrolled_window,
                             NULL, GTK_POS_BOTTOM, 1, 1);

    gtk_widget_show_all (dialog->sync_scrolled_window);
}

/* Show empty dialog->save dialog results->refresh parameters. */
static void on_sync_add_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                    gpointer user_data)
{
    const gchar *description;
    const gchar *command;
    guint period;
    gint idx;
    gint result;
    OrageSyncEditDialog *dialog;
    OrageApplication *app;

    dialog = ORAGE_SYNC_EDIT_DIALOG (orage_sync_edit_dialog_new ());
    result = gtk_dialog_run (GTK_DIALOG (dialog));

    if ((result == GTK_RESPONSE_OK) && orage_sync_edit_dialog_is_changed (dialog))
    {
        description = orage_sync_edit_dialog_get_description (dialog);
        command = orage_sync_edit_dialog_get_command (dialog);
        period = orage_sync_edit_dialog_get_period (dialog) * 60;

        idx = orage_sync_task_add (description, command, period);
        if (idx >= 0)
        {
            app = ORAGE_APPLICATION (g_application_get_default ());
            orage_task_runner_add (orage_application_get_sync (app),
                                   orage_sync_ext_command,
                                   &g_par.sync_conf[idx]);
        }
    }

    gtk_widget_destroy (GTK_WIDGET (dialog));
    refresh_sync_entries ((Itf *)user_data);
}

static void on_sync_edit_clicked_cb (G_GNUC_UNUSED GtkButton *b,
                                     gpointer user_data)
{
    gint result;
    const gchar *description;
    const gchar *command;
    guint period;
    OrageSyncEditDialog *dialog;
    orage_task_runner_conf *conf = (orage_task_runner_conf *)user_data;
    OrageApplication *app;
    OrageTaskRunner *runner;

    g_debug ("editing sync task '%s'", conf->description);

    dialog = ORAGE_SYNC_EDIT_DIALOG (
            orage_sync_edit_dialog_new_with_defaults (conf->description,
                                                      conf->command,
                                                      conf->period / 60));

    result = gtk_dialog_run (GTK_DIALOG (dialog));
    if ((result == GTK_RESPONSE_OK) && orage_sync_edit_dialog_is_changed (dialog))
    {
        description = orage_sync_edit_dialog_get_description (dialog);
        command = orage_sync_edit_dialog_get_command (dialog);
        period = orage_sync_edit_dialog_get_period (dialog) * 60;

        g_debug ("updated sync task '%s' (period: %u seconds)",
                description, period);

        app = ORAGE_APPLICATION (g_application_get_default ());
        runner = orage_application_get_sync (app);
        orage_task_runner_remove (runner, conf);
        orage_sync_task_change (conf, description, command, period);
        orage_task_runner_add (runner, orage_sync_ext_command, conf);
    }

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void on_sync_remove_clicked_remove_cb (G_GNUC_UNUSED GtkButton *b,
                                              gpointer user_data)
{
    orage_task_runner_conf *conf = (orage_task_runner_conf *)user_data;
    OrageApplication *app;
    gint result;

    result = orage_warning_dialog (NULL,
                                   _("Remove selected synchronization task."),
                                   _("Do you want to continue?"),
                                   _("No, cancel the removal"),
                                   _("Yes, remove"));

    if (result == GTK_RESPONSE_YES)
    {
        g_debug ("removing sync task '%s'", conf->description);

        /* As orage_sync_task_remove reorder configuration in global
         * parameters list, then orage_task_runner_remove should be always
         * before orage_sync_task_remove.
         */
        app = ORAGE_APPLICATION (g_application_get_default ());
        orage_task_runner_remove (orage_application_get_sync (app), conf);
        orage_sync_task_remove (conf);
    }
}

static void on_clicked_refresh_cb (G_GNUC_UNUSED GtkButton *b,
                                   gpointer user_data)
{
    refresh_sync_entries ((Itf *)user_data);
}

static void create_parameter_dialog_main_setup_tab(Itf *dialog)
{
    GtkWidget *hbox, *vbox;
#ifdef HAVE_ARCHIVE
    GtkWidget *label;
#endif

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
        g_warning ("parameters: local timezone missing");
        g_par.local_timezone = g_strdup("UTC");
    }
    gtk_button_set_label(GTK_BUTTON(dialog->timezone_button)
            , _(g_par.local_timezone));
    g_object_set (dialog->timezone_button, "margin-top", 5,
                                           "margin-bottom", 5,
                                           "margin-start", 5,
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
    g_object_set (dialog->archive_threshold_spin, "margin-start", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox),
                             dialog->archive_threshold_spin,
                             NULL, GTK_POS_RIGHT, 1, 1);
    label = gtk_label_new(_("(0 = no archiving)"));
    g_object_set (label, "margin-start", 10, NULL);
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
                  "margin-start", 5,
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
                  "margin-start", 10,
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

    dialog->use_new_ui_checkbutton =
        gtk_check_button_new_with_mnemonic (_("Use new user interface"));
    gtk_widget_set_tooltip_text (dialog->use_new_ui_checkbutton,
        _("Enable the new interface layout (requires restart)"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->use_new_ui_checkbutton),
                                  g_par.use_new_ui);

    dialog->show_borders_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show borders"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_borders_checkbutton), g_par.show_borders);

    table_add_row (table, dialog->use_new_ui_checkbutton,
                          dialog->show_borders_checkbutton,
                          row = 0);

    dialog->show_menu_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show menu"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_menu_checkbutton), g_par.show_menu);

    dialog->show_day_names_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show day names"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_day_names_checkbutton), g_par.show_day_names);

    table_add_row (table, dialog->show_menu_checkbutton,
                          dialog->show_day_names_checkbutton,
                          ++row);

    dialog->show_weeks_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show week numbers"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_weeks_checkbutton), g_par.show_weeks);

    dialog->show_heading_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show month and year"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_heading_checkbutton), g_par.show_heading);

    table_add_row (table, dialog->show_weeks_checkbutton,
                          dialog->show_heading_checkbutton,
                          ++row);

    g_signal_connect (G_OBJECT (dialog->use_new_ui_checkbutton), "toggled",
                      G_CALLBACK (ui_changed), dialog);
    g_signal_connect (G_OBJECT (dialog->show_borders_checkbutton), "toggled",
                      G_CALLBACK (borders_changed), dialog);
    g_signal_connect (G_OBJECT (dialog->show_menu_checkbutton), "toggled",
                      G_CALLBACK (menu_changed), dialog);
    g_signal_connect (G_OBJECT (dialog->show_heading_checkbutton), "toggled",
                      G_CALLBACK (heading_changed), dialog);
    g_signal_connect (G_OBJECT (dialog->show_day_names_checkbutton), "toggled",
                      G_CALLBACK (days_changed), dialog);
    g_signal_connect (G_OBJECT (dialog->show_weeks_checkbutton), "toggled",
                      G_CALLBACK (weeks_changed), dialog);

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
    g_object_set (label, "margin-start", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    dialog->show_events_spin = gtk_spin_button_new_with_range(0, 31, 1);
    g_object_set (dialog->show_events_spin, "margin-start", 10, NULL);
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
#ifdef HAVE_X11_TRAY_ICON
    dialog->show_systray_checkbutton = gtk_check_button_new_with_mnemonic(
            _("Show in systray"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            dialog->show_systray_checkbutton), g_par.show_systray);

    table_add_row(table, dialog->show_systray_checkbutton, NULL, ++row);
#endif

    g_signal_connect(G_OBJECT(dialog->set_stick_checkbutton), "toggled"
            , G_CALLBACK(stick_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->set_ontop_checkbutton), "toggled"
            , G_CALLBACK(ontop_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_taskbar_checkbutton), "toggled"
            , G_CALLBACK(taskbar_changed), dialog);
    g_signal_connect(G_OBJECT(dialog->show_pager_checkbutton), "toggled"
            , G_CALLBACK(pager_changed), dialog);
#ifdef HAVE_X11_TRAY_ICON
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
        g_signal_connect (G_OBJECT (dialog->show_systray_checkbutton),
                          "toggled", G_CALLBACK (systray_changed), dialog);
    }
#endif

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
    g_object_set (dialog->el_extra_days_spin, "margin-start", 10, NULL);
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

static void fill_sync_entries (gpointer conf_p, gpointer dialog_p)
{
    orage_task_runner_conf *conf = (orage_task_runner_conf *)conf_p;
    Itf *dialog = (Itf *)dialog_p;
    GtkGrid *list_grid = GTK_GRID (dialog->sync_entries_list);
    GtkWidget *grid;
    GtkWidget *description;
    GtkWidget *edit_button;
    GtkWidget *remove_button;

    description = gtk_label_new (conf->description);
    g_object_set (description, "hexpand", TRUE,
                               "halign", GTK_ALIGN_START,
                               NULL);
    gtk_label_set_ellipsize (GTK_LABEL (description), PANGO_ELLIPSIZE_END);
    edit_button = gtk_button_new_from_icon_name ("document-open", GTK_ICON_SIZE_BUTTON);
    remove_button = gtk_button_new_from_icon_name ("list-remove", GTK_ICON_SIZE_BUTTON);

    grid = gtk_grid_new ();
    gtk_grid_attach_next_to (GTK_GRID (grid), description, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (grid), edit_button, description, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (grid), remove_button, edit_button, GTK_POS_RIGHT, 1, 1);

    gtk_grid_attach_next_to (list_grid, grid, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect (edit_button, "clicked",
                      G_CALLBACK (on_sync_edit_clicked_cb), conf_p);
    g_signal_connect_after (edit_button, "clicked",
                            G_CALLBACK (on_clicked_refresh_cb), dialog_p);

    g_signal_connect (remove_button, "clicked",
                      G_CALLBACK (on_sync_remove_clicked_remove_cb), conf_p);
    g_signal_connect_after (remove_button, "clicked",
                            G_CALLBACK (on_clicked_refresh_cb), dialog_p);
}

static void create_parameter_dialog_sync_tab (Itf *dialog)
{
    GtkWidget *new_button;
    GtkWidget *vbox;

    dialog->sync_vbox = gtk_grid_new ();
    dialog->sync_tab = orage_create_framebox_with_content (
            NULL, GTK_SHADOW_NONE,  dialog->sync_vbox);
    g_object_set (dialog->sync_vbox, "row-spacing", 10,
                                     "margin-top", 10, NULL);

    dialog->sync_tab_label = gtk_label_new (_("Synchronization settings"));
    gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook),
                              dialog->sync_tab, dialog->sync_tab_label);

    vbox = gtk_grid_new ();
    dialog->sync_sources_frame = orage_create_framebox_with_content(
            _("Synchronization sources"), GTK_SHADOW_NONE, vbox);

    gtk_grid_attach_next_to (GTK_GRID (dialog->sync_vbox),
                             dialog->sync_sources_frame, NULL, GTK_POS_BOTTOM,
                             1, 1);

    new_button = orage_util_image_button ("list-add", _("_Add"));
    gtk_widget_set_halign (new_button, GTK_ALIGN_END);
    gtk_grid_attach_next_to (GTK_GRID (vbox), new_button, NULL,
                             GTK_POS_TOP, 1, 1);

    refresh_sync_entries (dialog);

    g_signal_connect (new_button, "clicked",
                      G_CALLBACK (on_sync_add_clicked_cb), dialog);
}

static Itf *create_parameter_dialog(void)
{
    Itf *dialog;

    dialog = g_new0 (Itf, 1);

    dialog->orage_dialog = gtk_dialog_new();
    gtk_window_set_default_size(GTK_WINDOW(dialog->orage_dialog), 300, 350);
    gtk_window_set_title(GTK_WINDOW(dialog->orage_dialog)
            , _("Orage Preferences"));
    gtk_window_set_position(GTK_WINDOW(dialog->orage_dialog)
            , GTK_WIN_POS_CENTER);
    gtk_window_set_modal(GTK_WINDOW(dialog->orage_dialog), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(dialog->orage_dialog), TRUE);
    gtk_window_set_icon_name (GTK_WINDOW (dialog->orage_dialog), ORAGE_APP_ID);

    dialog->dialog_vbox1 =
            gtk_dialog_get_content_area (GTK_DIALOG(dialog->orage_dialog));

    dialog->notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(dialog->dialog_vbox1), dialog->notebook);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->notebook), 5);

    create_parameter_dialog_main_setup_tab(dialog);
    create_parameter_dialog_calendar_setup_tab(dialog);
    create_parameter_dialog_extra_setup_tab(dialog);
    create_parameter_dialog_sync_tab (dialog);

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

    return(dialog);
}

/** Remove task from configuration and reorder configuration structures.
 *  @param conf pointer to configuration that will be removed
 */
static void orage_sync_task_remove (const orage_task_runner_conf *conf)
{
    gboolean found = FALSE;
    gint i;

    for (i = 0; i < g_par.sync_source_count; i++)
    {
        if (&g_par.sync_conf[i] == conf)
        {
            found = TRUE;
            break;
        }
    }

    if (found == FALSE)
        return;

    g_free (g_par.sync_conf[i].description);
    g_free (g_par.sync_conf[i].command);
    g_par.sync_source_count--;

    for (; i < g_par.sync_source_count; i++)
    {
        g_par.sync_conf[i].description = g_par.sync_conf[i + 1].description;
        g_par.sync_conf[i].command = g_par.sync_conf[i + 1].command;
        g_par.sync_conf[i].period = g_par.sync_conf[i + 1].period;
    }

    g_par.sync_conf[i].description = NULL;
    g_par.sync_conf[i].command = NULL;
    g_par.sync_conf[i].period = 0;

    write_parameters ();
}

static gint orage_sync_task_add (const gchar *description,
                                 const gchar *command,
                                 const guint period)
{
    guint idx;

    if (g_par.sync_source_count >= NUMBER_OF_SYNC_SOURCES)
    {
        g_info ("sync source limit (%u) reached", NUMBER_OF_SYNC_SOURCES);
        return -1;
    }

    idx = g_par.sync_source_count;
    g_par.sync_conf[idx].description = g_strdup (description);
    g_par.sync_conf[idx].command = g_strdup (command);
    g_par.sync_conf[idx].period = period;
    g_par.sync_source_count++;

    write_parameters ();

    g_debug ("added sync task '%s' (period: %u seconds)", description, period);

    return idx;
}

static void orage_sync_task_change (const orage_task_runner_conf *conf,
                                    const gchar *description,
                                    const gchar *command,
                                    const guint period)
{
    gint i;

    for (i = 0; i < g_par.sync_source_count; i++)
    {
        if (&g_par.sync_conf[i] == conf)
        {
            if (g_par.sync_conf[i].description)
                g_free (g_par.sync_conf[i].description);

            if (g_par.sync_conf[i].command)
                g_free (g_par.sync_conf[i].command);

            g_par.sync_conf[i].description = g_strdup (description);
            g_par.sync_conf[i].command = g_strdup (command);
            g_par.sync_conf[i].period = period;

            write_parameters ();
            return;
        }
    }
}

static OrageRc *orage_parameters_file_open(gboolean read_only)
{
    gchar *fpath;
    OrageRc *orc;

    fpath = orage_config_file_location(ORAGE_PAR_DIR_FILE);
    if ((orc = orage_rc_file_open(fpath, read_only)) == NULL) {
        g_warning ("could not open parameter file '%s'", fpath);
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
                g_warning ("could not read timezone file '%s': %s",
                           tz_fullfile,
                           error ? error->message : "unknown error");
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
    g_message ("first Orage start. Searching default timezone");
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
        g_message ("default timezone set to '%s'", g_par.local_timezone);
    else {
        g_par.local_timezone = g_strdup("UTC");
        g_message ("default timezone not found, please, set it manually");
    }
}

void read_parameters (void)
{
    gchar *fpath;
    OrageRc *orc;
    gint i;
    gchar f_par[100];

    orc = orage_parameters_file_open(TRUE);
    if (orc == NULL)
    {
        g_warning ("cannot read parameters: configuration file not available");
        return;
    }

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
    g_par.sound_application = orage_rc_get_str (orc, "Sound application",
                                                DEFAULT_SOUND_COMMAND);
    g_par.paned_pos = orage_rc_get_int (orc, MAIN_WINDOW_PANED_POSITION, -1);
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
#ifdef HAVE_X11_TRAY_ICON
    g_par.show_systray = orage_rc_get_bool(orc, "Show in systray", TRUE);
#endif
    g_par.show_taskbar = orage_rc_get_bool(orc, "Show in taskbar", TRUE);
    g_par.start_visible = orage_rc_get_bool(orc, "Start visible", TRUE);
    g_par.start_minimized = orage_rc_get_bool(orc, "Start minimized", FALSE);
    g_par.set_stick = orage_rc_get_bool(orc, "Set sticked", TRUE);
    g_par.set_ontop = orage_rc_get_bool(orc, "Set ontop", FALSE);
    g_par.use_new_ui = orage_rc_get_bool (orc, USE_NEW_UI, FALSE);
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

    g_par.sync_source_count = orage_rc_get_int (orc, SYNC_SOURCE_COUNT, 0);
    for (i = 0; i < g_par.sync_source_count; i++)
    {
        g_snprintf (f_par, sizeof (f_par), SYNC_DESCRIPTION, i);
        g_par.sync_conf[i].description = orage_rc_get_str (orc, f_par, "");

        g_snprintf (f_par, sizeof (f_par), SYNC_COMMAND, i);
        g_par.sync_conf[i].command = orage_rc_get_str (orc, f_par, "");

        g_snprintf (f_par, sizeof (f_par), SYNC_PERIOD, i);
        g_par.sync_conf[i].period = orage_rc_get_int (orc, f_par, 0);
    }

    orage_rc_file_close(orc);
}

void write_parameters(void)
{
    OrageRc *orc;
    gint i;
    gchar f_par[50];
    OrageWindow *window;
    OrageApplication *app;

    orc = orage_parameters_file_open(FALSE);
    if (orc == NULL)
    {
        g_warning ("cannot write parameters: configuration file not available");
        return;
    }

    orage_rc_set_group(orc, "PARAMETERS");
    orage_rc_put_str(orc, "Timezone", g_par.local_timezone);
#ifdef HAVE_ARCHIVE
    orage_rc_put_int(orc, "Archive limit", g_par.archive_limit);
    orage_rc_put_str(orc, "Archive file", g_par.archive_file);
#endif
    orage_rc_put_str(orc, "Orage file", g_par.orage_file);
    orage_rc_put_str(orc, "Sound application", g_par.sound_application);

    app = ORAGE_APPLICATION (g_application_get_default ());
    window = ORAGE_WINDOW (orage_application_get_window (app));
    if (window)
        orage_window_save_window_state (window);
    else
        g_debug ("no main window available, skipping window state save");

    orage_rc_put_int (orc, MAIN_WINDOW_PANED_POSITION, g_par.paned_pos);
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
#ifdef HAVE_X11_TRAY_ICON
    orage_rc_put_bool(orc, "Show in systray", g_par.show_systray);
#endif
    orage_rc_put_bool(orc, "Show in taskbar", g_par.show_taskbar);
    orage_rc_put_bool(orc, "Start visible", g_par.start_visible);
    orage_rc_put_bool(orc, "Start minimized", g_par.start_minimized);
    orage_rc_put_bool(orc, "Set sticked", g_par.set_stick);
    orage_rc_put_bool(orc, "Set ontop", g_par.set_ontop);
    orage_rc_put_bool (orc, USE_NEW_UI, g_par.use_new_ui);
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

    orage_rc_put_int (orc, SYNC_SOURCE_COUNT, g_par.sync_source_count);

    for (i = 0; i < g_par.sync_source_count; i++)
    {
        g_snprintf (f_par, sizeof (f_par), SYNC_DESCRIPTION, i);
        orage_rc_put_str (orc, f_par, g_par.sync_conf[i].description);

        g_snprintf (f_par, sizeof (f_par), SYNC_COMMAND, i);
        orage_rc_put_str (orc, f_par, g_par.sync_conf[i].command);

        g_snprintf (f_par, sizeof (f_par), SYNC_PERIOD, i);
        orage_rc_put_int (orc, f_par, g_par.sync_conf[i].period);
    }

    for (i = g_par.sync_source_count; i < 10; i++)
    {
        g_snprintf (f_par, sizeof (f_par), SYNC_DESCRIPTION, i);
        if (!orage_rc_exists_item (orc, f_par))
            break;

        orage_rc_del_item (orc, f_par);

        g_snprintf (f_par, sizeof (f_par), SYNC_COMMAND, i);
        orage_rc_del_item (orc, f_par);

        g_snprintf (f_par, sizeof (f_par), SYNC_PERIOD, i);
        orage_rc_del_item (orc, f_par);
    }

    orage_rc_file_close(orc);
}

void show_parameters(void)
{
    if (global_itf != NULL) {
        gtk_window_present(GTK_WINDOW(global_itf->orage_dialog));
    }
    else {
        global_itf = create_parameter_dialog();
    }
}

void set_parameters(void)
{
    set_menu();
    set_border();
    set_taskbar();
    set_pager();
    set_calendar();
    set_stick();
    set_ontop();
    set_wakeup_timer();
    xfical_set_local_timezone(FALSE);
}
