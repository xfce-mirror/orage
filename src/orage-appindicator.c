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

#include <config.h>
#include "orage-appindicator.h"
#include <libayatana-appindicator/app-indicator.h>
#include "orage-window.h"
#include "orage-i18n.h"
#include "functions.h"
#include "event-list.h"
#include "orage-tray-icon-common.h"
#include "parameters.h"

void *orage_appindicator_create (void)
{
    GtkWidget *menu;
    AppIndicator *ci;

    ci = app_indicator_new (ORAGE_APP_ID, ORAGE_APP_ID,
                            APP_INDICATOR_CATEGORY_APPLICATION_STATUS);

    g_assert (APP_IS_INDICATOR (ci));
    g_assert (G_IS_OBJECT (ci));

    app_indicator_set_status (ci, APP_INDICATOR_STATUS_ACTIVE);

    menu = orage_tray_icon_create_menu ();
    
    app_indicator_set_menu (ci, GTK_MENU (menu));

    return ci;
}

void orage_appindicator_refresh (void)
{
    void *appindicator;

    if (g_par.show_systray)
    {
        if (g_par.trayIcon)
        {
            orage_appindicator_set_visible (g_par.trayIcon, FALSE);
            g_object_unref (g_par.trayIcon);
        }

        appindicator = orage_appindicator_create ();
        g_return_if_fail (appindicator != NULL);

        orage_appindicator_set_visible (appindicator, TRUE);
        g_par.trayIcon = appindicator;
    }
}

void orage_appindicator_set_visible (void *tray_icon,
                                     const gboolean show_systray)
{
    AppIndicator *ci;
    AppIndicatorStatus status;

    ci = APP_INDICATOR (tray_icon);
    status = show_systray ? APP_INDICATOR_STATUS_ACTIVE
                          : APP_INDICATOR_STATUS_PASSIVE;

    app_indicator_set_status (ci, status);
}

void orage_appindicator_set_title (void *appindicator, const gchar *title)
{
    AppIndicator *ci = APP_INDICATOR (appindicator);

    app_indicator_set_title (ci, title);
}

void orage_appindicator_set_description (void *appindicator, const gchar *desc)
{
    AppIndicator *ci = APP_INDICATOR (appindicator);

    app_indicator_set_icon_full (ci, ORAGE_APP_ID, desc);
}
