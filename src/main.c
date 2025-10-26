/*
 * Copyright (c) 2021-2024 Erkki Moorits
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

#include "orage-i18n.h"
#include "orage-dbus.h"
#include "orage-application.h"

#include <glib.h>
#include <gio/gio.h>

#include <libxfce4util/libxfce4util.h>

#ifdef G_OS_UNIX
#include <signal.h>
#include <glib-unix.h>

static gboolean quit_handler (gpointer orage_app)
{
    g_application_quit (G_APPLICATION (orage_app));

    return G_SOURCE_REMOVE;
}
#endif

int main (int argc, char **argv)
{
    GError *error = NULL;

    g_autoptr (OrageApplication) orage_app = NULL;

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    g_set_application_name (_("Orage"));

    orage_app = orage_application_new ();

#ifdef G_OS_UNIX
    (void)g_unix_signal_add (SIGINT, quit_handler, orage_app);
#endif

    if (orage_dbus_register_service (orage_app, &error) == FALSE)
    {
        g_warning ("unable to register terminal service: %s", error->message);
        g_clear_error (&error);
    }

    return g_application_run (G_APPLICATION (orage_app), argc, argv);;
}
