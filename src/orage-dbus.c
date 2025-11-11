/*
 * Copyright (c) 2025 Erkki Moorits
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

#include "orage-dbus.h"

#include "orage-application.h"
#include "functions.h"

#include <gio/gio.h>
#include <glib.h>

#define ORAGE_DBUS_SERVICE ORAGE_APP_ID
#define ORAGE_DBUS_PATH "/org/xfce/orage"
#define ORAGE_DBUS_METHOD_OPEN_FILE "OpenFile"
#define ORAGE_DBUS_METHOD_IMPORT_FILE "ImportFile"
#define ORAGE_DBUS_METHOD_EXPORT_FILE "ExportFile"
#define ORAGE_DBUS_METHOD_ADD_FOREIGN_FILE "AddForeign"
#define ORAGE_DBUS_METHOD_REMOVE_FOREIGN_FILE "RemoveForeign"

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='" ORAGE_DBUS_SERVICE "'>"
    "    <method name='" ORAGE_DBUS_METHOD_OPEN_FILE "'>"
    "      <arg type='s' name='filename' direction='in'/>"
    "      <arg type='b' name='success' direction='out'/>"
    "    </method>"
    "    <method name='" ORAGE_DBUS_METHOD_IMPORT_FILE "'>"
    "      <arg type='s' name='filename' direction='in'/>"
    "      <arg type='b' name='success' direction='out'/>"
    "    </method>"
    "    <method name='" ORAGE_DBUS_METHOD_EXPORT_FILE "'>"
    "      <arg type='s' name='filename' direction='in'/>"
    "      <arg type='b' name='success' direction='out'/>"
    "    </method>"
    "    <method name='" ORAGE_DBUS_METHOD_ADD_FOREIGN_FILE "'>"
    "      <arg type='s' name='filename' direction='in'/>"
    "      <arg type='b' name='success' direction='out'/>"
    "    </method>"
    "    <method name='" ORAGE_DBUS_METHOD_REMOVE_FOREIGN_FILE "'>"
    "      <arg type='s' name='filename' direction='in'/>"
    "      <arg type='b' name='success' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

static void on_method_call (GDBusConnection *connection,
                            const gchar *sender,
                            const gchar *object_path,
                            const gchar *interface_name,
                            const gchar *method_name,
                            GVariant *parameters,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data);

static const GDBusInterfaceVTable interface_vtable =
{
    on_method_call,
    NULL,
    NULL,
};

static void on_method_call (G_GNUC_UNUSED GDBusConnection *connection,
                            G_GNUC_UNUSED const gchar *sender,
                            G_GNUC_UNUSED const gchar *object_path,
                            G_GNUC_UNUSED const gchar *interface_name,
                            const gchar *method_name,
                            GVariant *parameters,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data)
{
    OrageApplication *app;
    gboolean success;
    const gchar *filename;

    app = ORAGE_APPLICATION (user_data);

    g_debug ("%s: '%s' called", G_STRFUNC, method_name);

    if (g_strcmp0 (method_name, ORAGE_DBUS_METHOD_OPEN_FILE) == 0)
    {
        g_variant_get (parameters, "(&s)", &filename);
        success = orage_application_open_path (app, filename);

        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)",
                                               success));
    }
    else if (g_strcmp0 (method_name, ORAGE_DBUS_METHOD_IMPORT_FILE) == 0)
    {
        g_variant_get (parameters, "(&s)", &filename);
        success = orage_application_import_path (app, filename);

        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)",
                                               success));
    }
    else if (g_strcmp0 (method_name, ORAGE_DBUS_METHOD_EXPORT_FILE) == 0)
    {
        g_variant_get (parameters, "(&s)", &filename);
        success = orage_application_export_file (app, filename);
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)",
                                               success));
    }
    else if (g_strcmp0 (method_name, ORAGE_DBUS_METHOD_ADD_FOREIGN_FILE) == 0)
    {
        g_variant_get (parameters, "(&s)", &filename);
        success = orage_application_add_foreign_file (app, filename);
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)",
                                               success));
    }
    else if (g_strcmp0 (method_name, ORAGE_DBUS_METHOD_REMOVE_FOREIGN_FILE) == 0)
    {
        g_variant_get (parameters, "(&s)", &filename);
        success = orage_application_remove_foreign_file (app, filename);
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)",
                                               success));
    }
    else
        g_warning ("unknown DBUS method name '%s'", method_name);
}

static void orage_dbus_bus_acquired (GDBusConnection *connection,
                                     const gchar *name,
                                     gpointer user_data)
{
    guint register_id;
    GDBusNodeInfo *info;
    GError *error = NULL;

    info = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
    g_assert (info != NULL);
    g_assert (*info->interfaces != NULL);

    register_id = g_dbus_connection_register_object (connection,
                                                     ORAGE_DBUS_PATH,
                                                     *info->interfaces,
                                                     &interface_vtable,
                                                     user_data,
                                                     NULL,
                                                     &error);

    if (register_id == 0)
    {
        g_warning ("failed to register object: %s", error->message);
        g_error_free (error);
    }
    else
        g_info ("DBus service registered");

    g_dbus_node_info_unref (info);
}

gboolean orage_dbus_register_service (OrageApplication *app,
                                      G_GNUC_UNUSED GError **error)
{
    guint owner_id;

    owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                               ORAGE_DBUS_SERVICE,
                               G_BUS_NAME_OWNER_FLAGS_NONE,
                               orage_dbus_bus_acquired,
                               NULL,
                               NULL,
                               app,
                               NULL);
    return (owner_id != 0);
}
