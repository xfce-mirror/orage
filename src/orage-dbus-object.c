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

#include <glib.h>
#include <glib-object.h>

#include <gio/gio.h>

#include "interface.h"
#include "orage-dbus-object.h"
#include "orage-dbus-service.h"

static gboolean orage_dbus_service_load_file (OrageExportedService *skeleton,
                                              GDBusMethodInvocation *invocation,
                                              const char *IN_file,
                                              gpointer user_data);

static gboolean orage_dbus_service_export_file (OrageExportedService *skeleton,
                                                GDBusMethodInvocation *invocation,
                                                const gchar *file,
                                                gint type,
                                                const gchar *uids,
                                                gpointer user_data);

static gboolean orage_dbus_service_add_foreign (OrageExportedService *skeleton,
                                                GDBusMethodInvocation *invocation,
                                                const char *IN_file,
                                                const gboolean IN_mode,
                                                const char *IN_name,
                                                gpointer user_data);

static gboolean orage_dbus_service_remove_foreign (OrageExportedService *skeleton,
                                                   GDBusMethodInvocation *invocation,
                                                   const char *IN_file,
                                                   gpointer user_data);

struct _OrageDBusClass
{
    OrageExportedServiceSkeletonClass __parent__;
};

struct _OrageDBus
{
    OrageExportedServiceSkeletonClass __parent__;
    GDBusConnection *connection;
};

G_DEFINE_TYPE(OrageDBus, orage_dbus, ORAGE_TYPE_EXPORTED_SERVICE_SKELETON)

OrageDBus *orage_dbus;

static void orage_dbus_class_init (G_GNUC_UNUSED OrageDBusClass *orage_class)
{
}

static void orage_dbus_init(OrageDBus *o_dbus)
{
    GError *error = NULL;

    o_dbus->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    if (G_UNLIKELY (o_dbus->connection == NULL))
    {
        g_warning ("Failed to connect to the D-BUS session bus: %s",
                   error->message);
        g_error_free (error);
        return;
    }

    if (G_UNLIKELY (g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (o_dbus),
                                                      o_dbus->connection,
                                                      "/org/xfce/orage",
                                                      &error)) == FALSE)
    {
        g_critical ("Failed to export panel D-BUS interface: %s",
                    error->message);
        g_error_free (error);
        return;
    }

    g_signal_connect (o_dbus, "handle_load_file",
                      G_CALLBACK (orage_dbus_service_load_file), NULL);

    g_signal_connect (o_dbus, "handle_export_file",
                      G_CALLBACK (orage_dbus_service_export_file), NULL);

    g_signal_connect (o_dbus, "handle_add_foreign",
                      G_CALLBACK (orage_dbus_service_add_foreign), NULL);

    g_signal_connect (o_dbus, "handle_remove_foreign",
                      G_CALLBACK (orage_dbus_service_remove_foreign), NULL);
}

static gboolean orage_dbus_service_load_file (OrageExportedService *skeleton,
                                              GDBusMethodInvocation *invocation,
                                              const gchar *file,
                                              G_GNUC_UNUSED gpointer user_data)
{
    if (orage_import_file (file))
    {
        g_message ("DBUS File added %s", file);
        orage_exported_service_complete_load_file (skeleton, invocation);
    }
    else
    {
        g_warning ("DBUS File add failed %s", file);
        g_dbus_method_invocation_return_error (invocation, G_FILE_ERROR,
                                               G_FILE_ERROR_INVAL,
                                               "Invalid ical file '%s'",
                                               file);
    }

    return TRUE;
}

static gboolean orage_dbus_service_export_file (OrageExportedService *skeleton,
                                                GDBusMethodInvocation *invocation,
                                                const gchar *file,
                                                gint type,
                                                const gchar *uids,
                                                G_GNUC_UNUSED gpointer user_data)
{
    if (orage_export_file (file, type, uids))
    {
        g_message ("DBUS file exported: %s", file);
        orage_exported_service_complete_export_file (skeleton, invocation);
    }
    else
    {
        g_warning ("DBUS file export failed: %s", file);
        g_dbus_method_invocation_return_error (invocation, G_FILE_ERROR,
                                               G_FILE_ERROR_INVAL,
                                               "DBUS file export failed: %s",
                                               file);
    }

    return TRUE;
}

static gboolean orage_dbus_service_add_foreign (OrageExportedService *skeleton,
                                                GDBusMethodInvocation *invocation,
                                                const gchar *file,
                                                const gboolean mode,
                                                const gchar *name,
                                                G_GNUC_UNUSED gpointer user_data)
{
    if (orage_foreign_file_add (file, mode, name))
    {
        g_message ("DBUS Foreign file added %s", file);
        orage_exported_service_complete_add_foreign (skeleton, invocation);
    }
    else
    {
        g_warning ("DBUS Foreign file add failed %s", file);
        g_dbus_method_invocation_return_error (invocation, G_FILE_ERROR,
                                               G_FILE_ERROR_INVAL,
                                               "DBUS Foreign file add failed: %s",
                                               file);
    }

    return TRUE;
}

static gboolean orage_dbus_service_remove_foreign (OrageExportedService *skeleton,
                                                   GDBusMethodInvocation *invocation,
                                                   const gchar *file,
                                                   G_GNUC_UNUSED gpointer user_data)
{
    if (orage_foreign_file_remove (file))
    {
        g_message ("DBUS Foreign file removed %s", file);
        orage_exported_service_complete_remove_foreign (skeleton, invocation);
    }
    else
    {
        g_warning ("DBUS Foreign file remove failed %s", file);
        g_dbus_method_invocation_return_error (invocation, G_FILE_ERROR,
                                               G_FILE_ERROR_INVAL,
                                               "DBUS Foreign file remove failed %s",
                                               file);
    }

    return TRUE;
}

static void orage_dbus_bus_acquired (G_GNUC_UNUSED GDBusConnection *connection,
                                     G_GNUC_UNUSED const gchar     *name,
                                     G_GNUC_UNUSED gpointer         user_data)
{
    orage_dbus = g_object_new (ORAGE_TYPE_DBUS, NULL);
}

void orage_dbus_start(void)
{
    g_bus_own_name (G_BUS_TYPE_SESSION,
                    "org.xfce.orage",
                    G_BUS_NAME_OWNER_FLAGS_NONE,
                    orage_dbus_bus_acquired,
                    NULL,
                    NULL,
                    NULL, NULL);
}
