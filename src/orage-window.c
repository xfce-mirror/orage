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

#include "orage-window.h"

#include "orage-window-classic.h"
#include "orage-application.h"

#include <gtk/gtk.h>
#include <glib.h>

G_DEFINE_INTERFACE (OrageWindow, orage_window, GTK_TYPE_APPLICATION_WINDOW)

static void orage_window_default_init (OrageWindowInterface *iface)
{
}

GtkWidget *orage_window_create (OrageApplication *app, gboolean use_new_ui)
{
    if (use_new_ui)
    {
        /* Not yet implemented. */
        g_return_val_if_reached (NULL);
    }
    else
        return orage_window_classic_new (app);
}

void orage_window_mark_appointments (OrageWindow *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    ORAGE_WINDOW_GET_IFACE (window)->mark_appointments (window);
}

void orage_window_build_info (OrageWindow *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    ORAGE_WINDOW_GET_IFACE (window)->build_info (window);
}

void orage_window_build_events (OrageWindow *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    ORAGE_WINDOW_GET_IFACE (window)->build_events (window);
}

void orage_window_build_todo (OrageWindow *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    ORAGE_WINDOW_GET_IFACE (window)->build_todo (window);
}

void orage_window_month_changed (OrageWindow *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    ORAGE_WINDOW_GET_IFACE (window)->month_changed (window);
}

void orage_window_show_menubar (OrageWindow *window, gboolean show)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    ORAGE_WINDOW_GET_IFACE (window)->show_menubar (window, show);
}

void orage_window_hide_todo (OrageWindow *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    ORAGE_WINDOW_GET_IFACE (window)->hide_todo (window);
}

void orage_window_hide_event (OrageWindow *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    ORAGE_WINDOW_GET_IFACE (window)->hide_event (window);
}

GtkCalendar *orage_window_get_calendar (OrageWindow *window)
{
    g_return_val_if_fail (ORAGE_IS_WINDOW (window), NULL);

    return ORAGE_WINDOW_GET_IFACE (window)->get_calendar (window);
}

void orage_window_raise (OrageWindow *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    ORAGE_WINDOW_GET_IFACE (window)->raise (window);
}
