/*
 * Copyright (c) 2021-2023 Erkki Moorits
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
 *     Free Software Foundation
 *     51 Franklin Street, 5th Floor
 *     Boston, MA 02110-1301 USA
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
#include "orage-window.h"
#include "functions.h"
#include "ical-code.h"
#include "event-list.h"
#include "orage-appointment-window.h"
#include "parameters.h"
#include "tray_icon.h"

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
    GDateTime *gdt;

    gdt = g_date_time_new_now_local ();
    orage_select_date (orage_window_get_calendar (ORAGE_WINDOW (user_data)),
                       gdt);
    g_date_time_unref (gdt);
    (void)create_el_win (NULL);
}

static void on_preferences_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                     G_GNUC_UNUSED gpointer user_data)
{
    show_parameters ();
}

static void on_orage_quit_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                    gpointer orage_app)
{
    g_application_quit (G_APPLICATION (orage_app));
}

static void on_new_appointment_activate (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                         G_GNUC_UNUSED gpointer user_data)
{
    GtkWidget *appointment_window;

    appointment_window = orage_appointment_window_new_now ();
    gtk_window_present (GTK_WINDOW (appointment_window));
}

static void toggle_visible_cb (G_GNUC_UNUSED GtkStatusIcon *status_icon,
                               G_GNUC_UNUSED gpointer user_data)
{
    OrageApplication *app;
    GList *list;

    app = ORAGE_APPLICATION (g_application_get_default ());
    list = gtk_application_get_windows (GTK_APPLICATION (app));

    g_return_if_fail (list != NULL);

    if (gtk_widget_get_visible (GTK_WIDGET (list->data)))
    {
        write_parameters ();
        gtk_widget_hide (GTK_WIDGET (list->data));
    }
    else
        orage_window_raise (ORAGE_WINDOW (orage_application_get_window (app)));
}

static void show_menu (G_GNUC_UNUSED GtkStatusIcon *status_icon,
                       G_GNUC_UNUSED guint button,
                       G_GNUC_UNUSED guint activate_time,
                       gpointer user_data)
{
    gtk_menu_popup_at_pointer ((GtkMenu *)user_data, NULL);
}

static gboolean format_line (PangoLayout *pl, GDateTime *gdt, const gchar *fmt,
                             const PangoFontDescription *desc)
{
    gchar *date_str;

    if (ORAGE_STR_EXISTS(fmt))
    {
        date_str = g_date_time_format (gdt, fmt);
        if (date_str == NULL)
        {
            g_warning ("%s: g_date_time_format %s failed", G_STRFUNC, fmt);
            return(FALSE);
        }
        else
        {
            pango_layout_set_font_description (pl, desc);
            pango_layout_set_text (pl, date_str, -1);
            pango_layout_set_alignment (pl, PANGO_ALIGN_CENTER);
            g_free (date_str);
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

    (void)g_snprintf (buf, sizeof (buf) - 1 , "row-%d", row_idx);

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
                                          GDateTime *gdt,
                                          gint width,
                                          gint height)
{
    PangoRectangle real_rect, log_rect;
    gint x_size, x_offset, y_offset;
    gint y_size;
    PangoLayout *pl;
    PangoFontDescription *font_desc;
    GdkRGBA color;
    GtkStyleContext *sub_style_context;
    GtkStateFlags style_context_state;
    OrageApplication *app;
    const char *row_x_data;

    g_assert ((line >= 1) && (line <= 3));

    app = ORAGE_APPLICATION (g_application_get_default ());
    pl = gtk_widget_create_pango_layout (orage_application_get_window (app), "x");
    sub_style_context = get_row_style (style_context, line);
    style_context_state = gtk_style_context_get_state (sub_style_context);

    gtk_style_context_get (sub_style_context, style_context_state,
                           GTK_STYLE_PROPERTY_FONT, &font_desc, NULL);

    gtk_style_context_get_color (sub_style_context, style_context_state,
                                 &color);

    gdk_cairo_set_source_rgba (cr, &color);

    row_x_data = get_row_x_data (line);

    if (format_line (pl, gdt, row_x_data, font_desc) == FALSE)
    {
        g_object_unref (pl);
        pango_font_description_free (font_desc);
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
    pango_font_description_free (font_desc);
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
    GDateTime *gdt;
    cairo_surface_t *surface;
    GtkStyleContext *style_context;
    GtkBorder border;

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    g_assert (surface != NULL);

    style_context = get_style (NULL, "OrageTrayIcon");
    cr = create_icon_background (surface, style_context, width, height);

    gtk_style_context_get_border (style_context,
                                  gtk_style_context_get_state (style_context),
                                  &border);

    gdt = g_date_time_new_now_local ();
    /* Date line must be first, as this background may be overlap with upper
     * or lower text areas.
     */
    create_own_icon_pango_layout (2, cr, style_context, &border, gdt,
                                  width, height);
    create_own_icon_pango_layout (1, cr, style_context, &border, gdt,
                                  width, height);
    create_own_icon_pango_layout (3, cr, style_context, &border, gdt,
                                  width, height);

    g_date_time_unref (gdt);
    g_object_unref (style_context);
    pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
    cairo_paint (cr);
    cairo_surface_destroy (surface);
    cairo_destroy (cr);

    return pixbuf;
}

static GdkPixbuf *orage_create_icon (void)
{
    GError *error = NULL;
    GtkIconTheme *icon_theme = NULL;
    GdkPixbuf *pixbuf;

    icon_theme = gtk_icon_theme_get_default();

    pixbuf = create_dynamic_icon ();

    if (pixbuf == NULL)
    {
        g_warning ("%s: dynamic icon creation failed", G_STRFUNC);
        pixbuf = gtk_icon_theme_load_icon (icon_theme, ORAGE_APP_ID, 0,
                                           GTK_ICON_LOOKUP_USE_BUILTIN, &error);
    }

    if (pixbuf == NULL) {
        g_warning ("%s: static icon creation failed, using default About icon",
                   G_STRFUNC);
        /* dynamic icon also tries static before giving up */
        pixbuf = gtk_icon_theme_load_icon (icon_theme, "help-about", 0,
                                           GTK_ICON_LOOKUP_USE_BUILTIN, &error);
    }

    if (pixbuf == NULL)
    {
        g_warning ("%s: couldn’t load icon '%s'", G_STRFUNC, error->message);
        g_error_free (error);
    }

    return(pixbuf);
}

static GtkWidget *create_TrayIcon_menu(void)
{
    GtkWidget *trayMenu;
    GtkWidget *menuItem;
    OrageApplication *app;

    trayMenu = gtk_menu_new();

    menuItem = orage_image_menu_item (_("Today"), "go-home");
    app = ORAGE_APPLICATION (g_application_get_default ());
    g_signal_connect (menuItem, "activate", G_CALLBACK(on_Today_activate),
                      orage_application_get_window (app));
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
    g_signal_connect (menuItem, "activate",
                      G_CALLBACK (on_orage_quit_activate), app);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);

    menuItem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), menuItem);

    gtk_widget_show_all(trayMenu);
    return(trayMenu);
}

GtkStatusIcon *orage_create_trayicon (void)
{
    GtkWidget *trayMenu;
    GtkStatusIcon *trayIcon = NULL;
    GdkPixbuf *orage_logo;

    orage_logo = orage_create_icon ();
    g_return_val_if_fail (orage_logo != NULL, NULL);

    /*
     * Create the tray icon
     */
    trayIcon = orage_status_icon_new_from_pixbuf (orage_logo);
    g_object_unref (orage_logo);
    g_object_ref(trayIcon);
    g_object_ref_sink(trayIcon);

    /* Create the tray icon popup menu. */
    trayMenu = create_TrayIcon_menu();

    g_signal_connect (G_OBJECT (trayIcon), "activate",
                      G_CALLBACK (toggle_visible_cb), NULL);
    g_signal_connect(G_OBJECT(trayIcon), "popup-menu",
    			   G_CALLBACK(show_menu), trayMenu);
    return(trayIcon);
}

void orage_refresh_trayicon(void)
{
    GtkStatusIcon *trayIcon;

    if (g_par.show_systray) { /* refresh tray icon */
        if (ORAGE_TRAYICON) {
            orage_status_icon_set_visible (ORAGE_TRAYICON, FALSE);
            g_object_unref (ORAGE_TRAYICON);
        }

        trayIcon = orage_create_trayicon ();
        g_return_if_fail (trayIcon != NULL);

        orage_status_icon_set_visible (trayIcon, TRUE);
        g_par.trayIcon = trayIcon;
    }
}
