/*
 * Copyright (c) 2021-2023 Erkki Moorits
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
#include <config.h>
#endif

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include "orage-i18n.h"
#include "orage-window.h"

#include "orage-application.h"

static OrageApplication *orage_app;

static void quit_handler (G_GNUC_UNUSED int s)
{
    g_application_quit (G_APPLICATION (orage_app));
}

int main (int argc, char **argv)
{
    int status;
    struct sigaction sig_int_handler;

    g_set_application_name (_("Orage"));

    orage_app = orage_application_new ();

    sig_int_handler.sa_handler = quit_handler;
    sigemptyset (&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;

    sigaction (SIGINT, &sig_int_handler, NULL);
    status = g_application_run (G_APPLICATION (orage_app), argc, argv);
    g_object_unref (orage_app);

    return status;
}
