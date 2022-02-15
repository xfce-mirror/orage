/*      Orage - Calendar and alarm handler
 * Copyright (c) 2021 Erkki Moorits  (erkki.moorits at mail.ee)
 * Copyright (c) 2006-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2004-2006 Mickael Graf (korbinus at xfce.org)
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <time.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkx.h>

#include "orage-i18n.h"
#include "functions.h"
#include "mainbox.h"
#include "about-xfcalendar.h"
#include "ical-code.h"
#include "event-list.h"
#include "appointment.h"
#include "parameters.h"
#include "tray_icon.h"
#include "../globaltime/globaltime.h"

#ifdef HAVE_LIBXFCE4UI
#include <libxfce4ui/libxfce4ui.h>
#endif

#define ORAGE_TRAYICON ((GtkStatusIcon *)g_par.trayIcon)

static GtkStyleContext *get_style (GtkStyleContext *parent,
                                   const char *selector)
{
    GtkWidgetPath *path;
    GtkStyleContext *context;

    if (parent)
        path = gtk_widget_path_copy (gtk_style_context_get_path (parent));
    else
        path = gtk_widget_path_new ();

    gtk_widget_path_append_type (path, G_TYPE_NONE);
    gtk_widget_path_iter_set_object_name (path, -1, selector);

    context = gtk_style_context_new ();
    gtk_style_context_set_path (context, path);
    gtk_style_context_set_parent (context, parent);

    /* Unfortunately, we have to explicitly set the state again here for it to
     * take effect.
     */
    gtk_style_context_set_state (context,
                                 gtk_widget_path_iter_get_state (path, -1));
    gtk_widget_path_unref (path);

    return context;
}

static GtkWidget *orage_image_menu_item (const gchar *label,
                                         const gchar *icon_name)
{
#ifdef HAVE_LIBXFCE4UI
    return xfce_gtk_image_menu_item_new_from_icon_name (
            label, NULL, NULL, NULL, NULL, icon_name, NULL);
#else
    (void)icon_name;

    return gtk_menu_item_new_with_mnemonic (label);
#endif
}

static void on_Today_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                               gpointer user_data)
{
    struct tm *t;
    CalWin *xfcal = (CalWin *)user_data;

    t = orage_localtime();
    orage_select_date(GTK_CALENDAR(xfcal->mCalendar), t->tm_year+1900
            , t->tm_mon, t->tm_mday);
    (void)create_el_win (NULL);
}

static void on_preferences_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                     G_GNUC_UNUSED gpointer user_data)
{
    show_parameters ();
}

static void on_new_appointment_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                         G_GNUC_UNUSED gpointer user_data)
{
    struct tm *t;
    char cur_date[9];

    t = orage_localtime();
    g_snprintf(cur_date, sizeof (cur_date), "%04d%02d%02d", t->tm_year+1900
               , t->tm_mon+1, t->tm_mday);
    create_appt_win("NEW", cur_date);
}

static void on_globaltime_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                    G_GNUC_UNUSED gpointer user_data)
{
    GError *error = NULL;

    if (!orage_exec("globaltime", NULL, &error))
        g_message("%s: start of %s failed: %s", "Orage", "globaltime"
                , error->message);
}

static gboolean button_press_cb (G_GNUC_UNUSED GtkStatusIcon *status_icon,
                                 GdkEventButton *event,
                                 G_GNUC_UNUSED gpointer user_data)
{
    GdkAtom atom;
    Window xwindow;
    Display *display;
    XEvent xevent;

    if (event->type != GDK_BUTTON_PRESS) /* double or triple click */
        return(FALSE); /* ignore */
    else if (event->button == 2)
    {
        /* send message to program to check if it is running */
        atom = gdk_atom_intern (GLOBALTIME_RUNNING, FALSE);
        display = gdk_x11_get_default_xdisplay ();
        if ((xwindow = XGetSelectionOwner (display,
                gdk_x11_atom_to_xatom(atom))) != None) { /* yes, then toggle */
            xevent.xclient.type = ClientMessage;
            xevent.xclient.format = 8;
            xevent.xclient.message_type =
                    XInternAtom (display, GLOBALTIME_TOGGLE, FALSE);
            xevent.xclient.send_event = TRUE;
            xevent.xclient.window = xwindow;

            if (XSendEvent (display, xwindow, FALSE, NoEventMask, &xevent))
                g_debug(_("Raising GlobalTime window..."));
            else
                g_warning (_("GlobalTime window raise failed"));

            (void)XFlush (display);

            return(TRUE);
        }
        else { /* not running, let's try to start it. Need to reset TZ! */
            on_globaltime_activate(NULL, NULL);
            return(TRUE);
        }
    }

    return(FALSE);
}

static void toggle_visible_cb (G_GNUC_UNUSED GtkStatusIcon *status_icon,
                               G_GNUC_UNUSED gpointer user_data)
{
    orage_toggle_visible();
}

static void show_menu (G_GNUC_UNUSED GtkStatusIcon *status_icon,
                       G_GNUC_UNUSED guint button,
                       G_GNUC_UNUSED guint activate_time,
                       gpointer user_data)
{
    gtk_menu_popup_at_pointer ((GtkMenu *)user_data, NULL);
}

static gboolean format_line (PangoLayout *pl, struct tm *t, const char *data,
                             const PangoFontDescription *desc)
{
    gchar row[90];

    if (ORAGE_STR_EXISTS(data)) {
        if (strftime(row, sizeof (row) - 1, data, t) == 0) {
            g_warning("format_line: strftime %s failed", data);
            return(FALSE);
        }
        else
        {
            pango_layout_set_font_description (pl, desc);
            pango_layout_set_text (pl, row, -1);
            pango_layout_set_alignment (pl, PANGO_ALIGN_CENTER);
            return(TRUE);
        }
    }
    else
        return(FALSE);
}

static void draw_pango_layout (cairo_t *cr, PangoLayout *pl, gint x, gint y)
{
    cairo_move_to (cr, x, y);
    pango_cairo_show_layout (cr, pl);
}

static GtkStyleContext *get_row_style (GtkStyleContext *style_context,
                                       gint row_idx)
{
    char buf[8];

    (void)g_snprintf (buf, sizeof (buf) - 1 , "row%d", row_idx);

    return get_style (style_context, buf);
}

static const char *get_row_x_data (const gint line)
{
    char *row_x_data;

    switch (line)
    {
        case 1:
            row_x_data = g_par.own_icon_row1_data;
            break;

        case 2:
            row_x_data = g_par.own_icon_row2_data;
            break;

        case 3:
            row_x_data = g_par.own_icon_row3_data;
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    return row_x_data;
}

static gint find_y_offset (const gint line,
                           const gint height,
                           const gint y_size,
                           const gint16 border_top,
                           const gint16 border_bottom)
{
    gint y_offset;

    switch (line)
    {
        case 1:
            y_offset = border_top;
            break;

        case 2:
            y_offset = ((height - y_size) / 2) - border_top;
            break;

        case 3:
            y_offset = height - y_size - border_top - border_bottom;
            break;

        default:
            g_assert_not_reached ();
            break;
    }

    return y_offset;
}

static void create_own_icon_pango_layout (gint line,
                                          cairo_t *cr,
                                          GtkStyleContext *style_context,
                                          GtkBorder *border,
                                          struct tm *t,
                                          gint width,
                                          gint height)
{
    CalWin *xfcal = (CalWin *)g_par.xfcal;
    PangoRectangle real_rect, log_rect;
    gint x_size, x_offset, y_offset;
    gint y_size;
    PangoLayout *pl;
    PangoFontDescription *font_desc;
    GdkRGBA color;
    GtkStyleContext *sub_style_context;
    GtkStateFlags style_context_state;
    const char *row_x_data;

    g_assert ((line >= 1) && (line <= 3));

    pl = gtk_widget_create_pango_layout (xfcal->mWindow, "x");
    sub_style_context = get_row_style (style_context, line);
    style_context_state = gtk_style_context_get_state (sub_style_context);

    gtk_style_context_get (sub_style_context, style_context_state,
                           GTK_STYLE_PROPERTY_FONT, &font_desc, NULL);

    gtk_style_context_get_color (sub_style_context, style_context_state,
                                 &color);

    gdk_cairo_set_source_rgba (cr, &color);

    row_x_data = get_row_x_data (line);

    if (format_line (pl, t, row_x_data, font_desc) == FALSE)
    {
        g_object_unref (pl);
        g_warning ("icon line format failed");
        return;
    }

    pango_layout_get_pixel_extents (pl, &real_rect, &log_rect);
    x_size = log_rect.width;
    y_size = log_rect.height;

    x_offset = (width - x_size) / 2;
    y_offset = find_y_offset(line, height, y_size, border->top, border->bottom);

    draw_pango_layout (cr, pl, x_offset, y_offset);

    g_object_unref (pl);
}

static cairo_t *create_icon_background (cairo_surface_t *surface,
                                        GtkStyleContext *style_context,
                                        gint width, gint height)
{
    cairo_t *cr;

    cr = cairo_create (surface);
    gtk_render_background (style_context, cr, 0, 0, width, height);
    gtk_render_frame (style_context, cr, 0, 0, width, height);

    return cr;
}

static GdkPixbuf *create_dynamic_icon (void)
{
    const gint width = 160, height = 160; /* size of icon */
    cairo_t *cr;
    GdkPixbuf *pixbuf;
    struct tm *t;
    cairo_surface_t *surface;
    GtkStyleContext *style_context;
    GtkBorder border;

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    g_assert (surface != NULL);

    style_context = get_style (NULL, "orage_tray_icon");
    cr = create_icon_background (surface, style_context, width, height);

    gtk_style_context_get_border (style_context,
                                  gtk_style_context_get_state (style_context),
                                  &border);

    t = orage_localtime ();
    /* Date line must be first, as this background may be overlap with upper
     * or lower text areas.
     */
    create_own_icon_pango_layout (2, cr, style_context, &border, t,
                                  width, height);
    create_own_icon_pango_layout (1, cr, style_context, &border, t,
                                  width, height);
    create_own_icon_pango_layout (3, cr, style_context, &border, t,
                                  width, height);

    g_object_unref (style_context);
    pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
    cairo_paint (cr);
    cairo_surface_destroy (surface);
    cairo_destroy (cr);

    return pixbuf;
}

GdkPixbuf *orage_create_icon(gboolean static_icon, gint size)
{
    GError *error = NULL;
    GtkIconTheme *icon_theme = NULL;
    GdkPixbuf *pixbuf, *pixbuf2;

    icon_theme = gtk_icon_theme_get_default();
    if (static_icon)
    { /* load static icon */
        pixbuf = gtk_icon_theme_load_icon(icon_theme, "xfcalendar", size
                , GTK_ICON_LOOKUP_USE_BUILTIN, &error);
    }
    else { /***** dynamic icon build starts now *****/
        pixbuf = create_dynamic_icon ();

        if (size) {
            pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, size, size
                    , GDK_INTERP_BILINEAR);
            g_object_unref(pixbuf);
            pixbuf = pixbuf2;
        }

        if (pixbuf == NULL) {
            g_warning("orage_create_icon: dynamic icon creation failed\n");
            pixbuf = gtk_icon_theme_load_icon(icon_theme, "xfcalendar", size
                    , GTK_ICON_LOOKUP_USE_BUILTIN, &error);
        }
    }

    if (pixbuf == NULL) {
        g_warning ("orage_create_icon: static icon creation failed, "
                   "using default About icon");
        /* dynamic icon also tries static before giving up */
        pixbuf = gtk_icon_theme_load_icon(icon_theme, "help-about", size
                , GTK_ICON_LOOKUP_USE_BUILTIN, &error);
    }

    if (pixbuf == NULL)
    {
        g_warning("orage_create_icon: couldn’t load icon '%s'", error->message);
        g_error_free (error);
    }

    return(pixbuf);
}

static GtkWidget *create_TrayIcon_menu(void)
{
    CalWin *xfcal = (CalWin *)g_par.xfcal;
    GtkWidget *trayMenu;
    GtkWidget *menuItem;

    trayMenu = gtk_menu_new();

    menuItem = orage_image_menu_item (_("Today"), "go-home");
    g_signal_connect(menuItem, "activate"
            , G_CALLBACK(on_Today_activate), xfcal);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);

    menuItem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
    menuItem = orage_image_menu_item(_("New appointment"), "document-new");
    g_signal_connect(menuItem, "activate"
            , G_CALLBACK(on_new_appointment_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);

    menuItem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
    menuItem = orage_image_menu_item (_("Preferences"), "preferences-system");
    g_signal_connect(menuItem, "activate"
            , G_CALLBACK(on_preferences_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);

    menuItem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
    menuItem = orage_image_menu_item(_("Quit"), "application-exit");
    g_signal_connect(menuItem, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);

    menuItem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);
    menuItem = gtk_menu_item_new_with_label(_("Globaltime"));
    g_signal_connect(menuItem, "activate"
            , G_CALLBACK(on_globaltime_activate), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);

    gtk_widget_show_all(trayMenu);
    return(trayMenu);
}

static void destroy_TrayIcon(GtkStatusIcon *trayIcon)
{
    g_object_unref(trayIcon);
}

GtkStatusIcon* create_TrayIcon(GdkPixbuf *orage_logo)
{
    CalWin *xfcal = (CalWin *)g_par.xfcal;
    GtkWidget *trayMenu;
    GtkStatusIcon *trayIcon = NULL;

    /*
     * Create the tray icon popup menu
     */
    trayMenu = create_TrayIcon_menu();

    /*
     * Create the tray icon
     */
    trayIcon = orage_status_icon_new_from_pixbuf (orage_logo);
    g_object_ref(trayIcon);
    g_object_ref_sink(trayIcon);

    g_signal_connect(G_OBJECT(trayIcon), "button-press-event",
    			   G_CALLBACK(button_press_cb), xfcal);
    g_signal_connect(G_OBJECT(trayIcon), "activate",
    			   G_CALLBACK(toggle_visible_cb), xfcal);
    g_signal_connect(G_OBJECT(trayIcon), "popup-menu",
    			   G_CALLBACK(show_menu), trayMenu);
    return(trayIcon);
}

void refresh_TrayIcon(void)
{
    GdkPixbuf *orage_logo;

    orage_logo = orage_create_icon(FALSE, 0);
    if (!orage_logo) {
        return;
    }
    if (g_par.show_systray) { /* refresh tray icon */
        if (ORAGE_TRAYICON && orage_status_icon_is_embedded (ORAGE_TRAYICON)) {
            orage_status_icon_set_visible (ORAGE_TRAYICON, FALSE);
            destroy_TrayIcon(ORAGE_TRAYICON);
        }
        g_par.trayIcon = create_TrayIcon(orage_logo);
        orage_status_icon_set_visible (ORAGE_TRAYICON, TRUE);
    }
    gtk_window_set_default_icon(orage_logo);
    /* main window is always active so we need to change it's icon also */
    gtk_window_set_icon(GTK_WINDOW(((CalWin *)g_par.xfcal)->mWindow)
            , orage_logo);
    g_object_unref(orage_logo);
}
