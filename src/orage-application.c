/*
 * Copyright (c) 2023 Erkki Moorits
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

#include "orage-application.h"

#include "reminder.h"
#include "parameters.h"
#include "orage-i18n.h"
#include "orage-css.h"
#include "mainbox.h"
#include "interface.h"
#include "ical-code.h"
#include "functions.h"
#include <libxfce4util/libxfce4util.h>
#include <gtk/gtk.h>
#include <glib-2.0/gio/gapplication.h>

#ifdef ENABLE_SYNC
#include "orage-sync-ext-command.h"
#include "orage-task-runner.h"
#endif

#define HINT_ADD 'a'
#define HINT_EXPORT 'x'
#define HINT_IMPORT 'i'
#define HINT_REMOVE 'r'
#define HINT_OPEN 'o'

#define LOGIND_BUS_NAME "org.freedesktop.login1"
#define LOGIND_IFACE_NAME "org.freedesktop.login1.Manager"
#define LOGIND_OBJ_PATH "/org/freedesktop/login1"

struct _OrageApplication
{
    GtkApplication parent;
    GDBusConnection *connection;
    OrageTaskRunner *sync;
    guint prepare_for_sleep_id;
    gboolean toggle_option;
    gboolean preferences_option;
};

G_DEFINE_TYPE (OrageApplication, orage_application, GTK_TYPE_APPLICATION)

static gboolean window_delete_event_cb (G_GNUC_UNUSED GtkWidget *widget,
                                        G_GNUC_UNUSED GdkEvent *event,
                                        CalWin *cal)
{
    if (g_par.close_means_quit)
        orage_quit ();
    else
        gtk_widget_hide (cal->mWindow);

    return TRUE;
}

static gboolean resuming_after_delay (G_GNUC_UNUSED gpointer user_data)
{
    g_message ("resuming after sleep");

    alarm_read ();
    orage_day_change (&g_par);

    return FALSE;
}

static void resuming_cb (G_GNUC_UNUSED GDBusConnection *connection,
                         const gchar *sender_name,
                         const gchar *object_path,
                         const gchar *interface_name,
                         const gchar *signal_name,
                         GVariant *parameters,
                         G_GNUC_UNUSED gpointer user_data)
{
    gboolean prepare_for_sleep;

    g_debug ("%s: sender_name='%s', object_path='%s', %s.%s %s", G_STRFUNC,
             sender_name, object_path, interface_name,
             signal_name, g_variant_print (parameters, TRUE));

    if (!g_variant_check_format_string (parameters, "(b)", FALSE))
    {
        g_warning ("received incorrect parameter for PrepareForSleep signal");
        return;
    }

    g_variant_get (parameters, "(b)", &prepare_for_sleep);

    if (prepare_for_sleep == FALSE)
    {
        g_debug ("received resuming signal");

        /* We need this delay to prevent updating tray icon too quickly when the
         * normal code handles it also.
         */
        g_timeout_add_seconds (2, resuming_after_delay, NULL);
    }
}

static void resuming_handler_register (OrageApplication *self)
{
    GError *error = NULL;

    self->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (self->connection == NULL)
    {
        g_warning ("failed to connect to the D-BUS session bus: %s",
                   error->message);
        g_error_free (error);
        return;
    }

    self->prepare_for_sleep_id = g_dbus_connection_signal_subscribe (
            self->connection,
            LOGIND_BUS_NAME,
            LOGIND_IFACE_NAME,
            "PrepareForSleep",
            LOGIND_OBJ_PATH,
            NULL,
            G_DBUS_SIGNAL_FLAGS_NONE,
            resuming_cb,
            NULL,
            NULL);

    g_debug ("%s: prepare_for_sleep_id=%d", G_STRFUNC,
             self->prepare_for_sleep_id);
}

static void resuming_handler_unregister (OrageApplication *self)
{
    g_debug ("%s: prepare_for_sleep_id=%d", G_STRFUNC,
             self->prepare_for_sleep_id);

    if (self->prepare_for_sleep_id)
    {
        g_dbus_connection_signal_unsubscribe (self->connection,
                                              self->prepare_for_sleep_id);
    }

    if (self->connection)
        g_object_unref (self->connection);
}

static void print_version (void)
{
    g_print (_("\tThis is %s version %s\n\n")
            , PACKAGE, VERSION);
    g_print (_("\tReleased under the terms of the GNU General Public License.\n"));
    g_print (_("\tCompiled against GTK+-%d.%d.%d, ")
            , GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
    g_print (_("using GTK+-%d.%d.%d.\n")
            , gtk_major_version, gtk_minor_version, gtk_micro_version);
#ifdef HAVE_NOTIFY
    g_print (_("\tUsing libnotify.\n"));
#else
    g_print (_("\tNot using libnotify.\n"));
#endif
#ifdef HAVE_ARCHIVE
    g_print (_("\tUsing automatic archiving.\n"));
#else
    g_print (_("\tNot using archiving.\n"));
#endif
#ifdef HAVE_LIBXFCE4UI
    g_print (_("\tUsing libxfce4ui: yes\n"));
#else
    g_print (_("\tUsing libxfce4ui: no\n"));
#endif
    g_print ("\n");
}

#ifdef ENABLE_SYNC
static void load_sync_conf (OrageTaskRunner *sync)
{
    guint i;

    for (i = 0; i < (guint)g_par.sync_source_count; i++)
    {
        orage_task_runner_add (sync, orage_sync_ext_command,
                               &g_par.sync_conf[i]);
    }
}
#endif

static void raise_window (void)
{
    CalWin *cal = (CalWin *)g_par.xfcal;

    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move (GTK_WINDOW (cal->mWindow), g_par.pos_x, g_par.pos_y);

    if (g_par.select_always_today)
        orage_select_today (GTK_CALENDAR (cal->mCalendar));

    if (g_par.set_stick)
        gtk_window_stick (GTK_WINDOW (cal->mWindow));

    gtk_window_set_keep_above (GTK_WINDOW (cal->mWindow), g_par.set_ontop);
    gtk_window_present (GTK_WINDOW (cal->mWindow));
}

static void orage_application_startup (GApplication *app)
{
#ifdef ENABLE_SYNC
    OrageApplication *self = ORAGE_APPLICATION (app);
#endif

    G_APPLICATION_CLASS (orage_application_parent_class)->startup (app);
    
    /* init i18n = nls to use gettext */
    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
    register_css_provider ();
    read_parameters ();
#ifdef ENABLE_SYNC
    self->sync = g_object_new (ORAGE_TASK_RUNNER_TYPE, NULL);
    load_sync_conf (self->sync);
#endif
}

static void orage_application_activate (GApplication *app)
{
    GList *list;
    OrageApplication *self;

    self = ORAGE_APPLICATION (app);

    list = gtk_application_get_windows (GTK_APPLICATION (app));

    if (list)
    {
        if (gtk_widget_get_visible (GTK_WIDGET (list->data)) &&
            self->toggle_option)
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
        ((CalWin *)g_par.xfcal)->mWindow =
                gtk_application_window_new (GTK_APPLICATION (app));

        g_signal_connect ((gpointer) ((CalWin *)g_par.xfcal)->mWindow,
                          "delete_event",
                          G_CALLBACK(window_delete_event_cb),
                          (gpointer)g_par.xfcal);

        build_mainWin ();
        set_parameters ();
        if (g_par.start_visible)
            gtk_widget_show (((CalWin *)g_par.xfcal)->mWindow);
        else if (g_par.start_minimized)
        {
            gtk_window_iconify (GTK_WINDOW (((CalWin *)g_par.xfcal)->mWindow));
            gtk_widget_show (((CalWin *)g_par.xfcal)->mWindow);
        }
        else
        {
            /* hidden */
            gtk_widget_realize (((CalWin *)g_par.xfcal)->mWindow);
            gtk_widget_hide (((CalWin *)g_par.xfcal)->mWindow);
        }

        alarm_read ();
        orage_day_change (NULL); /* first day change after we start */
        mCalendar_month_changed_cb (
                (GtkCalendar *)((CalWin *)g_par.xfcal)->mCalendar, NULL);

        /* start monitoring external file updates */
        g_timeout_add_seconds (30, orage_external_update_check, NULL);

        /* day change after resuming */
        resuming_handler_register (self);
    }

    if (self->preferences_option)
        show_parameters ();
}

static void orage_application_shutdown (GApplication *app)
{
#ifdef ENABLE_SYNC
    OrageApplication *self = ORAGE_APPLICATION (app);

    g_object_unref (self->sync);
#endif

    resuming_handler_unregister (self);

#ifdef HAVE_NOTIFY
    orage_notify_uninit ();
#endif

#ifdef HAVE_ARCHIVE
    /* move old appointment to other file to keep the active
       calendar file smaller and faster */
    xfical_archive ();
#endif

    write_parameters ();

    G_APPLICATION_CLASS (orage_application_parent_class)->shutdown (app);
}

static gint orage_application_handle_local_options (
    G_GNUC_UNUSED GApplication *app,
    GVariantDict *options)
{
    if (g_variant_dict_contains (options, "version"))
    {
        print_version ();
        return EXIT_SUCCESS;
    }

    return -1;
}

static gint orage_application_command_line (GApplication *app,
                                            GApplicationCommandLine *cmdline)
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
    OrageApplication *self = ORAGE_APPLICATION (app);

    options = g_application_command_line_get_options_dict (cmdline);

    if (g_variant_dict_contains (options, "preferences"))
        self->preferences_option = TRUE;

    if (g_variant_dict_contains (options, "toggle"))
        self->toggle_option = TRUE;

    if (g_variant_dict_lookup (options, "add-foreign", "^&ay", &file_name))
    {
        str_array = g_strsplit (file_name, ":", 2);
        key[0] = HINT_ADD;
        hint = g_strjoin (":", key, str_array[1], NULL);

        file = g_application_command_line_create_file_for_arg (cmdline,
                                                               str_array[0]);
        g_strfreev (str_array);
        g_application_open (app, &file, 1, hint);
        g_free (hint);
        g_object_unref (file);
    }

    if (g_variant_dict_lookup (options, "remove-foreign", "^&ay", &file_name))
    {
        file = g_application_command_line_create_file_for_arg (cmdline,
                                                               file_name);
        key[0] = HINT_REMOVE;
        g_application_open (app, &file, 1, key);
        g_object_unref (file);
    }

    if (g_variant_dict_lookup (options, "export", "^&ay", &file_name))
    {
        str_array = g_strsplit (file_name, ":", 2);
        key[0] = HINT_EXPORT;
        hint = g_strjoin (":", key, str_array[1], NULL);

        file = g_application_command_line_create_file_for_arg (cmdline,
                                                               str_array[0]);
        g_strfreev (str_array);
        g_application_open (app, &file, 1, hint);
        g_free (hint);
        g_object_unref (file);
    }

    if (g_variant_dict_lookup (options, "import", "^&ay", &file_name))
    {
        str_array = g_strsplit (file_name, ":", 2);
        key[0] = HINT_IMPORT;
        hint = g_strjoin (":", key, str_array[1], NULL);
        file = g_application_command_line_create_file_for_arg (cmdline,
                                                               str_array[0]);
        g_strfreev (str_array);
        g_application_open (app, &file, 1, hint);
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

        key[0] = HINT_OPEN;
        g_application_open (app, files, n_files, key);

        for (n = 0; n < n_files; n++)
            g_object_unref (files[n]);

        g_free (files);
    }

    g_application_activate (app);

    return EXIT_SUCCESS;
}

static void orage_application_open (G_GNUC_UNUSED GApplication *app,
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
            case HINT_OPEN:
                g_message ("open not yet implemented");
                break;

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

static void orage_application_class_init (OrageApplicationClass *klass)
{
    GApplicationClass *application_class;

    application_class = G_APPLICATION_CLASS (klass);
    application_class->startup = orage_application_startup;
    application_class->activate = orage_application_activate;
    application_class->shutdown = orage_application_shutdown;
    application_class->open = orage_application_open;
    application_class->handle_local_options = orage_application_handle_local_options;
    application_class->command_line = orage_application_command_line;
}

static void orage_application_init (OrageApplication *application)
{
    const GOptionEntry option_entries[] =
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
            .description = _("Remove a foreign file"),
            .arg_description = "<file>",
        },
        {
            .long_name = "import",
            .short_name = 'i',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_FILENAME,
            .arg_data = NULL,
            .description = _("Import appointments from file to Orage"),
            .arg_description = "<file>:[appointment...]",
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
            .long_name = NULL,
            .short_name = '\0',
            .flags = 0,
            .arg = 0,
            .arg_data = NULL,
            .description = NULL,
            .arg_description = NULL,
        }
    };

    g_application_add_main_option_entries (G_APPLICATION (application),
                                           option_entries);
    gtk_window_set_default_icon_name (ORAGE_APP_ID);
}

OrageApplication *orage_application_new (void)
{
    return g_object_new (ORAGE_APPLICATION_TYPE,
                         "application-id", ORAGE_APP_ID,
                         "flags", G_APPLICATION_HANDLES_COMMAND_LINE |
                                  G_APPLICATION_HANDLES_OPEN,
                         NULL);
}

OrageTaskRunner *orage_application_get_sync (OrageApplication *application)
{
    return application->sync;
}
