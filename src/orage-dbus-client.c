/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2006-2011 Juha Kautto  (juha at xfce.org)
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

#include <glib-object.h>
#include <gio/gio.h>

#include "orage-dbus-client.h"
#include "orage-dbus-object.h"

static gboolean dbus_call (const gchar *method, GVariant *parameters)
{
    GDBusConnection *connection;
    GError *error = NULL;
    GDBusProxy *proxy;
    GVariant *export_result;
    gboolean result;

    connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    if (connection == NULL)
    {
        g_warning ("Failed to connect to the D-BUS session bus: %s",
                   error->message);
        g_error_free (error);
        return FALSE;
    }

    proxy = g_dbus_proxy_new_sync (connection, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                   ORAGE_DBUS_NAME, ORAGE_DBUS_PATH,
                                   ORAGE_DBUS_INTERFACE, NULL, &error);
    if (proxy == NULL)
    {
        g_warning ("Failed to create proxy: %s", error->message);
        g_error_free (error);
        return FALSE;
    }

    export_result = g_dbus_proxy_call_sync (proxy, method, parameters,
                                            G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                                            &error);
    if (G_LIKELY (export_result != NULL))
    {
        g_variant_unref (export_result);
        result = TRUE;
    }
    else
    {
        g_warning ("Proxy call failed: %s", error->message);
        g_error_free (error);
        result = FALSE;
    }

    g_object_unref (proxy);

    return result;
}

/* ********************************
 *      CLIENT side call
 * ********************************
 */
gboolean orage_dbus_import_file (const gchar *file_name)
{
    return dbus_call ("LoadFile", g_variant_new ("(s)", file_name));
}

gboolean orage_dbus_export_file (const gchar *file_name, gint type,
                                 const gchar *uids)
{
    const gchar *_uids = uids ? uids : "";
    return dbus_call ("ExportFile",
                      g_variant_new ("(sis)", file_name, type, _uids));
}

gboolean orage_dbus_foreign_add (const gchar *file_name, gboolean read_only,
                                 const gchar *name)
{
    return dbus_call ("AddForeign",
                      g_variant_new ("(sbs)", file_name, read_only, name));
}

gboolean orage_dbus_foreign_remove (const gchar *file_name)
{
    return dbus_call ("RemoveForeign", g_variant_new ("(s)", file_name));
}
