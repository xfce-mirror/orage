/*
 * Copyright (c) 2021-2025 Erkki Moorits
 * Copyright (c) 2005-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2003-2006 Mickael Graf (korbinus at xfce.org)
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
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#ifdef HAVE_NOTIFY
#include <libnotify/notify.h>
#endif

#include "event-list.h"
#include "functions.h"
#include "ical-code.h"
#include "orage-alarm-structure.h"
#include "orage-appointment-window.h"
#include "orage-i18n.h"
#include "orage-rc-file.h"
#include "orage-window.h"
#include "parameters.h"
#include "reminder.h"

#ifdef HAVE_X11_TRAY_ICON
#include <gdk/gdkx.h>
#include "tray_icon.h"
#endif

#define RC_ALARM_TIME "ALARM_TIME"
#define RC_ACTION_TIME "ACTION_TIME"
#define RC_TITLE "TITLE"
#define RC_DESCRIPTION "DESCRIPTION"
#define RC_DISPLAY_ORAGE "DISPLAY_ORAGE"
#define RC_TEMPORARY "TEMPORARY"
#define RC_DISPLAY_NOTIFY "DISPLAY_NOTIFY"
#define RC_NOTIFY_TIMEOUT "NOTIFY_TIMEOUT"
#define RC_AUDIO "AUDIO"
#define RC_SOUND "SOUND"
#define RC_REPEAT_CNT "REPEAT_CNT"
#define RC_REPEAT_DELAY "REPEAT_DELAY"
#define RC_PROCEDURE "PROCEDURE"
#define RC_CMD "CMD"

#ifdef HAVE_NOTIFY
static gboolean orage_notify_initted = FALSE;
#endif

static void create_notify_reminder(alarm_struct *l_alarm);
static void reset_orage_alarm_clock (void);

void alarm_list_free(void)
{
    GDateTime *time_now;
    alarm_struct *l_alarm;
    GList *alarm_l, *kept_l=NULL;

    time_now = g_date_time_new_now_local ();

        /* FIXME: This could be tuned since now it runs into the remaining
           elements several times. They could be moved to another list and
           removed and added back at the end, remove with g_list_remove_link */
    for (alarm_l = g_list_first(g_par.alarm_list); 
         alarm_l != NULL; 
         alarm_l = g_list_first(g_par.alarm_list)) {
        l_alarm = alarm_l->data;
        if (l_alarm->temporary && (g_date_time_compare (time_now, l_alarm->alarm_time) < 0)) {
            /* We keep temporary alarms, which have not yet fired.
               Remove the element from the list, but do not loose it. */
            g_par.alarm_list = g_list_remove_link(g_par.alarm_list, alarm_l);
#if 0
            if (!kept_l)
                kept_l = alarm_l;
            else
#endif
            kept_l = g_list_concat(kept_l, alarm_l);
        }
        else { /* get rid of that l_alarm element */
            g_par.alarm_list=g_list_remove(g_par.alarm_list, l_alarm);
            orage_alarm_unref (l_alarm);
        }
    }
    g_date_time_unref (time_now);
    g_list_free(g_par.alarm_list);
    g_par.alarm_list = NULL;
    if (g_list_length(kept_l)) {
        g_par.alarm_list = g_list_concat(g_par.alarm_list, kept_l);
        g_par.alarm_list = g_list_sort(g_par.alarm_list, orage_alarm_order);
    }
}

void alarm_add(alarm_struct *l_alarm)
{
    g_par.alarm_list = g_list_prepend(g_par.alarm_list, l_alarm);
}

/************************************************************/
/* persistent alarms start                                  */
/************************************************************/

static OrageRc *orage_persistent_file_open (void)
{
    gchar *fpath;
    OrageRc *orc;

    fpath = orage_data_file_location(ORAGE_PERSISTENT_ALARMS_DIR_FILE);

    if ((orc = orage_rc_file_open (fpath, TRUE)) == NULL) {
        g_warning ("%s: persistent alarms file open failed.", G_STRFUNC);
    }
    g_free(fpath);

    return(orc);
}

static OrageRc *orage_persistent_file_new (void)
{
    gchar *fpath;
    OrageRc *orc;

    fpath = orage_data_file_location (ORAGE_PERSISTENT_ALARMS_DIR_FILE);

    if ((orc = orage_rc_file_new (fpath)) == NULL)
        g_warning ("%s: persistent alarms file open failed.", G_STRFUNC);

    g_free (fpath);

    return orc;
}

static alarm_struct *alarm_read_next_alarm(OrageRc *orc, GDateTime *gdt)
{
    gint cmp_result;
    alarm_struct *new_alarm;

    new_alarm = orage_alarm_new ();

    new_alarm->uid = orage_rc_get_group(orc);
    new_alarm->alarm_time = orage_rc_get_gdatetime (orc, RC_ALARM_TIME, NULL);
    new_alarm->action_time = orage_rc_get_str(orc, RC_ACTION_TIME, "0000");
    new_alarm->title = orage_rc_get_str(orc, RC_TITLE, NULL);
    new_alarm->description = orage_rc_get_str(orc, RC_DESCRIPTION, NULL);
    new_alarm->persistent = TRUE; /* this must be */
    new_alarm->temporary = orage_rc_get_bool(orc, RC_TEMPORARY, FALSE);
    new_alarm->display_orage = orage_rc_get_bool(orc, RC_DISPLAY_ORAGE, FALSE);

#ifdef HAVE_NOTIFY
    new_alarm->display_notify = orage_rc_get_bool(orc, RC_DISPLAY_NOTIFY, FALSE);
    new_alarm->notify_timeout = orage_rc_get_int(orc, RC_NOTIFY_TIMEOUT, FALSE);
#endif

    new_alarm->audio = orage_rc_get_bool(orc, RC_AUDIO, FALSE);
    new_alarm->sound = orage_rc_get_str(orc, RC_SOUND, NULL);
    new_alarm->repeat_cnt = orage_rc_get_int(orc, RC_REPEAT_CNT, 0);
    new_alarm->repeat_delay = orage_rc_get_int(orc, RC_REPEAT_DELAY, 2);
    new_alarm->procedure = orage_rc_get_bool(orc, RC_PROCEDURE, FALSE);
    new_alarm->cmd = orage_rc_get_str(orc, RC_CMD, NULL);

    /* let's first check if the time has gone so that we need to
     * send that delayed l_alarm or can we just ignore it since it is
     * still in the future */
    cmp_result = g_date_time_compare (gdt, new_alarm->alarm_time);

    if (cmp_result < 0) {
        /* real l_alarm has not happened yet */
        if (new_alarm->temporary)
            /* we need to store this or it will get lost */
            alarm_add(new_alarm);
        else  /* we can ignore this as it will be created again soon */
            orage_alarm_unref (new_alarm);
        return(NULL);
    }

    return(new_alarm);
}

void alarm_read(void)
{
    alarm_struct *new_alarm;
    OrageRc *orc;
    GDateTime *time_now;
    gchar **alarm_groups;
    gint i;

    time_now = g_date_time_new_now_local ();
    orc = orage_persistent_file_open ();
    alarm_groups = orage_rc_get_groups(orc);
    for (i = 0; alarm_groups[i] != NULL; i++) {
        orage_rc_set_group(orc, alarm_groups[i]);
        if ((new_alarm = alarm_read_next_alarm(orc, time_now)) != NULL) {
            create_reminders(new_alarm);
            orage_alarm_unref (new_alarm);
        }
    }
    g_date_time_unref (time_now);
    g_strfreev(alarm_groups);
    orage_rc_file_close(orc);
}

static void alarm_store(gpointer galarm, gpointer par)
{
    const alarm_struct *l_alarm = (const alarm_struct *)galarm;
    OrageRc *orc = (OrageRc *)par;

    if (!l_alarm->persistent)
        return; /* only store persistent alarms */

    orage_rc_set_group(orc, l_alarm->uid);

    orage_rc_put_gdatetime (orc, RC_ALARM_TIME, l_alarm->alarm_time);
    orage_rc_put_str(orc, RC_ACTION_TIME, l_alarm->action_time);
    orage_rc_put_str(orc, RC_TITLE, l_alarm->title);
    orage_rc_put_str(orc, RC_DESCRIPTION, l_alarm->description);
    orage_rc_put_bool(orc, RC_DISPLAY_ORAGE, l_alarm->display_orage);
    orage_rc_put_bool(orc, RC_TEMPORARY, l_alarm->temporary);

#ifdef HAVE_NOTIFY
    orage_rc_put_bool(orc, RC_DISPLAY_NOTIFY, l_alarm->display_notify);
    orage_rc_put_int(orc, RC_NOTIFY_TIMEOUT, l_alarm->notify_timeout);
#endif

    orage_rc_put_bool(orc, RC_AUDIO, l_alarm->audio);
    orage_rc_put_str(orc, RC_SOUND, l_alarm->sound);
    orage_rc_put_int(orc, RC_REPEAT_CNT, l_alarm->repeat_cnt);
    orage_rc_put_int(orc, RC_REPEAT_DELAY, l_alarm->repeat_delay);
    orage_rc_put_bool(orc, RC_PROCEDURE, l_alarm->procedure);
    orage_rc_put_str(orc, RC_CMD, l_alarm->cmd);
}

static void store_persistent_alarms(void)
{
    OrageRc *orc;

    orc = orage_persistent_file_new ();
    g_list_foreach(g_par.alarm_list, alarm_store, orc);
    orage_rc_file_close(orc);
}

/************************************************************/
/* persistent alarms end                                    */
/************************************************************/

#ifdef HAVE_NOTIFY
static void notify_action_open (G_GNUC_UNUSED NotifyNotification *n,
                                G_GNUC_UNUSED const char *action, gpointer par)
{
    GtkWidget *appointment_window;
    alarm_struct *l_alarm = (alarm_struct *)par;

    appointment_window = orage_appointment_window_new_update (l_alarm->uid);
    gtk_window_present (GTK_WINDOW (appointment_window));
}
#endif

static gboolean sound_alarm(gpointer data)
{
    alarm_struct *l_alarm = (alarm_struct *)data;
    GError *error = NULL;
    gboolean status;
    GtkWidget *stop;
#ifdef HAVE_NOTIFY
    gboolean notify_cleanup = FALSE;
#endif

    g_debug ("%s: repeat_cnt=%d", G_STRFUNC, l_alarm->repeat_cnt);

    /* note: -1 loops forever */
    if (l_alarm->repeat_cnt)
    {
        if (l_alarm->active_alarm->sound_active)
            return TRUE;

        status = orage_exec (l_alarm->sound_cmd,
                             &l_alarm->active_alarm->sound_active, &error);
        if (status == FALSE)
        {
            g_warning ("%s: sound command failed (%s) %s", G_STRFUNC,
                       l_alarm->sound, error->message);
#ifdef HAVE_NOTIFY
            notify_cleanup = TRUE;
#endif
        }
        else if (l_alarm->repeat_cnt > 0)
            l_alarm->repeat_cnt--;
    }
    else
    {
        if (l_alarm->display_orage &&
            ((stop = l_alarm->active_alarm->stop_noise_reminder) != NULL))
        {
            gtk_widget_set_sensitive (GTK_WIDGET(stop), FALSE);
        }

#ifdef HAVE_NOTIFY
        notify_cleanup = TRUE;
#endif
        l_alarm->audio = FALSE;
        status = FALSE; /* no more alarms, end timeouts */
    }

#ifdef HAVE_NOTIFY
    if (notify_cleanup)
    {
        g_debug ("%s: notify_cleanup", G_STRFUNC);
        if (l_alarm->display_notify &&
            l_alarm->active_alarm->notify_stop_noise_action)
        {
            g_debug ("%s: create_notify_reminder", G_STRFUNC);
            l_alarm->repeat_cnt = 0;
            /* We need to remove the silence button from notify window. This is
             * not nice method, but it is not possible to access the timeout so
             * we just need to start it from all over.
             */
            notify_notification_close (l_alarm->active_alarm->active_notify,
                                       NULL);
            create_notify_reminder (l_alarm);
        }
    }
#endif

    return(status);
}

static void create_sound_reminder(alarm_struct *l_alarm)
{
    orage_alarm_ref (l_alarm);
    l_alarm->sound_cmd = g_strconcat(g_par.sound_application, " \""
        , l_alarm->sound, "\"", NULL);
    l_alarm->active_alarm->sound_active = FALSE;
    if (l_alarm->repeat_cnt == 0) {
        l_alarm->repeat_cnt++; /* need to do it once */
    }

    g_debug ("%s: sound_cmd='%s'", G_STRFUNC, l_alarm->sound_cmd);
    g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, l_alarm->repeat_delay,
                                sound_alarm, l_alarm,
                                (GDestroyNotify)orage_alarm_unref);
}

#ifdef HAVE_NOTIFY
static gboolean orage_notify_init (void)
{
    if (orage_notify_initted)
        return TRUE;

    if (notify_init (PACKAGE_NAME) == FALSE)
    {
        g_warning ("notify init failed");
        return FALSE;
    }

    orage_notify_initted = TRUE;

    return TRUE;
}

static void notify_closed (G_GNUC_UNUSED NotifyNotification *n, gpointer par)
{
    alarm_struct *l_alarm = (alarm_struct *)par;

    l_alarm->display_notify = FALSE;
    orage_alarm_unref (l_alarm);
}

static void notify_action_silence (G_GNUC_UNUSED NotifyNotification *n,
                                   G_GNUC_UNUSED const char *action
        , gpointer par)
{
    alarm_struct *l_alarm = (alarm_struct *)par;

    l_alarm->repeat_cnt = 0; /* end sound during next  play */
    create_notify_reminder(l_alarm); /* recreate after notify close */
}
#endif

static void create_notify_reminder(alarm_struct *l_alarm) 
{
#ifdef HAVE_NOTIFY
    char heading[250];
    NotifyNotification *n;

    if (orage_notify_init () == FALSE)
        return;

    g_strlcpy(heading, _("Reminder"), sizeof (heading));
    g_strlcat(heading, " ", sizeof (heading));
    /* l_alarm will be unreferenced with notify_closed callback function. */
    orage_alarm_ref (l_alarm);
    if (l_alarm->title)
        g_strlcat(heading, l_alarm->title, sizeof (heading));
    if (l_alarm->action_time) {
        g_strlcat(heading, "\n", sizeof (heading));
        g_strlcat(heading, l_alarm->action_time, sizeof (heading));
    }

    n = notify_notification_new(heading, l_alarm->description, NULL);
    l_alarm->active_alarm->active_notify = n;

    if (l_alarm->notify_timeout == -1)
        notify_notification_set_timeout(n, NOTIFY_EXPIRES_NEVER);
    else if (l_alarm->notify_timeout == 0)
        notify_notification_set_timeout(n, NOTIFY_EXPIRES_DEFAULT);
    else
        notify_notification_set_timeout(n, l_alarm->notify_timeout*1000);

    if (l_alarm->uid)
    {
        notify_notification_add_action(n, "open", _("Open")
                , (NotifyActionCallback)notify_action_open
                , l_alarm, NULL);
    }
    if ((l_alarm->audio) && (l_alarm->repeat_cnt > 1)) {
        notify_notification_add_action(n, "stop", _("Silence")
                , (NotifyActionCallback)notify_action_silence
                , l_alarm, NULL);
        /* this let's sound l_alarm to know that we have notify active with
           end sound button, which needs to be removed when sound ends */
        l_alarm->active_alarm->notify_stop_noise_action = TRUE;
    }
    (void)g_signal_connect(G_OBJECT(n), "closed"
           , G_CALLBACK(notify_closed), l_alarm);

    if (!notify_notification_show(n, NULL)) {
        g_warning ("failed to send notification");
        orage_alarm_unref (l_alarm);
    }

#else
    g_warning ("libnotify not linked in. Can't use notifications.");
    (void)l_alarm;
#endif
}

static void destroy_orage_reminder (G_GNUC_UNUSED GtkWidget *wReminder,
                                    gpointer user_data)
{
    alarm_struct *l_alarm = (alarm_struct *)user_data;

    l_alarm->display_orage = FALSE; /* I am gone */
    orage_alarm_unref (l_alarm);
}

static void on_btStopNoiseReminder_clicked(GtkButton *button
        , gpointer user_data)
{
    alarm_struct *l_alarm = (alarm_struct *)user_data;

    l_alarm->repeat_cnt = 0;
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
}

static void on_btOkReminder_clicked (G_GNUC_UNUSED GtkButton *button,
                                     gpointer user_data)
{
    GtkWidget *wReminder = (GtkWidget *)user_data;

    gtk_widget_destroy(wReminder); /* destroy the specific appointment window */
}

static void on_btOpenReminder_clicked (G_GNUC_UNUSED GtkButton *button,
                                       gpointer user_data)
{
    GtkWidget *appointment_window;
    alarm_struct *l_alarm = (alarm_struct *)user_data;

    appointment_window = orage_appointment_window_new_update (l_alarm->uid);
    gtk_window_present (GTK_WINDOW (appointment_window));
}

static void on_btRecreateReminder_clicked (G_GNUC_UNUSED GtkButton *button,
                                           gpointer user_data)
{
    const alarm_struct *l_alarm = (const alarm_struct *)user_data;
    orage_ddmmhh_hbox_struct *display_data;
    alarm_struct *n_alarm;
    gint days;
    gint hours;
    gint minutes;
    GDateTime *gdt_local;
    GDateTime *gdt_local_d;
    GDateTime *gdt_local_dh;
    GDateTime *gdt_local_dhm;

    display_data = (orage_ddmmhh_hbox_struct *)l_alarm->orage_display_data;
    /* we do not have l_alarm time here */
    n_alarm = orage_alarm_copy (l_alarm);
    n_alarm->temporary = TRUE;

    /* let's count new l_alarm time */
    days = gtk_spin_button_get_value_as_int (
            GTK_SPIN_BUTTON (display_data->spin_dd));
    hours = gtk_spin_button_get_value_as_int (
            GTK_SPIN_BUTTON (display_data->spin_hh));
    minutes = gtk_spin_button_get_value_as_int (
            GTK_SPIN_BUTTON (display_data->spin_mm));
    gdt_local = g_date_time_new_now_local ();
    gdt_local_d = g_date_time_add_days (gdt_local, days);
    gdt_local_dh = g_date_time_add_hours (gdt_local_d, hours);
    gdt_local_dhm = g_date_time_add_minutes (gdt_local_dh, minutes);
    g_date_time_unref (n_alarm->alarm_time);
    n_alarm->alarm_time = gdt_local_dhm;
    g_date_time_unref (gdt_local);
    g_date_time_unref (gdt_local_d);
    g_date_time_unref (gdt_local_dh);
    alarm_add(n_alarm);
    setup_orage_alarm_clock();
    gtk_widget_destroy(display_data->dialog);
}

static void create_orage_reminder(alarm_struct *l_alarm)
{
    GtkWidget *wReminder;
    GtkWidget *vbReminder;
    GtkWidget *lbReminder;
    GtkWidget *daaReminder;
    GtkWidget *btOpenReminder;
    GtkWidget *btStopNoiseReminder;
    GtkWidget *btOkReminder;
    GtkWidget *btRecreateReminder;
    GtkWidget *swReminder;
    GtkWidget *hdReminder;
    GtkWidget *hdtReminder;
    orage_ddmmhh_hbox_struct *ddmmhh_hbox;
    GtkWidget *e_hbox;
    gchar *tmp;

    orage_alarm_ref (l_alarm);

    wReminder = gtk_dialog_new();
    gtk_widget_set_size_request(wReminder, -1, 250);
    gtk_window_set_title(GTK_WINDOW(wReminder),  _("Reminder - Orage"));
    gtk_window_set_position(GTK_WINDOW(wReminder), GTK_WIN_POS_CENTER);
    gtk_window_set_modal(GTK_WINDOW(wReminder), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(wReminder), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(wReminder), TRUE);

    /* Main AREA */
    vbReminder = gtk_dialog_get_content_area (GTK_DIALOG(wReminder));

    hdReminder = gtk_label_new(NULL);
    gtk_label_set_selectable(GTK_LABEL(hdReminder), TRUE);
    tmp = g_markup_printf_escaped("<b>%s</b>", l_alarm->title);
    gtk_label_set_markup(GTK_LABEL(hdReminder), tmp);
    g_free(tmp);
    gtk_box_pack_start(GTK_BOX(vbReminder), hdReminder, FALSE, TRUE, 0);

    hdtReminder = gtk_label_new(NULL);
    gtk_label_set_selectable(GTK_LABEL(hdtReminder), TRUE);
    tmp = g_markup_printf_escaped("<i>%s</i>", l_alarm->action_time);
    gtk_label_set_markup(GTK_LABEL(hdtReminder), tmp);
    g_free(tmp);
    gtk_box_pack_start(GTK_BOX(vbReminder), hdtReminder, FALSE, TRUE, 0);

    swReminder = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swReminder)
            , GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(vbReminder), swReminder, TRUE, TRUE, 5);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swReminder)
            , GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    lbReminder = gtk_label_new(l_alarm->description);
    gtk_label_set_line_wrap(GTK_LABEL(lbReminder), TRUE);
    gtk_label_set_selectable(GTK_LABEL(lbReminder), TRUE);

    gtk_container_add (GTK_CONTAINER (swReminder), lbReminder);

    /* TIME AREA */
    ddmmhh_hbox = (orage_ddmmhh_hbox_struct *)l_alarm->orage_display_data;
    ddmmhh_hbox->spin_dd = gtk_spin_button_new_with_range(0, 100, 1);
    ddmmhh_hbox->spin_dd_label = gtk_label_new(_("days"));
    ddmmhh_hbox->spin_hh = gtk_spin_button_new_with_range(0, 23, 1);
    ddmmhh_hbox->spin_hh_label = gtk_label_new(_("hours"));
    ddmmhh_hbox->spin_mm = gtk_spin_button_new_with_range(0, 59, 5);
    ddmmhh_hbox->spin_mm_label = gtk_label_new(_("mins"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ddmmhh_hbox->spin_mm), 5);
    ddmmhh_hbox->time_hbox = orage_period_hbox_new(TRUE, FALSE
            , ddmmhh_hbox->spin_dd, ddmmhh_hbox->spin_dd_label
            , ddmmhh_hbox->spin_hh, ddmmhh_hbox->spin_hh_label
            , ddmmhh_hbox->spin_mm, ddmmhh_hbox->spin_mm_label);
    /* we want it to the left: */

    e_hbox = gtk_grid_new ();
    gtk_grid_attach_next_to (GTK_GRID (e_hbox), ddmmhh_hbox->time_hbox, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_box_pack_end(GTK_BOX(vbReminder), e_hbox, FALSE, FALSE, 0);
    ddmmhh_hbox->dialog = wReminder;

    /* ACTION AREA: Following two lines change buttons layout in dialog window.
     * GTK4 does not have 'gtk_dialog_get_action_area' function. However,
     * following two lines is safe to remove, then reminder box have GTK defaul
     * buttons layout in reminder dialog.
     */
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    daaReminder = gtk_dialog_get_action_area (GTK_DIALOG (wReminder));
    G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_button_box_set_layout(GTK_BUTTON_BOX(daaReminder), GTK_BUTTONBOX_EXPAND);

    if (l_alarm->uid) {
        btOpenReminder = orage_util_image_button ("document-open", _("_Open"));
        gtk_dialog_add_action_widget(GTK_DIALOG(wReminder), btOpenReminder
                , GTK_RESPONSE_OK);
        g_signal_connect((gpointer) btOpenReminder, "clicked"
                , G_CALLBACK(on_btOpenReminder_clicked), l_alarm);
    }

    btOkReminder = orage_util_image_button ("window-close", _("_Close"));
    gtk_dialog_add_action_widget(GTK_DIALOG(wReminder), btOkReminder
            , GTK_RESPONSE_OK);
    g_signal_connect((gpointer) btOkReminder, "clicked"
            , G_CALLBACK(on_btOkReminder_clicked), wReminder);

    if ((l_alarm->audio) && (l_alarm->repeat_cnt > 1)) {
        btStopNoiseReminder = orage_util_image_button ("process-stop",
                                                       _("_Stop"));
        l_alarm->active_alarm->stop_noise_reminder = btStopNoiseReminder;
        gtk_dialog_add_action_widget(GTK_DIALOG(wReminder)
                , btStopNoiseReminder, GTK_RESPONSE_OK);
        g_signal_connect((gpointer)btStopNoiseReminder, "clicked",
            G_CALLBACK(on_btStopNoiseReminder_clicked), l_alarm);
    }

    btRecreateReminder = orage_util_image_button ("gtk-ok", _("Postpone"));
    gtk_widget_set_tooltip_text(btRecreateReminder
            , _("Remind me again after the specified time"));
    gtk_dialog_add_action_widget(GTK_DIALOG(wReminder)
            , btRecreateReminder, GTK_RESPONSE_OK);
    g_signal_connect((gpointer) btRecreateReminder, "clicked"
            , G_CALLBACK(on_btRecreateReminder_clicked), l_alarm);

    g_signal_connect(G_OBJECT(wReminder), "destroy",
        G_CALLBACK(destroy_orage_reminder), l_alarm);
    gtk_widget_show_all(wReminder);
}

static void create_procedure_reminder(alarm_struct *l_alarm)
{
    GDateTime *gdt;
#if 0
    gboolean status, active; / * active not used * /
    GError *error = NULL;
#endif
    gchar *cmd, *atime, *sep = NULL;
    gint status;

    orage_alarm_ref (l_alarm);
#if 0
    status = orage_exec(l_alarm->cmd, &active, &error);
#endif
    cmd = g_strconcat(l_alarm->cmd, " &", NULL);

    cmd = orage_replace_text(cmd, "<&T>", l_alarm->title);

    cmd = orage_replace_text(cmd, "<&D>", l_alarm->description);

    if (l_alarm->alarm_time)
        gdt = g_date_time_ref (l_alarm->alarm_time);
    else
        gdt = g_date_time_new_now_local ();

    atime = orage_gdatetime_to_i18_time (gdt, FALSE);
    g_date_time_unref (gdt);
    cmd = orage_replace_text(cmd, "<&AT>", atime);
    g_free(atime);

    /* l_alarm->action_time format is <start-time - end-time>
    and times are in user format already. Problem is if that format contain
    string " - " already, but let's hope not.  */
    if (l_alarm->action_time != NULL)
        sep = strstr(l_alarm->action_time, " - ");

    if (sep) { /* we should always have this */
        if (strstr(sep+1, " - ")) {
            /* this is problem as now we have more than one separator.
               We could try to find the middle one using strlen, but as
               there probably is not this kind of issues, it is not worth
               the trouble */
            g_message ("%s: <&ST>/<&ET> string conversion failed (%s)",
                       G_STRFUNC, l_alarm->action_time);
        }
        else {
            sep[0] = '\0'; /* temporarily to end start-time string */
            cmd = orage_replace_text(cmd, "<&ST>", l_alarm->action_time);
            sep[0] = ' '; /* back to real value */

            sep += strlen(" - "); /* points now to the end-time */
            cmd = orage_replace_text(cmd, "<&ET>", sep);
        }
    }
    else 
        g_message ("%s: <&ST>/<&ET> string conversion failed 2 (%s)", G_STRFUNC,
                   l_alarm->action_time);

    g_debug ("%s: cmd='%s'", G_STRFUNC, cmd);
    status = system(cmd);
    if (status)
    {
        g_warning ("%s: cmd failed(%s)->(%s) status:%d",
                   G_STRFUNC, l_alarm->cmd, cmd, status);
    }

    orage_alarm_unref (l_alarm);
    g_free(cmd);
}

void create_reminders(alarm_struct *l_alarm)
{
    if (l_alarm->audio && l_alarm->sound)
        create_sound_reminder (l_alarm);

    if (l_alarm->display_orage)
        create_orage_reminder (l_alarm);

    if (l_alarm->display_notify)
        create_notify_reminder (l_alarm);

    if (l_alarm->procedure && l_alarm->cmd)
        create_procedure_reminder (l_alarm);
}

static void reset_orage_day_change(gboolean changed)
{
    GDateTime *gdt;
    gint secs_left;

    if (changed) { /* date was change, need to count next change time */
        gdt = g_date_time_new_now_local ();
        secs_left = 60 * 60 * (24 - g_date_time_get_hour (gdt))
                  - 60 * g_date_time_get_minute (gdt)
                  - g_date_time_get_second (gdt);
        g_date_time_unref (gdt);
    }
    else { /* the change did not happen. Need to try again asap. */
        secs_left = 1;
    }
    g_par.day_timer = g_timeout_add_seconds (secs_left, orage_day_change, NULL);
}

/* fire after the date has changed and setup the icon 
 * and change the date in the mainwindow.
 *
 * TODO: all xxx_year, xxx_month and xxx_day variables can be replaced with
 * GDate
 */
gboolean orage_day_change(gpointer user_data)
{
    OrageApplication *app;
    GtkCalendar *calendar;
    GDateTime *gdt;
    gint year;
    gint month;
    gint day;
    static gint previous_year=0, previous_month=0, previous_day=0;
    guint selected_year=0, selected_month=0, selected_day=0;
    gint current_year=0, current_month=0, current_day=0;

    gdt = g_date_time_new_now_local ();
    g_date_time_get_ymd (gdt, &year, &month, &day);
    /* See if the day just changed.  Note that when we are called first time we
     * always have day change.  If user_data is not NULL, we also force day
     * change.
     */
    if (user_data || previous_day != day || previous_month != month || previous_year != year)
    {
        if (user_data) {
            if (g_par.day_timer) { /* need to stop it if running */
                g_source_remove(g_par.day_timer);
                g_par.day_timer = 0;
            }
        }
        current_year  = year;
        current_month = month;
        current_day   = day;
        /* Get the selected date from calendar */
        app = ORAGE_APPLICATION (g_application_get_default ());
        calendar = orage_window_get_calendar (
            ORAGE_WINDOW (orage_application_get_window (app)));

        gtk_calendar_get_date (calendar, &selected_year, &selected_month,
                               &selected_day);
        selected_month++;
        if ((gint)selected_year == previous_year
        && (gint)selected_month == previous_month
        && (gint)selected_day == previous_day) {
            /* previous day was indeed selected,
               keep it current automatically */
            orage_select_date (calendar, gdt);
        }
        previous_year  = current_year;
        previous_month = current_month;
        previous_day   = current_day;
#ifdef HAVE_X11_TRAY_ICON
        if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
            orage_refresh_trayicon ();
#endif
        xfical_alarm_build_list(TRUE);  /* new l_alarm list when date changed */
        reset_orage_day_change(TRUE);   /* setup for next time */
    }
    else { 
        /* we should very seldom come here since we schedule us to happen
         * after the date has changed, but it is possible that the clock
         * in this machine is not accurate and we end here too early.
         * */
        reset_orage_day_change(FALSE);
    }

    g_date_time_unref (gdt);

    return(FALSE); /* we started new timer, so we end here */
}

/* check and raise alarms if there are any */
static gboolean orage_alarm_clock (G_GNUC_UNUSED gpointer user_data)
{
    GList *alarm_l;
    alarm_struct *cur_alarm;
    gboolean alarm_raised=FALSE;
    gboolean more_alarms=TRUE;
    GDateTime *time_now;

    time_now = g_date_time_new_now_local ();
    /* Check if there are any alarms to show */
    for (alarm_l = g_list_first(g_par.alarm_list);
         alarm_l != NULL && more_alarms;
         alarm_l = g_list_next(alarm_l)) {
        /* remember that it is sorted list */
        cur_alarm = orage_alarm_ref ((alarm_struct *)alarm_l->data);
        if (g_date_time_compare (time_now, cur_alarm->alarm_time) > 0) {
            create_reminders(cur_alarm);
            alarm_raised = TRUE;
        }
        else /* sorted so scan can be stopped */
            more_alarms = FALSE;

        orage_alarm_unref (cur_alarm);
        }

    g_date_time_unref (time_now);

    if (alarm_raised)  /* at least one l_alarm processed, need new list */
        xfical_alarm_build_list(FALSE); /* this calls reset_orage_alarm_clock */
    else
        reset_orage_alarm_clock(); /* need to setup next timer */

    return(FALSE); /* only once */
}

static void reset_orage_alarm_clock(void)
{
    GList *alarm_l;
    alarm_struct *cur_alarm;
    GTimeSpan t_diff;
    gint secs_to_alarm;
    GDateTime *gdt_now;

    if (g_par.alarm_timer) { /* need to stop it if running */
        g_source_remove(g_par.alarm_timer);
        g_par.alarm_timer = 0;
    }
    if (g_par.alarm_list) { /* we have alarms */
        alarm_l = g_list_first(g_par.alarm_list);
        cur_alarm = orage_alarm_ref ((alarm_struct *)alarm_l->data);
        gdt_now = g_date_time_new_now_local ();

        /* Let's find out how much time we have until l_alarm happens. Adding
         * 999999 to time difference before dividing is for ceiling value.
         */
        t_diff = g_date_time_difference (cur_alarm->alarm_time, gdt_now);
        g_date_time_unref (gdt_now);
        orage_alarm_unref (cur_alarm);

        t_diff = (t_diff + 999999) / 1000000;
        /* alarm needs to come a bit later */
        secs_to_alarm = (gint)t_diff + 1;
        if (secs_to_alarm < 1) /* rare, but possible */
            secs_to_alarm = 1;
        g_par.alarm_timer = g_timeout_add_seconds(secs_to_alarm
                , orage_alarm_clock, NULL);
    }
}

/* refresh trayicon tooltip once per minute */
static gboolean orage_tooltip_update (G_GNUC_UNUSED gpointer user_data)
{
#ifdef HAVE_X11_TRAY_ICON
    GDateTime *gdt;
    GList *alarm_l;
    alarm_struct *cur_alarm;
    gboolean more_alarms=TRUE;
    GString *tooltip=NULL, *tooltip_highlight_helper=NULL;
    gint alarm_cnt=0;
    gint tooltip_alarm_limit=5;
    gint hour, minute;
    gint dd, hh, min;
    gchar *tmp;

    if (!(g_par.trayIcon 
    && orage_status_icon_is_embedded ((GtkStatusIcon *)g_par.trayIcon))) {
           /* no trayicon => no need to update the tooltip */
        return(FALSE);
    }
    gdt = g_date_time_new_now_local ();
    tooltip = g_string_new(_("Next active alarms:"));
    g_string_prepend(tooltip, "<span foreground=\"blue\" weight=\"bold\" underline=\"single\">");
    g_string_append(tooltip, " </span>");
    /* Check if there are any alarms to show */
    for (alarm_l = g_list_first(g_par.alarm_list);
         alarm_l != NULL && more_alarms;
         alarm_l = g_list_next(alarm_l)) {
        /* remember that it is sorted list */
        cur_alarm = orage_alarm_ref ((alarm_struct *)alarm_l->data);
        if (alarm_cnt < tooltip_alarm_limit) {
            hour = g_date_time_get_hour (cur_alarm->alarm_time);
            minute = g_date_time_get_minute (cur_alarm->alarm_time);
            dd = orage_gdatetime_days_between (gdt, cur_alarm->alarm_time);
            hh = hour - g_date_time_get_hour (gdt);
            min = minute - g_date_time_get_minute (gdt);
            if (min < 0) {
                min += 60;
                hh -= 1;
            }
            if (hh < 0) {
                hh += 24;
                dd -= 1;
            }

            g_string_append(tooltip, "<span weight=\"bold\">");
            tooltip_highlight_helper = g_string_new(" </span>");
            if (cur_alarm->temporary) { /* let's add a small mark */
                g_string_append_c(tooltip_highlight_helper, '[');
            }
            tmp = cur_alarm->title 
                ? g_markup_escape_text(cur_alarm->title
                        , strlen(cur_alarm->title))
                : g_strdup(_("No title defined"));
            g_string_append_printf(tooltip_highlight_helper, "%s", tmp);
            g_free(tmp);
            if (cur_alarm->temporary) { /* let's add a small mark */
                g_string_append_c(tooltip_highlight_helper, ']');
            }
            g_string_append_printf(tooltip, 
                    _("\n%02d d %02d h %02d min to: %s"),
                    dd, hh, min, tooltip_highlight_helper->str);
            g_string_free(tooltip_highlight_helper, TRUE);
            alarm_cnt++;
        }
        else /* sorted so scan can be stopped */
            more_alarms = FALSE;

        orage_alarm_unref (cur_alarm);
    }

    g_date_time_unref (gdt);

    if (alarm_cnt == 0)
        g_string_append_printf(tooltip, _("\nNo active alarms found"));

    orage_status_icon_set_tooltip_markup ((GtkStatusIcon *)g_par.trayIcon, tooltip->str);

    g_string_free(tooltip, TRUE);

    return(TRUE);
#else
    return FALSE;
#endif
}

/* start timer to fire every minute to keep tooltip accurate */
static gboolean start_orage_tooltip_update (G_GNUC_UNUSED gpointer user_data)
{
    if (g_par.tooltip_timer) { /* need to stop it if running */
        g_source_remove(g_par.tooltip_timer);
    }

    orage_tooltip_update(NULL);
    g_par.tooltip_timer = g_timeout_add_seconds(60
            , (GSourceFunc) orage_tooltip_update, NULL);

    return(FALSE);
}

/* adjust the call to happen when minute changes */
static gboolean reset_orage_tooltip_update (G_GNUC_UNUSED gpointer user_data)
{
    GDateTime *gdt;
    gint secs_left;

    gdt = g_date_time_new_now_local ();
    secs_left = 60 - g_date_time_get_second (gdt);
    g_date_time_unref (gdt);

    if (secs_left > 2) 
        orage_tooltip_update(NULL);
    /* FIXME: do not start this, if it is already in progress.
     * Minor thing and does not cause any real trouble and happens
     * only when appoinments are updated in less than 1 minute apart. 
     * Perhaps not worth fixing. 
     * Should add another timer or static time to keep track of this */
    g_timeout_add_seconds(secs_left
            , (GSourceFunc) start_orage_tooltip_update, NULL);

    return(FALSE);
}

void setup_orage_alarm_clock(void)
{
    /* order the list */
    g_par.alarm_list = g_list_sort(g_par.alarm_list, orage_alarm_order);
    reset_orage_alarm_clock();
    store_persistent_alarms(); /* keep track of alarms when orage is down */
    /* We need to use timer since for some reason it does not work if we
     * do it here directly. Ugly, I know, but it works. */
    g_timeout_add_seconds (1, reset_orage_tooltip_update, NULL);
}

void orage_notify_uninit (void)
{
#ifdef HAVE_NOTIFY
    if (orage_notify_initted && notify_is_initted ())
        notify_uninit ();
#endif
}
