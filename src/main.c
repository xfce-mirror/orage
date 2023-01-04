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

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>

#define ORAGE_MAIN "orage"

#include "functions.h"
#include "mainbox.h"
#include "parameters.h"

#include "orage-application.h"

static OrageApplication *orage_app;

static void raise_window (void)
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

static void quit_handler (G_GNUC_UNUSED int s)
{
    orage_quit ();
}

void orage_toggle_visible (void)
{
    GList *list;

    list = gtk_application_get_windows (GTK_APPLICATION (orage_app));

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

    g_set_application_name ("Orage");

    orage_app = orage_application_new ();

    sig_int_handler.sa_handler = quit_handler;
    sigemptyset (&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;

    sigaction (SIGINT, &sig_int_handler, NULL);
    status = g_application_run (G_APPLICATION (orage_app), argc, argv);
    g_object_unref (orage_app);

    return status;
}
