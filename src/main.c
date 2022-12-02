/*      Orage - Calendar and alarm handler
 *
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
       Free Software Foundation
       51 Franklin Street, 5th Floor
       Boston, MA 02110-1301 USA

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>

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
#include <time.h>

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>

#include <libxfce4util/libxfce4util.h>

#define ORAGE_MAIN  "orage"

#include "orage-i18n.h"
#include "orage-css.h"
#include "orage-dbus-client.h"
#include "orage-dbus-object.h"
#include "functions.h"
#include "mainbox.h"
#include "reminder.h"
#include "ical-code.h"
#include "tray_icon.h"
#include "parameters.h"
#include "interface.h"

#define HINT_ADD 'a'
#define HINT_EXPORT 'x'
#define HINT_IMPORT 'i'
#define HINT_REMOVE 'r'

typedef struct
{
    gboolean toggle_option;
    gboolean preferences_option;
} AppOptions;

static GtkApplication *orage_app;
static AppOptions app_options;

#if 0
static gboolean resume_after_sleep (G_GNUC_UNUSED gpointer user_data)
{
    g_message ("Resuming after sleep");
    alarm_read();
    orage_day_change(&g_par);

    return(FALSE); /* only once */
}

static void resuming_cb (G_GNUC_UNUSED DBusGProxy *proxy,
                         G_GNUC_UNUSED gpointer user_data)
{
    g_message ("Resuming");
    /* we need this delay to prevent updating tray icon too quickly when
       the normal code handles it also */
    g_timeout_add_seconds(2, (GSourceFunc) resume_after_sleep, NULL);
}

static void handle_resuming(void)
{
    DBusGConnection *connection;
    GError *error = NULL;
    DBusGProxy *proxy;

    connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
    if (connection) {
       proxy = dbus_g_proxy_new_for_name(connection, "org.freedesktop.UPower"
               , "/org/freedesktop/UPower", "org.freedesktop.UPower");
       if (proxy) {
           dbus_g_proxy_add_signal(proxy, "Resuming", G_TYPE_INVALID);
           dbus_g_proxy_connect_signal(proxy, "Resuming"
                   , G_CALLBACK(resuming_cb), NULL, NULL);
       }
       else {
           g_warning("Failed to create proxy object");
       }
    }
    else {
        g_warning("Failed to connect to D-BUS daemon: %s", error->message);
    }
}
#endif

static gboolean keep_tidy(void)
{
#ifdef HAVE_ARCHIVE
    /* move old appointment to other file to keep the active
       calendar file smaller and faster */
    xfical_archive();
#endif

    write_parameters();
    return(TRUE);
}

static gboolean mWindow_delete_event_cb (G_GNUC_UNUSED GtkWidget *widget,
                                         G_GNUC_UNUSED GdkEvent *event,
                                         CalWin *cal)
{
    if (g_par.close_means_quit) {
        orage_quit ();
    }
    else {
        gtk_widget_hide (cal->mWindow);
    }

    return(TRUE);
}

static void raise_window(void)
{
    CalWin *cal = (CalWin *)g_par.xfcal;

    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move(GTK_WINDOW(cal->mWindow)
                , g_par.pos_x, g_par.pos_y);
    if (g_par.select_always_today)
        orage_select_today(GTK_CALENDAR(cal->mCalendar));
    if (g_par.set_stick)
        gtk_window_stick(GTK_WINDOW(cal->mWindow));
    gtk_window_set_keep_above(GTK_WINDOW(cal->mWindow)
            , g_par.set_ontop);
    gtk_window_present(GTK_WINDOW(cal->mWindow));
}

static void print_version(void)
{
    g_print(_("\tThis is %s version %s\n\n")
            , PACKAGE, VERSION);
    g_print(_("\tReleased under the terms of the GNU General Public License.\n"));
    g_print(_("\tCompiled against GTK+-%d.%d.%d, ")
            , GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
    g_print(_("using GTK+-%d.%d.%d.\n")
            , gtk_major_version, gtk_minor_version, gtk_micro_version);
    g_print(_("\tUsing DBUS for import.\n"));
#ifdef HAVE_NOTIFY
    g_print(_("\tUsing libnotify.\n"));
#else
    g_print(_("\tNot using libnotify.\n"));
#endif
#ifdef HAVE_ARCHIVE
    g_print(_("\tUsing automatic archiving.\n"));
#else
    g_print(_("\tNot using archiving.\n"));
#endif
#ifdef HAVE_LIBXFCE4UI
    g_print(_("\tUsing libxfce4ui: yes\n"));
#else
    g_print(_("\tUsing libxfce4ui: no\n"));
#endif
    g_print("\n");
}

static void startup (G_GNUC_UNUSED GtkApplication *app,
                     G_GNUC_UNUSED gpointer user_data)
{
    /* init i18n = nls to use gettext */
    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
    register_css_provider ();
    orage_dbus_start ();
    read_parameters ();
}

static void activate (GtkApplication *app, AppOptions *app_options)
{
    GList *list;

    list = gtk_application_get_windows (app);

    if (list)
    {
        if (gtk_widget_get_visible (GTK_WIDGET (list->data)) &&
            app_options->toggle_option)
        {
            write_parameters ();
            gtk_widget_hide (GTK_WIDGET (list->data));
        }
        else
            raise_window ();
    }
    else
    {
        g_par.xfcal = g_new (CalWin, 1);

        /* Create the main window */
        ((CalWin *)g_par.xfcal)->mWindow = gtk_application_window_new (app);

        g_signal_connect((gpointer) ((CalWin *)g_par.xfcal)->mWindow, "delete_event"
                , G_CALLBACK(mWindow_delete_event_cb), (gpointer)g_par.xfcal);

        build_mainWin();
        set_parameters();
        if (g_par.start_visible)
        {
            gtk_widget_show(((CalWin *)g_par.xfcal)->mWindow);
        }
        else if (g_par.start_minimized)
        {
            gtk_window_iconify(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow));
            gtk_widget_show(((CalWin *)g_par.xfcal)->mWindow);
        }
        else
        {
            /* hidden */
            gtk_widget_realize(((CalWin *)g_par.xfcal)->mWindow);
            gtk_widget_hide(((CalWin *)g_par.xfcal)->mWindow);
        }

        alarm_read();
        orage_day_change(NULL); /* first day change after we start */
        mCalendar_month_changed_cb (
                (GtkCalendar *)((CalWin *)g_par.xfcal)->mCalendar, NULL);

        /* start monitoring external file updates */
        g_timeout_add_seconds(30, (GSourceFunc)orage_external_update_check, NULL);
#if 0
        /* day change after resuming */
        handle_resuming();
#endif
    }

    if (app_options->preferences_option)
        show_parameters ();
}

static void shutdown (G_GNUC_UNUSED GtkApplication *app,
                      G_GNUC_UNUSED gpointer user_data)
{
    g_debug ("%s", G_STRFUNC);
    keep_tidy ();
}

static void open (G_GNUC_UNUSED GApplication *application,
                  GFile **files,
                  gint n_files,
                  const gchar *hint)
{
    gchar **hint_array;
    gint i;
    gchar *file;
    gchar *file_name;
    gint export_type;
    gboolean foreign_file_read_only;

    for (i = 0; i < n_files; i++)
    {
        switch (hint[0])
        {
            case HINT_ADD:
                file = g_file_get_path (files[i]);
                hint_array = g_strsplit (hint, ":", 3);
                file_name = NULL;

                if (hint_array[1])
                {
                    foreign_file_read_only =
                            (g_ascii_strcasecmp (hint_array[1], "RW") != 0) &&
                            (g_ascii_strcasecmp (hint_array[1], "READWRITE") != 0);

                    if (hint_array[2])
                        file_name = g_strdup (hint_array[2]);
                }
                else
                    foreign_file_read_only = TRUE;

                if (file_name == NULL)
                    file_name = g_file_get_basename (files[i]);

                g_debug ("add foreign file='%s', file_name='%s', ro=%d",
                         file, file_name, foreign_file_read_only);

                if (orage_foreign_file_add (file, foreign_file_read_only, file_name))
                    g_message ("add done, foreign file=%s", file);
                else
                    g_warning ("add failed, foreign file=%s", file);

                g_free (file_name);
                g_free (file);
                g_strfreev (hint_array);
                break;

            case HINT_EXPORT:
                file = g_file_get_path (files[i]);
                hint_array = g_strsplit (hint, ":", 2);
                export_type = hint_array[1] ? 1 : 0;
                g_debug ("exporting to='%s', uids='%s'", file, hint_array[1]);

                if (xfical_export_file (file, export_type, hint_array[1]))
                    g_message ("export done to file=%s", file);
                else
                    g_warning ("export failed file=%s", file);

                g_free (file);
                g_strfreev (hint_array);
                break;

            case HINT_IMPORT:
                file = g_file_get_path (files[i]);
                g_debug ("import, file=%s", file);

                if (xfical_import_file (file))
                    g_message ("import done, file=%s", file);
                else
                    g_warning ("import failed, file=%s", file);

                g_free (file);
                break;

            case HINT_REMOVE:
                file = g_file_get_path (files[i]);

                g_debug ("remove foreign, file=%s", file);
                if (orage_foreign_file_remove (file))
                    g_message ("remove done, foreign file=%s", file);
                else
                    g_warning ("remove failed, foreign file=%s", file);

                g_free (file);

                break;

            default:
                g_assert_not_reached ();
                break;
        }
    }

    /* Note: when doing a longer-lasting action here that returns to the
     * mainloop, you should use g_application_hold() and g_application_release()
     * to keep the application alive until the action is completed.
     */
}

static gint handle_local_options (G_GNUC_UNUSED GApplication *application,
                                  GVariantDict *options,
                                  G_GNUC_UNUSED gpointer user_data)
{
    if (g_variant_dict_contains (options, "version"))
    {
        print_version ();
        return EXIT_SUCCESS;
    }

    return -1;
}

static gint command_line (GApplication *application,
                          GApplicationCommandLine *cmdline,
                          AppOptions *app_options)
{
    GFile **files;
    GFile *file;
    const gchar **filenames = NULL;
    const gchar *file_name;
    gchar **str_array;
    gchar *hint;
    gchar key[2] = {'\0'};
    GVariantDict *options;
    gint n_files;
    gint n;

    options = g_application_command_line_get_options_dict (cmdline);

    if (g_variant_dict_contains (options, "preferences"))
        app_options->preferences_option = TRUE;

    if (g_variant_dict_contains (options, "toggle"))
        app_options->toggle_option = TRUE;

    if (g_variant_dict_lookup (options, "add-foreign", "^&ay", &file_name))
    {
        str_array = g_strsplit (file_name, ":", 2);
        key[0] = HINT_ADD;
        hint = g_strjoin (":", key, str_array[1], NULL);
        g_strfreev (str_array);

        file = g_application_command_line_create_file_for_arg (cmdline,
                                                               str_array[0]);
        g_application_open (application, &file, 1, hint);
        g_free (hint);
        g_object_unref (file);
    }

    if (g_variant_dict_lookup (options, "remove-foreign", "^&ay", &file_name))
    {
        file = g_application_command_line_create_file_for_arg (cmdline,
                                                               file_name);
        key[0] = HINT_REMOVE;
        g_application_open (application, &file, 1, key);
        g_object_unref (file);
    }

    if (g_variant_dict_lookup (options, "export", "^&ay", &file_name))
    {
        str_array = g_strsplit (file_name, ":", 2);
        key[0] = HINT_EXPORT;
        hint = g_strjoin (":", key, str_array[1], NULL);
        g_strfreev (str_array);

        file = g_application_command_line_create_file_for_arg (cmdline,
                                                               str_array[0]);
        g_application_open (application, &file, 1, hint);
        g_free (hint);
        g_object_unref (file);
    }

    g_variant_dict_lookup (options, G_OPTION_REMAINING, "^a&ay", &filenames);

    if (filenames != NULL && (n_files = g_strv_length ((gchar **)filenames)) > 0)
    {
        files = g_new (GFile *, n_files);

        for (n = 0; n < n_files; n++)
        {
            file = g_application_command_line_create_file_for_arg (cmdline,
                                                                   filenames[n]);
            files[n] = file;
        }

        key[0] = HINT_IMPORT;
        g_application_open (application, files, n_files, key);

        for (n = 0; n < n_files; n++)
            g_object_unref (files[n]);

        g_free (files);
    }

    g_application_activate (application);

    return EXIT_SUCCESS;
}

static void g_application_init_cmd_parameters (GApplication *app)
{
    const GOptionEntry cmd_params[] =
    {
        {
            .long_name = "version",
            .short_name = 'v',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_NONE,
            .arg_data = NULL,
            .description = _("Show version of orage"),
            .arg_description = NULL,
        },
        {
            .long_name = "preferences",
            .short_name = 'p',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_NONE,
            .arg_data = NULL,
            .description = _("Show preferences form"),
            .arg_description = NULL,
        },
        {
            .long_name = "toggle",
            .short_name = 't',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_NONE,
            .arg_data = NULL,
            .description = _("Make Orage visible/unvisible"),
            .arg_description = NULL,
        },
        {
            .long_name = "add-foreign",
            .short_name = 'a',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_FILENAME,
            .arg_data = NULL,
            .description = _("Add a foreign file"),
            .arg_description = "<file>:[RW]:[name]",
        },
        {
            .long_name = "remove-foreign",
            .short_name = 'r',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_FILENAME,
            .arg_data = NULL,
            .description = "Remove a foreign file",
            .arg_description = "<file>",
        },
        {
            .long_name = "export",
            .short_name = 'e',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_FILENAME,
            .arg_data = NULL,
            .description = _("Export appointments from Orage to file"),
            .arg_description = "<file>:[appointment...]",
        },
        {
            .long_name = G_OPTION_REMAINING,
            .short_name = '\0',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_FILENAME_ARRAY,
            .arg_data = NULL,
            .description = NULL,
            .arg_description = "[files...]",
        },
        {
            NULL
        }
    };

    g_application_add_main_option_entries (app, cmd_params);
}

static void close_cb (G_GNUC_UNUSED int s)
{
    orage_quit ();
}

void orage_toggle_visible (void)
{
    GList *list;

    list = gtk_application_get_windows (orage_app);

    g_return_if_fail (list != NULL);

    if (gtk_widget_get_visible (GTK_WIDGET (list->data)))
    {
        write_parameters ();
        gtk_widget_hide (GTK_WIDGET (list->data));
    }
    else
        raise_window ();
}

void orage_quit (void)
{
    g_application_quit (G_APPLICATION (orage_app));
}

int main (int argc, char **argv)
{
    int status;
    struct sigaction sig_int_handler;

    sig_int_handler.sa_handler = close_cb;
    sigemptyset (&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;

    orage_app = gtk_application_new ("org.xfce.orage",
                                     G_APPLICATION_HANDLES_COMMAND_LINE |
                                     G_APPLICATION_HANDLES_OPEN);
    g_signal_connect (orage_app, "startup", G_CALLBACK (startup), NULL);
    g_signal_connect (orage_app, "activate", G_CALLBACK (activate), &app_options);
    g_signal_connect (orage_app, "shutdown", G_CALLBACK (shutdown), NULL);
    g_signal_connect (orage_app, "open", G_CALLBACK (open), NULL);
    g_signal_connect (orage_app, "handle-local-options", G_CALLBACK (handle_local_options), NULL);
    g_signal_connect (orage_app, "command-line", G_CALLBACK (command_line), &app_options);
    g_application_init_cmd_parameters (G_APPLICATION (orage_app));

    sigaction (SIGINT, &sig_int_handler, NULL);
    status = g_application_run (G_APPLICATION (orage_app), argc, argv);
    g_object_unref (orage_app);

    return status;
}
