/*
 * Copyright (C) 2025 Erkki Moorits
 * Copyright (C) 2022 Christian Henz <chrhenz@gmx.de>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>
#include <unistd.h>

#include <gio/gio.h>

#include "orage-sleep-monitor.h"

/* Configuration:
 *
 * - Compile time checks for runtime detection mechanisms.
 *   Currently only D-BUS is used, which is implied.
 *
 * - Compile time checks for available systems.
 *   Currently none, but could check for libraries, target OS.
 */

#define SLEEP_MONITOR_USE_LOGIND 1
#define SLEEP_MONITOR_USE_CONSOLEKIT 1

#define LOGIND_RUNNING() (access ("/run/systemd/seats/", F_OK) >= 0)

/* Base class
 *
 * Provides the signal `woke-up()` that is used to notify the timeouts
 * of wakeup events.
 */

static guint orage_sleep_monitor_woke_up_signal = 0;

G_DEFINE_ABSTRACT_TYPE (OrageSleepMonitor, orage_sleep_monitor, G_TYPE_OBJECT)

static void orage_sleep_monitor_finalize (GObject *object);

static void orage_sleep_monitor_class_init (OrageSleepMonitorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = orage_sleep_monitor_finalize;

  orage_sleep_monitor_woke_up_signal =
    g_signal_new (
      g_intern_static_string ("woke-up"),
      G_TYPE_FROM_CLASS (gobject_class),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}

static void orage_sleep_monitor_init (OrageSleepMonitor *monitor)
{
}

static void orage_sleep_monitor_finalize (GObject *object)
{
  G_OBJECT_CLASS (orage_sleep_monitor_parent_class)->finalize (object);
}

#if defined (SLEEP_MONITOR_USE_LOGIND) || defined (SLEEP_MONITOR_USE_CONSOLEKIT)

struct _OrageSleepDBusMonitor
{
  OrageSleepMonitor parent_instance;
  GDBusProxy *monitor_proxy;
};

#define ORAGE_SLEEP_DBUS_MONITOR_TYPE (orage_sleep_dbus_monitor_get_type ())

G_DECLARE_FINAL_TYPE (OrageSleepDBusMonitor, orage_sleep_dbus_monitor, ORAGE,
                      SLEEP_DBUS_MONITOR, OrageSleepMonitor)

#if GLIB_VERSION_MAX_ALLOWED >= GLIB_VERSION_2_70
G_DEFINE_FINAL_TYPE (OrageSleepDBusMonitor, orage_sleep_dbus_monitor,
                     ORAGE_SLEEP_MONITOR_TYPE)
#else
G_DEFINE_TYPE (OrageSleepDBusMonitor, orage_sleep_dbus_monitor,
               ORAGE_SLEEP_MONITOR_TYPE)
#endif

static void orage_sleep_dbus_monitor_finalize (GObject *object);

static void orage_sleep_dbus_monitor_class_init (OrageSleepDBusMonitorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = orage_sleep_dbus_monitor_finalize;
}

static void orage_sleep_dbus_monitor_init (OrageSleepDBusMonitor *monitor)
{
}

static void orage_sleep_dbus_monitor_finalize (GObject *object)
{
  OrageSleepDBusMonitor *monitor = ORAGE_SLEEP_DBUS_MONITOR (object);
  g_return_if_fail (monitor != NULL);

  if (monitor->monitor_proxy != NULL)
    {
      g_signal_handlers_disconnect_by_data (monitor->monitor_proxy, monitor);
      g_object_unref (G_OBJECT (monitor->monitor_proxy));
    }

  G_OBJECT_CLASS (orage_sleep_dbus_monitor_parent_class)->finalize (object);
}

static void on_prepare_sleep_signal (GDBusProxy *proxy,
                                     gchar *sender_name,
                                     gchar *signal_name,
                                     GVariant *parameters,
                                     OrageSleepMonitor *monitor)
{
  const gchar *format_string = "(b)";
  gboolean going_to_sleep;

  if (strcmp (signal_name, "PrepareForSleep") != 0)
    return;

  if (!g_variant_check_format_string (parameters, format_string, FALSE))
    {
      g_critical ("unexpected format string: %s",
                  g_variant_get_type_string (parameters));
      return;
    }

  g_variant_get (parameters, format_string, &going_to_sleep);

  if (!going_to_sleep)
    g_signal_emit (G_OBJECT (monitor), orage_sleep_monitor_woke_up_signal, 0);
}

static OrageSleepMonitor* orage_sleep_dbus_monitor_create (const gchar *name,
                                                           const gchar *object_path,
                                                           const gchar *interface_name)
{
  OrageSleepDBusMonitor *monitor;
  gchar *owner_name;

  g_debug ("trying to instantiate sleep monitor %s", name);

  monitor = g_object_new (ORAGE_SLEEP_DBUS_MONITOR_TYPE, NULL);
  monitor->monitor_proxy = g_dbus_proxy_new_for_bus_sync (
      G_BUS_TYPE_SYSTEM,
      G_DBUS_PROXY_FLAGS_NONE,
      NULL,
      name,
      object_path,
      interface_name,
      NULL,
      NULL);
  if (monitor->monitor_proxy == NULL)
    {
      g_warning ("could not get proxy for %s", name);
      g_object_unref (G_OBJECT (monitor));
      return NULL;
    }

  owner_name = g_dbus_proxy_get_name_owner (monitor->monitor_proxy);
  if (owner_name == NULL)
    {
      g_debug ("d-bus service %s not active", name);
      g_object_unref (G_OBJECT (monitor));
      return NULL;
    }
  g_free (owner_name);

  g_signal_connect (monitor->monitor_proxy, "g-signal",
                    G_CALLBACK (on_prepare_sleep_signal), monitor);

  return ORAGE_SLEEP_MONITOR (monitor);
}
#endif

#ifdef SLEEP_MONITOR_USE_LOGIND
/* Logind-based implementation */
static OrageSleepMonitor* orage_sleep_monitor_logind_create (void)
{
  if (!LOGIND_RUNNING ())
    {
      g_debug ("logind not running");
      return NULL;
    }

  return orage_sleep_dbus_monitor_create (
      "org.freedesktop.login1",
      "/org/freedesktop/login1",
      "org.freedesktop.login1.Manager");
}
#endif /* defined SLEEP_MONITOR_USE_LOGIND */

#ifdef SLEEP_MONITOR_USE_CONSOLEKIT
static OrageSleepMonitor* orage_sleep_monitor_consolekit_create (void)
{
  return orage_sleep_dbus_monitor_create (
      "org.freedesktop.ConsoleKit",
      "/org/freedesktop/ConsoleKit/Manager",
      "org.freedesktop.ConsoleKit.Manager");
}
#endif /* defined SLEEP_MONITOR_USE_CONSOLEKIT */

/* Factory registration
 *
 * Collect available implementations in a reasonable order.
 */

typedef OrageSleepMonitor* (*SleepMonitorFactory) (void);

static SleepMonitorFactory sleep_monitor_factories[] =
{
  #ifdef SLEEP_MONITOR_USE_LOGIND
  orage_sleep_monitor_logind_create,
  #endif
  #ifdef SLEEP_MONITOR_USE_CONSOLEKIT
  orage_sleep_monitor_consolekit_create,
  #endif
  NULL
};

OrageSleepMonitor *orage_sleep_monitor_create (void)
{
  SleepMonitorFactory *factory_ptr = &sleep_monitor_factories[0];
  OrageSleepMonitor *monitor = NULL;

  for (; monitor == NULL && *factory_ptr != NULL; factory_ptr++)
    monitor = (*factory_ptr) ();

  if (monitor == NULL && sleep_monitor_factories[0] != NULL)
    g_debug ("could not instantiate a sleep monitor");

  return monitor;
}
