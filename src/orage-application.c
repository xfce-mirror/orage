/*
 * Copyright (c) 2023-2025 Erkki Moorits
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

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include "orage-application.h"

#include "event-list.h"
#include "functions.h"
#include "ical-code.h"
#include "interface.h"
#include "orage-appointment-window.h"
#include "orage-css.h"
#include "orage-i18n.h"
#include "orage-import.h"
#include "orage-sleep-monitor.h"
#include "orage-window.h"
#include "parameters.h"
#include "reminder.h"
#include <glib-2.0/gio/gapplication.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_NOTIFY
#include <libnotify/notify.h>
#endif

#ifdef ENABLE_SYNC
#include "orage-sync-ext-command.h"
#include "orage-task-runner.h"
#endif

#define HINT_ADD 'a'
#define HINT_EXPORT 'x'
#define HINT_IMPORT 'i'
#define HINT_REMOVE 'r'
#define HINT_OPEN 'o'

struct _OrageApplication
{
    GtkApplication parent;
    GtkWidget *window;
    OrageSleepMonitor *sleep_monitor;
    OrageTaskRunner *sync;
    GList *appointments;
    GList *files;
    guint prepare_for_sleep_id;
    gboolean toggle_option;
    gboolean preferences_option;
    gboolean today_option;
    gboolean new_appointment_option;
};

G_DEFINE_TYPE (OrageApplication, orage_application, GTK_TYPE_APPLICATION)

static gboolean window_delete_event_cb (G_GNUC_UNUSED GtkWidget *widget,
                                        G_GNUC_UNUSED GdkEvent *event,
                                        OrageApplication *app)
{
    orage_application_close (app);

    return TRUE;
}

static void cb_preview_dialog_response (GtkDialog *gtk_dialog,
                                        const gint response_id,
                                        gpointer data)
{
    GFile *file;
    gchar *filename;
    GList *tmp_list;
    OrageImportWindow *dialog = ORAGE_IMPORT_WINDOW (gtk_dialog);
    OrageApplication *app = ORAGE_APPLICATION (data);

    g_return_if_fail (ORAGE_IS_IMPORT_WINDOW (dialog));

    switch (response_id)
    {
        case GTK_RESPONSE_ACCEPT:
            for (tmp_list = g_list_first (app->files);
                 tmp_list != NULL;
                 tmp_list = g_list_next (tmp_list))
            {
                file = G_FILE (tmp_list->data);
                filename = g_file_get_path (file);
                if (xfical_import_file (file))
                    g_message ("import done, file=%s", filename);
                else
                    g_warning ("import failed, file=%s", filename);

                g_free (filename);
            }

            break;

        case GTK_RESPONSE_CANCEL:
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    gtk_widget_destroy (GTK_WIDGET (gtk_dialog));
    g_list_free_full (app->appointments, g_object_unref);
    g_list_free_full (app->files, g_object_unref);
    app->appointments = NULL;
    app->files = NULL;
}

static gboolean resuming_after_delay (G_GNUC_UNUSED gpointer user_data)
{
    g_message ("resuming after sleep");

    alarm_read ();
    orage_day_change (&g_par);

    return FALSE;
}

static void woke_up_cb (void)
{
    g_debug ("received resuming signal");

    /* We need this delay to prevent updating tray icon too quickly when the
     * normal code handles it also.
     */
    g_timeout_add_seconds (2, resuming_after_delay, NULL);
}

static void resuming_handler_register (OrageApplication *self)
{
    self->sleep_monitor = orage_sleep_monitor_create ();
    if (self->sleep_monitor)
    {
        g_signal_connect_swapped (G_OBJECT (self->sleep_monitor), "woke-up",
                                  G_CALLBACK (woke_up_cb), NULL);
        g_object_ref (G_OBJECT (self->sleep_monitor));
    }
}

static void print_version (void)
{
    g_print ("%s %s\n\n", PACKAGE, VERSION_FULL);
    g_print (_("Released under the terms of the GNU General Public License."));
    g_print ("\n\n");
    g_print (_("Compiled against GTK+-%d.%d.%d, using GTK+-%d.%d.%d."),
             GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
             gtk_major_version, gtk_minor_version, gtk_micro_version);
    g_print ("\n");

    g_print (_("Compiled against libxfce4ui-%d.%d.%d, using libxfce4ui-%d.%d.%d."),
             LIBXFCE4UI_MAJOR_VERSION, LIBXFCE4UI_MINOR_VERSION, LIBXFCE4UI_MICRO_VERSION,
             libxfce4ui_major_version, libxfce4ui_minor_version, libxfce4ui_micro_version);
    g_print ("\n");

#ifdef HAVE_NOTIFY
    g_print (_("Compiled against libnotify-%d.%d.%d."),
             NOTIFY_VERSION_MAJOR, NOTIFY_VERSION_MINOR, NOTIFY_VERSION_MICRO);
#else
    g_print (_("Not using libnotify."));
#endif
    g_print ("\n");

#ifdef HAVE_ARCHIVE
    g_print (_("Using automatic archiving."));
#else
    g_print (_("Not using archiving."));
#endif

    g_print ("\n\n");
    g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
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

static gboolean is_readable (GFile *file)
{
    gboolean result;
    GError *error = NULL;
    GFileInfo *info = g_file_query_info (file,
        G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_ACCESS_CAN_READ,
        G_FILE_QUERY_INFO_NONE,
        NULL, &error);

    if (info)
    {
        result = g_file_info_get_attribute_boolean (info,
                                                    G_FILE_ATTRIBUTE_ACCESS_CAN_READ);

        if (result == FALSE)
            g_debug ("file is not readale");

        g_object_unref (info);
    }
    else
    {
        g_debug ("could not open file: '%s'", error->message);
        g_clear_error (&error);
        result = FALSE;
    }

    return result;
}

static void show_appointment_preview (OrageApplication *self, GtkWindow *parent)
{
    GtkWidget *dialog = orage_import_window_new (self->appointments);

    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
    gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);

    g_signal_connect (dialog, "response",
                      G_CALLBACK (cb_preview_dialog_response),
                      self);

#if 0
    /* TODO: Inside preview window event description should be also reziable. */
    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
#endif
    gtk_widget_show (dialog);
}

static void orage_application_startup (GApplication *app)
{
#ifdef ENABLE_SYNC
    OrageApplication *self = ORAGE_APPLICATION (app);
#endif

    G_APPLICATION_CLASS (orage_application_parent_class)->startup (app);

    orage_css_set_theme ();
    read_parameters ();
#ifdef ENABLE_SYNC
    self->sync = g_object_new (ORAGE_TASK_RUNNER_TYPE, NULL);
    load_sync_conf (self->sync);
#endif
}

static void open_new_appointment_window (void)
{
    GtkWidget *appointment_window;

    appointment_window = orage_appointment_window_new_now ();
    gtk_window_present (GTK_WINDOW (appointment_window));
}

static void orage_open_today_window (OrageWindow *window)
{
    orage_window_select_today (window);
    (void)create_el_win (NULL);
}

static gboolean orage_application_open_file (OrageApplication *self,
                                             GFile *file)
{
    GList *tmp_list;
    gboolean result;
    gchar *filename;

    g_object_ref (file);
    filename = g_file_get_path (file);

    if (is_readable (file))
    {
        g_debug ("opening calendar file '%s'", filename);
        tmp_list = o_cal_component_list_from_file (file);
        if (tmp_list)
        {
            g_debug ("loaded %d events from file '%s'",
                     g_list_length (tmp_list), filename);
            /* 'file' is freed by callback. */
            self->files = g_list_append (self->files, file);
            self->appointments = g_list_concat (self->appointments, tmp_list);
            result = TRUE;
        }
        else
        {
            g_warning ("ICS file '%s' read failed", filename);
            result = FALSE;
        }
    }
    else
    {
        g_warning ("file '%s' does not exist or cannot be read",
                   filename);
        result = FALSE;
        g_object_unref (file);
    }

    g_free (filename);

    return result;
}

static gboolean orage_application_import_file (OrageApplication *self,
                                               GFile *file)
{
    gboolean result;
    gchar *filename;

    g_object_ref (file);
    filename = g_file_get_path (file);

    if (is_readable (file))
    {
        result = xfical_import_file (file);
        if (result)
        {
            orage_window_update_appointments (ORAGE_WINDOW (self->window));
            g_message ("import done, file=%s", filename);
        }
        else
            g_warning ("import failed, file=%s", filename);
    }
    else
    {
        g_warning ("file '%s' does not exist or cannot be read",
                   filename);
        result = FALSE;
    }

    g_free (filename);
    g_object_unref (file);

    return result;
}

static gboolean orage_application_export_file (G_GNUC_UNUSED OrageApplication *self,
                                               GFile *file,
                                               const gchar *uids)
{
    gboolean result;
    gchar *filename;

    g_object_ref (file);
    filename = g_file_get_path (file);

    g_debug ("exporting to='%s', uids='%s'", filename, uids);

    result = xfical_export_file (file, uids);
    if (result)
        g_message ("export done to file=%s", filename);
    else
        g_warning ("export failed file=%s", filename);

    g_free (filename);
    g_object_unref (file);

    return result;
}

static gboolean orage_application_add_foreign_file (OrageApplication *self,
                                                    GFile *file,
                                                    const gchar *display_name,
                                                    const gboolean read_only)
{
    gboolean result;
    gchar *file_name = g_file_get_path (file);

    result = orage_application_add_foreign_path (self, file_name, display_name,
                                                 read_only);
    g_free (file_name);

    return result;
}

static void orage_application_activate (GApplication *app)
{
    GList *list;
    OrageApplication *self;
    GtkWidget *window;
    gboolean hide_main_window;

    self = ORAGE_APPLICATION (app);
    hide_main_window = self->preferences_option
                     | self->new_appointment_option
                     | self->today_option;

    list = gtk_application_get_windows (GTK_APPLICATION (app));

    if (list)
    {
        /* Orage already running. */
        if (gtk_widget_get_visible (GTK_WIDGET (list->data)) &&
            self->toggle_option)
        {
            write_parameters ();
            gtk_widget_hide (GTK_WIDGET (list->data));
            hide_main_window = TRUE;
        }
        else if (hide_main_window == FALSE)
            orage_window_raise (ORAGE_WINDOW (self->window));

        window = self->window;
    }
    else
    {
        /* Create the main window */
        window = orage_window_create (self, g_par.use_new_ui);

        g_signal_connect (window, "delete-event",
                          G_CALLBACK (window_delete_event_cb), self);

        self->window = window;

        /* XXX: TODO check if this line is needed for main window (it is copied
         * from original code).
         */
        gtk_window_set_destroy_with_parent (GTK_WINDOW (window), TRUE);

        set_parameters ();
        if (g_par.start_minimized)
            gtk_window_iconify (GTK_WINDOW (window));
        else if (g_par.start_visible == FALSE)
        {
            /* hidden */
            gtk_widget_realize (window);
            hide_main_window = TRUE;
        }

        alarm_read ();
        orage_day_change (NULL); /* first day change after we start */
        orage_window_initial_load (ORAGE_WINDOW (window));

        /* start monitoring external file updates */
        g_timeout_add_seconds (30, orage_external_update_check, NULL);

        /* day change after resuming */
        resuming_handler_register (self);
    }

    if (self->appointments)
    {
        gtk_widget_hide (window);
        show_appointment_preview (self, GTK_WINDOW (window));
        hide_main_window = TRUE;
    }

    if (self->preferences_option)
        show_parameters ();

    if (self->new_appointment_option)
        open_new_appointment_window ();

    if (self->today_option)
        orage_open_today_window (ORAGE_WINDOW (window));

    if (hide_main_window)
        gtk_widget_hide (window);
    else
        gtk_widget_show (window);

    self->toggle_option = FALSE;
    self->preferences_option = FALSE;
    self->new_appointment_option = FALSE;
    self->today_option = FALSE;
}

static void orage_application_shutdown (GApplication *app)
{
    OrageApplication *self = ORAGE_APPLICATION (app);

#ifdef ENABLE_SYNC
    g_object_unref (self->sync);
#endif

    if (self->sleep_monitor)
        g_object_unref (G_OBJECT (self->sleep_monitor));

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

    if (g_variant_dict_contains (options, "today"))
        self->today_option = TRUE;

    if (g_variant_dict_contains (options, "new-appointment"))
        self->new_appointment_option = TRUE;

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

static void orage_application_open (GApplication *app,
                                    GFile **files,
                                    const gint n_files,
                                    const gchar *hint)
{
    gchar **hint_array;
    gint i;
    gchar *file;
    gchar *file_name;
    gchar *uids;
    gboolean foreign_file_read_only;

    for (i = 0; i < n_files; i++)
    {
        switch (hint[0])
        {
            case HINT_OPEN:
                (void)orage_application_open_file (ORAGE_APPLICATION (app),
                                                   files[i]);
                break;

            case HINT_ADD:
                hint_array = g_strsplit (hint, ":", 3);
                file_name = NULL;

                if (hint_array[1])
                {
                    foreign_file_read_only =
                            (g_ascii_strcasecmp (hint_array[1], "RW") != 0) &&
                            (g_ascii_strcasecmp (hint_array[1], "READWRITE") != 0);

                    if (hint_array[2])
                        file_name = hint_array[2];
                }
                else
                    foreign_file_read_only = TRUE;

                g_strfreev (hint_array);

                (void)orage_application_add_foreign_file (
                    ORAGE_APPLICATION (app), files[i], file_name,
                    foreign_file_read_only);
                break;

            case HINT_EXPORT:
                hint_array = g_strsplit (hint, ":", 2);
                uids = xfce_str_is_empty (hint_array[1]) ? NULL : hint_array[1];

                (void)orage_application_export_file (ORAGE_APPLICATION (app),
                                                     files[i], uids);
                g_strfreev (hint_array);
                break;

            case HINT_IMPORT:
                (void)orage_application_import_file (ORAGE_APPLICATION (app),
                                                     files[i]);
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
    application_class->open = orage_application_open;
    application_class->command_line = orage_application_command_line;
    application_class->shutdown = orage_application_shutdown;
    application_class->handle_local_options = orage_application_handle_local_options;
}

static void orage_application_init (OrageApplication *self)
{
    const GOptionEntry option_entries[] =
    {
        {
            .long_name = "today",
            .short_name = 'T',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_NONE,
            .arg_data = NULL,
            .description = _("Open today's tasks"),
            .arg_description = NULL,
        },
        {
            .long_name = "new-appointment",
            .short_name = 'n',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_NONE,
            .arg_data = NULL,
            .description = _("Add new appointment"),
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
            .long_name = "version",
            .short_name = 'v',
            .flags = G_OPTION_FLAG_NONE,
            .arg = G_OPTION_ARG_NONE,
            .arg_data = NULL,
            .description = _("Show version of Orage"),
            .arg_description = NULL,
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

    g_application_add_main_option_entries (G_APPLICATION (self),
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

GtkWidget *orage_application_get_window (OrageApplication *self)
{
    return self->window;
}

void orage_application_close (OrageApplication *self)
{
    if (g_par.close_means_quit)
        g_application_quit (G_APPLICATION (self));
    else
        gtk_widget_hide (orage_application_get_window (self));
}

gboolean orage_application_open_path (OrageApplication *self,
                                      const gchar *filename)
{
    gboolean result;
    GFile *file = g_file_new_for_path (filename);

    result = orage_application_open_file (self, file);
    g_object_unref (file);

    if (result)
        show_appointment_preview (self, NULL);

    return result;
}

gboolean orage_application_import_path (OrageApplication *self,
                                        const gchar *filename)
{
    GFile *file = g_file_new_for_path (filename);
    const gboolean result = orage_application_import_file (self, file);

    g_object_unref (file);

    return result;
}

gboolean orage_application_export_path (OrageApplication *self,
                                        const gchar *filename,
                                        const gchar *uids)
{
    GFile *file = g_file_new_for_path (filename);
    const gboolean result = orage_application_export_file (self, file, uids);

    g_object_unref (file);

    return result;
}

gboolean orage_application_add_foreign_path (G_GNUC_UNUSED OrageApplication *self,
                                             const gchar *filename,
                                             const gchar *display_name,
                                             const gboolean read_only)
{
    gboolean result;
    gchar *name;

    name = xfce_str_is_empty (display_name) ? g_path_get_basename (filename)
                                            : g_strdup (display_name);

    g_debug ("add foreign file='%s', display_name='%s', ro=%s",
             filename, name, read_only ? "TRUE" : "FALSE");

    result = orage_foreign_file_add (filename, read_only, name);
    g_free (name);

    if (result)
        g_message ("add done, foreign file='%s'", filename);
    else
        g_warning ("add failed, foreign file='%s'", filename);

    return result;
}

gboolean orage_application_remove_foreign_file (OrageApplication *self,
                                                const gchar *filename)
{
    g_debug ("%s: filename='%s'", G_STRFUNC, filename);
    return FALSE;
}
