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

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#define ORAGE_MAIN  "orage"

#include "orage-i18n.h"
#include "orage-css.h"
#include "functions.h"
#include "mainbox.h"
#include "reminder.h"
#include "ical-code.h"
#include "tray_icon.h"
#include "parameters.h"
#include "interface.h"
#ifdef HAVE_DBUS
#include "orage-dbus-client.h"
#include <dbus/dbus-glib-lowlevel.h>
#endif

#define CALENDAR_RAISE_EVENT "_XFCE_CALENDAR_RAISE"
#define CALENDAR_PREFERENCES_EVENT "_XFCE_CALENDAR_PREFERENCES"

/* session client handler */
/*
static SessionClient	*session_client = NULL;
*/
static GdkAtom atom_alive;

#ifdef HAVE_DBUS
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

static void send_event (const char *event)
{
    XEvent xevent;
    Window xwindow;
    Display *display;

    (void)memset (&xevent, 0, sizeof (xevent));

    display = gdk_x11_get_default_xdisplay ();
    xwindow = XGetSelectionOwner (display, gdk_x11_atom_to_xatom (atom_alive));

    xevent.xclient.type = ClientMessage;
    xevent.xclient.display = display;
    xevent.xclient.window = xwindow;
    xevent.xclient.send_event = TRUE;
    xevent.xclient.message_type = XInternAtom (display, event, FALSE);
    xevent.xclient.format = 8;

    if (XSendEvent (display, xwindow, FALSE, NoEventMask, &xevent) == 0)
        g_warning ("send message to %s failed", event);

    (void)XFlush (display);
}

void orage_toggle_visible(void)
{
    g_debug ("orage_toggle_visible(), send '" CALENDAR_TOGGLE_EVENT "' event");
    send_event (CALENDAR_TOGGLE_EVENT);
}

static void raise_orage(void)
{
    g_debug ("raise_orage(), send '" CALENDAR_RAISE_EVENT "' event");
    send_event (CALENDAR_RAISE_EVENT);
}

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
                                         G_GNUC_UNUSED gpointer user_data)
{
    if (g_par.close_means_quit) {
        gtk_main_quit();
    }
    else {
        orage_toggle_visible();
    }

    return(TRUE);
}

static void raise_window(void)
{
    GdkWindow *window;
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
    window = gtk_widget_get_window (GTK_WIDGET(cal->mWindow));
    gdk_x11_window_set_user_time(window, gdk_x11_get_server_time(window));
    gtk_widget_show(cal->mWindow);
    gtk_window_present(GTK_WINDOW(cal->mWindow));
}

static GdkFilterReturn client_message_filter (GdkXEvent *gdkxevent,
                                              G_GNUC_UNUSED GdkEvent *event,
                                              G_GNUC_UNUSED gpointer data)
{
    XClientMessageEvent *evt;
    XEvent *xevent = (XEvent *)gdkxevent;
    CalWin *cal = (CalWin *)g_par.xfcal;

    if (xevent->type != ClientMessage)
        return GDK_FILTER_CONTINUE;

    evt = (XClientMessageEvent *)gdkxevent;

    if (evt->message_type ==
            XInternAtom (evt->display, CALENDAR_RAISE_EVENT, FALSE))
    {
        g_debug ("received '" CALENDAR_RAISE_EVENT "' event");
        raise_window ();
        return GDK_FILTER_REMOVE;
    }
    else if (evt->message_type ==
             XInternAtom (evt->display, CALENDAR_TOGGLE_EVENT, FALSE))
    {
        g_debug ("received '" CALENDAR_TOGGLE_EVENT "' event");
        if (gtk_widget_get_visible (cal->mWindow))
        {
            write_parameters ();
            gtk_widget_hide (cal->mWindow);
        }
        else
            raise_window ();

        return GDK_FILTER_REMOVE;
    }
    else if (evt->message_type ==
            XInternAtom (evt->display, CALENDAR_PREFERENCES_EVENT, FALSE))
    {
        g_debug ("received '" CALENDAR_PREFERENCES_EVENT "' event");
        show_parameters ();
        return GDK_FILTER_REMOVE;
    }

    return GDK_FILTER_CONTINUE;
}

/*
 * SaveYourself callback
 *
 * This is called when the session manager requests the client to save its
 * state.
 */
/*
void save_yourself_cb(gpointer data, int save_style, gboolean shutdown
        , int interact_style, gboolean fast)
{
    write_parameters();
}
*/

/*
 * Die callback
 *
 * This is called when the session manager requests the client to go down.
 */
/*
void die_cb(gpointer data)
{
    gtk_main_quit();
}
*/

static void print_version(void)
{
    g_print(_("\tThis is %s version %s\n\n")
            , PACKAGE, VERSION);
    g_print(_("\tReleased under the terms of the GNU General Public License.\n"));
    g_print(_("\tCompiled against GTK+-%d.%d.%d, ")
            , GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
    g_print(_("using GTK+-%d.%d.%d.\n")
            , gtk_major_version, gtk_minor_version, gtk_micro_version);
#ifdef HAVE_DBUS
    g_print(_("\tUsing DBUS for import.\n"));
#else
    g_print(_("\tNot using DBUS. Import works only partially.\n"));
#endif
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
#ifdef HAVE_LIBICAL
    g_print(_("\tUsing operating system package libical.\n"));
#else
    g_print(_("\tUsing Orage local version of libical.\n"));
#endif
#ifdef HAVE_LIBXFCE4UI
    g_print(_("\tUsing libxfce4ui: yes\n"));
#else
    g_print(_("\tUsing libxfce4ui: no\n"));
#endif
    g_print("\n");
}

static void preferences(void)
{
    send_event (CALENDAR_RAISE_EVENT);
    send_event (CALENDAR_PREFERENCES_EVENT);
}

static void print_help(void)
{
    g_print(_("Usage: orage [options] [files]\n\n"));
    g_print(_("Options:\n"));
    g_print(_("--version (-v) \t\tshow version of orage\n"));
    g_print(_("--help (-h) \t\tprint this text\n"));
    g_print(_("--preferences (-p) \tshow preferences form\n"));
    g_print(_("--toggle (-t) \t\tmake orage visible/unvisible\n"));
    g_print(_("--add-foreign (-a) file [RW] [name] \tadd a foreign file\n"));
    g_print(_("--remove-foreign (-r) file \tremove a foreign file\n"));
    g_print(_("--export (-e) file [appointment...] \texport appointments from Orage to file\n"));
    g_print("\n");
    g_print(_("files=ical files to load into orage\n"));
#ifndef HAVE_DBUS
    g_print(_("\tdbus not included in orage. \n"));
    g_print(_("\twithout dbus [files] and foreign file options(-a & -r) can only be used when starting orage \n"));
#endif
    g_print("\n");
}

static void import_file(gboolean running, char *file_name, gboolean initialized)
{
    if (running && !initialized) {
        /* let's use dbus since server is running there already */
#ifdef HAVE_DBUS
        if (orage_dbus_import_file(file_name))
            g_message ("import done file=%s", file_name);
        else
            g_warning ("import failed file=%s", file_name);
#else
        g_warning("Can not do import without dbus. import failed file=%s",
                  file_name);
#endif
    }
    else if (!running && initialized) {/* do it self directly */
        if (xfical_import_file(file_name))
            g_message ("import done file=%s", file_name);
        else
            g_warning ("import failed file=%s", file_name);
    }
}

static void export_file(gboolean running, char *file_name, gboolean initialized
        , gchar *uid_list)
{
    gint type = 0;
    
    if (uid_list)
        type = 1;
    else
        type = 0;
    g_print("export_file: running=%d initialized= %d type=%d, file=%s, uids=%s\n", running, initialized, type, file_name, uid_list);
    if (running && !initialized) {
        /* let's use dbus since server is running there already */
#ifdef HAVE_DBUS
        if (orage_dbus_export_file(file_name, type, uid_list))
            g_message ("export done to file=%s", file_name);
        else
            g_warning ("export failed file=%s", file_name);
#else
        g_warning("Can not do export without dbus. failed file=%s", file_name);
#endif
    }
    else if (!running && initialized) { /* do it self directly */
        if (xfical_export_file(file_name, type, uid_list))
            g_message ("export done to file=%s", file_name);
        else
            g_warning ("export failed file=%s", file_name);
    }
}

static void add_foreign(gboolean running, char *file_name, gboolean initialized
        , gboolean read_only, char *name)
{
    g_print("\nadd_foreign: file_name%s name:%s\n\n", file_name, name);
    if (running && !initialized) {
        /* let's use dbus since server is running there already */
#ifdef HAVE_DBUS
        if (orage_dbus_foreign_add(file_name, read_only, name))
            g_message ("Add done online foreign file=%s", file_name);
        else
            g_warning ("Add failed online foreign file=%s", file_name);
#else
        g_warning ("Can not do add foreign file to running Orage without dbus. "
                   "Add failed foreign file=%s", file_name);
#endif
    }
    else if (!running && initialized) { /* do it self directly */
        if (orage_foreign_file_add(file_name, read_only, name))
            g_message ("Add done foreign file=%s", file_name);
        else
            g_warning ("Add failed foreign file=%s", file_name);
    }
}

static void remove_foreign(gboolean running, char *file_name, gboolean initialized)
{
    if (running && !initialized) {
        /* let's use dbus since server is running there already */
#ifdef HAVE_DBUS
        if (orage_dbus_foreign_remove(file_name))
            g_message ("Remove done foreign file=%s", file_name);
        else
            g_warning ("Remove failed foreign file=%s", file_name);
#else
        g_warning ("Can not do remove foreign file without dbus. "
                   "Remove failed foreign file=%s", file_name);
#endif
    }
    else if (!running && initialized) { /* do it self directly */
        if (orage_foreign_file_remove(file_name))
            g_message ("Remove done foreign file=%s", file_name);
        else
            g_warning ("Remove failed foreign file=%s", file_name);
    }
}

static gboolean process_args(int argc, char *argv[], gboolean running
        , gboolean initialized)
{
    int argi;
    gboolean end = FALSE;
    gboolean foreign_file_read_only = TRUE;
    gboolean foreign_file_name_parameter = FALSE;
    gchar *export_uid_list = NULL;
    gchar *file_name = NULL;

    if (running && argc == 1) { /* no parameters */
        raise_orage();
        return(TRUE);
    }
    end = running;
    for (argi = 1; argi < argc; argi++) {
        if (!strcmp(argv[argi], "--sm-client-id")) {
            argi++; /* skip the parameter also */
        }
        else if (!strcmp(argv[argi], "--version") || 
                 !strcmp(argv[argi], "-v")        ||
                 !strcmp(argv[argi], "-V")) {
            print_version();
            end = TRUE;
        }
        else if (!strcmp(argv[argi], "--help") || 
                 !strcmp(argv[argi], "-h")     ||
                 !strcmp(argv[argi], "-?")) {
            print_help();
            end = TRUE;
        }
        else if (!strcmp(argv[argi], "--preferences") || 
                 !strcmp(argv[argi], "-p")) {
            if (running && !initialized) {
                preferences();
                end = TRUE;
            }
            else if (!running && initialized) {
                preferences();
            }
            /* if (!running && !initialized) Do nothing 
             * if (running && initialized) impossible
             */
        }
        else if (!strcmp(argv[argi], "--toggle") || 
                 !strcmp(argv[argi], "-t")) {
            orage_toggle_visible();
            end = TRUE;
        }
        else if (!strcmp(argv[argi], "--add-foreign") ||
                 !strcmp(argv[argi], "-a")) {
            if (argi+1 >= argc) {
                g_print("\nFile not specified\n\n");
                print_help();
                end = TRUE;
            } 
            else {
                if (argi+2 < argc && (
                    !strcmp(argv[argi+2], "RW") ||
                    !strcmp(argv[argi+2], "READWRITE"))) {
                    foreign_file_read_only = FALSE;
                    if (argi+3 < argc) {
                        file_name = g_strdup(argv[argi+3]);
                        foreign_file_name_parameter = TRUE;
                    }
                }
                else if (argi+2 < argc) { /* take argi+2 as name of file */
                    file_name = g_strdup(argv[argi+2]);
                    foreign_file_name_parameter = TRUE;
                }
                if (!file_name) {
                    file_name = g_path_get_basename(argv[argi+1]);
                }
                add_foreign(running, argv[++argi], initialized
                        , foreign_file_read_only, file_name);
                if (!foreign_file_read_only)
                    ++argi;
                if (foreign_file_name_parameter) {
                    ++argi;
                }
                g_free(file_name);
            }
        }
        else if (!strcmp(argv[argi], "--remove-foreign") ||
                 !strcmp(argv[argi], "-r")) {
            if (argi+1 >= argc) {
                g_print("\nFile not specified\n\n");
                print_help();
                end = TRUE;
            } 
            else {
                remove_foreign(running, argv[++argi], initialized);
            }
        }
        else if (!strcmp(argv[argi], "--export") ||
                 !strcmp(argv[argi], "-e")) {
            if (argi+1 >= argc) {
                g_print("\nFile not specified\n\n");
                print_help();
                end = TRUE;
            } 
            else {
                file_name = argv[++argi];
                if (argi+1 < argc) {
                    export_uid_list = argv[++argi];
                }
                export_file(running, file_name, initialized, export_uid_list);
            }
        }
        else if (argv[argi][0] == '-') {
            g_print(_("\nUnknown option %s\n\n"), argv[argi]);
            print_help();
            end = TRUE;
        }
        else {
            import_file(running, argv[argi], initialized);
            raise_orage();
        }
    }
    return(end);
}

static gboolean check_orage_alive(void)
{
    if (XGetSelectionOwner(gdk_x11_get_default_xdisplay(),
                           gdk_x11_atom_to_xatom(atom_alive)) != None)
        return(TRUE); /* yes, we got selection owner, so orage must be there */
    else /* no selction owner, so orage is not running yet */
        return(FALSE);
}

static void mark_orage_alive(void)
{
    GtkWidget *hidden;
    GdkWindow *window;

    hidden = gtk_invisible_new();
    gtk_widget_show(hidden);
    window = gtk_widget_get_window (hidden);
    if (!gdk_selection_owner_set(window, atom_alive
                , gdk_x11_get_server_time(window), FALSE)) {
        g_error("Unable acquire ownership of selection");
    }

    gdk_window_add_filter (window, client_message_filter, NULL);
}

int main(int argc, char *argv[])
{
    gboolean running, initialized = FALSE;

    /* init i18n = nls to use gettext */
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif
    textdomain(GETTEXT_PACKAGE);

    gtk_init(&argc, &argv);
    register_css_provider ();

    atom_alive = gdk_atom_intern (CALENDAR_RUNNING, FALSE);
    running = check_orage_alive();
    if (process_args(argc, argv, running, initialized)) 
        return(EXIT_SUCCESS);
    /* we need to start since orage was not found to be running already */
    mark_orage_alive();

    g_par.xfcal = g_new(CalWin, 1);
    /* Create the main window */
    ((CalWin *)g_par.xfcal)->mWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect((gpointer) ((CalWin *)g_par.xfcal)->mWindow, "delete_event"
            , G_CALLBACK(mWindow_delete_event_cb), (gpointer)g_par.xfcal);

    /* 
    * try to connect to the session manager
    */
    /*
    session_client = client_session_new(argc, argv, NULL
            , SESSION_RESTART_IF_RUNNING, 50);
    session_client->save_yourself = save_yourself_cb;
    session_client->die = die_cb;
    (void)session_init(session_client);
    */

    /*
    * Now it's serious, the application is running, so we create the RC
    * directory and check for config files in old location.
    */
#ifdef HAVE_DBUS
    orage_dbus_start();
#endif

    /*
    * Create the orage.
    */
    read_parameters();
    build_mainWin();
    set_parameters();
    if (g_par.start_visible) {
        gtk_widget_show(((CalWin *)g_par.xfcal)->mWindow);
    }
    else if (g_par.start_minimized) {
        gtk_window_iconify(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow));
        gtk_widget_show(((CalWin *)g_par.xfcal)->mWindow);
    }
    else { /* hidden */
        gtk_widget_realize(((CalWin *)g_par.xfcal)->mWindow);
        gtk_widget_hide(((CalWin *)g_par.xfcal)->mWindow);
    }
    alarm_read();
    orage_day_change(NULL); /* first day change after we start */
    mCalendar_month_changed_cb(
            (GtkCalendar *)((CalWin *)g_par.xfcal)->mCalendar, NULL);

    /* start monitoring external file updates */
    g_timeout_add_seconds(30, (GSourceFunc)orage_external_update_check, NULL);

    /* let's check if I got filename as a parameter */
    initialized = TRUE;
    process_args(argc, argv, running, initialized);

#ifdef HAVE_DBUS
    /* day change after resuming */
    handle_resuming();
#endif

    gtk_main();
    keep_tidy();
    return(EXIT_SUCCESS);
}
