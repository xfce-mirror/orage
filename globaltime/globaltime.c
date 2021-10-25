/*  
 *  Global Time - Set of clocks showing time in different parts of world.
 *  Copyright 2006-2011 Juha Kautto (kautto.juha@kolumbus.fi)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  To get a copy of the GNU General Public License write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 /*
  TODO: 
 *DONE* 1. Add parameter page (and clean the nasty 0 clock separator issue)
 *DONE* 2. Clean Xfce clock integration size requests. Now it is pretty ugly
        3. Add event for moving and store location when it is changed.
           Currently location is stored only when some clock is changed.
        4. Add Daylight Saving Time displays
        5. What if you have more clocks than what fits in screen ?
 *DONE* 6. Fix core dump when location coordinates are missing from XML file
 *DONE* 7. Create missing directories for XML parameter file
 *DONE* 8. Remove "automatic" setting from decorations
 *DONE* 9. Fix panel plugin visibity like orage
 *DONE*10. Fix panel plugin position.
       11. Fix settings formatting. and add close button like in orage
 *DONE*12. Fix clock formatting after hour adjustment ends (free space)
  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include "globaltime.h"
#include "../src/orage-i18n.h"


#define NAME_VERSION "Global Time (2.2)"


global_times_struct clocks;

char *attr_underline[] = 
    { N_("no underline"), N_("single"), N_("double"), N_("low"), "END" } ;


static struct tm *get_time(const gchar *tz)
{
    time_t t;
    static struct tm now;
    static char env_tz[256];

#if 0
    setenv("TZ", tz, 1);
#endif
    g_snprintf(env_tz, sizeof (env_tz), "TZ=%s", tz);
    putenv(env_tz);
    tzset();
    if (clocks.time_adj_act) {
        t = clocks.previous_t + clocks.mm_adj*60 + clocks.hh_adj*3600;
    }
    else {
        time(&t);
    }
    localtime_r(&t, &now);

    return(&now);
}

static gboolean global_time_active_already(GdkAtom *atom)
{
#if (GTK_MAJOR_VERSION < 3)
    Window xwindow;
    GdkEventClient gev; 

    *atom = gdk_atom_intern (GLOBALTIME_RUNNING, FALSE);
    xwindow = XGetSelectionOwner(gdk_x11_get_default_xdisplay(),
                                 gdk_x11_atom_to_xatom(*atom));
    if (xwindow != None) { /* real owner found; must be us. Let's go visible */
/* DOES NOT WORK: if ((window = gdk_selection_owner_get(atom)) != NULL) { */
        gev.type = GDK_CLIENT_EVENT;
        gev.window = NULL;
        gev.send_event = TRUE;
        gev.message_type = gdk_atom_intern ("_XFCE_GLOBALTIME_RAISE", FALSE);
        gev.data_format = 8; 
        if (gdk_event_send_client_message ((GdkEvent *) &gev
                    , (GdkNativeWindow) xwindow))
            g_message(_("Raising GlobalTime window..."));
        else
            g_warning(_("GlobalTime window raise failed"));
        return(TRUE); 
    }
    else
#else
#warning "TODO for GTK 3"
        (void)atom;
#endif
        return(FALSE); 
}

#if (GTK_MAJOR_VERSION < 3)
static void raise_window(void)
{
    GdkWindow *window;

    gtk_window_set_decorated(GTK_WINDOW(clocks.window), clocks.decorations);
    gtk_window_move(GTK_WINDOW(clocks.window), clocks.x, clocks.y);
    gtk_window_stick(GTK_WINDOW(clocks.window));
    window = gtk_widget_get_window(GTK_WIDGET(clocks.window));
    gdk_x11_window_set_user_time(window, gdk_x11_get_server_time(window));
    gtk_widget_show(clocks.window);
    gtk_window_present(GTK_WINDOW(clocks.window));
}

static gboolean client_message_received(GtkWidget *widget
            , GdkEventClient *event, gpointer user_data)
{
    if (event->message_type == 
            gdk_atom_intern("_XFCE_GLOBALTIME_RAISE", FALSE)) {
        raise_window();
        return(TRUE);
    }
    else if (event->message_type ==
            gdk_atom_intern (GLOBALTIME_TOGGLE, FALSE)) {
        if (gtk_widget_get_visible(clocks.window)) {
            gtk_widget_hide(clocks.window);
            return(TRUE);
        }
        raise_window();
        return(TRUE);
    }
                                                                                
    return(FALSE);
}
#else
#warning "TODO for GTK 3"
static gboolean client_message_received(GtkWidget *widget
            , void *event, gpointer user_data)
{                 
    (void)widget;
    (void)event;
    (void)user_data;
    return(FALSE);
}
#endif

static gboolean clock_button_pressed(GtkWidget *widget, GdkEventButton *event
        , clock_struct *clockp)
{
    (void)event;
    return(clock_parameters(widget, clockp));
}

static void show_clock_format_attr(GString *tmp, GString *underline)
{
    if (underline->len != 0)
        g_string_append_printf(tmp, " underline=\"%s\"", underline->str);
}

static void show_clock_format_name(clock_struct *clockp)
{
    GString *tmp;

    if ((clockp->clock_attr.name_underline_modified
        && !strcmp(clockp->clock_attr.name_underline->str
                , attr_underline[0]))
    ||  (!clockp->clock_attr.name_underline_modified
        && !strcmp(clocks.clock_default_attr.name_underline->str
                , attr_underline[0]))) {
    /* either modified and "no underlne"
     * or default and default = "no underline"
     *  => simple, basic text */
        gtk_label_set_text(GTK_LABEL(clockp->name_label), clockp->name->str);
    }
    else { /* underline exists, need special processing */
        tmp = g_string_new("<span");
        if (clockp->clock_attr.name_underline_modified)
            show_clock_format_attr(tmp, clockp->clock_attr.name_underline);
        else
            show_clock_format_attr(tmp
                    , clocks.clock_default_attr.name_underline);
        g_string_append_printf(tmp, ">%s</span>", clockp->name->str);
        gtk_label_set_markup(GTK_LABEL(clockp->name_label), tmp->str);
        g_string_free(tmp, TRUE);
    }
}

static void show_clock_format_time(clock_struct *clockp)
{
    GString *tmp;

    if ((clockp->clock_attr.time_underline_modified
        && !strcmp(clockp->clock_attr.time_underline->str
                , attr_underline[0]))
    ||  (!clockp->clock_attr.time_underline_modified
        && !strcmp(clocks.clock_default_attr.time_underline->str
                , attr_underline[0]))) {
    /* either modified and "no underlne"
     * or default and default = "no underline"
     *  => simple, basic text */
        gtk_label_set_text(GTK_LABEL(clockp->time_label), clocks.time_now);
    }
    else { /* underline exists, need special processing */
        tmp = g_string_new("<span");
        if (clockp->clock_attr.time_underline_modified)
            show_clock_format_attr(tmp, clockp->clock_attr.time_underline);
        else
            show_clock_format_attr(tmp
                    , clocks.clock_default_attr.time_underline);
        g_string_append_printf(tmp, ">%s</span>", clocks.time_now);
        gtk_label_set_markup(GTK_LABEL(clockp->time_label), tmp->str);
        g_string_free(tmp, TRUE);
    }
}

static void show_clock_format_clock(clock_struct *clockp)
{
    gchar tmp[100];
    PangoFontDescription *font;

/*********** Clock Name font ***********/
    if (clockp->clock_attr.name_font_modified) {
        font = pango_font_description_from_string(
                clockp->clock_attr.name_font->str);
        gtk_widget_modify_font(clockp->name_label, font);
        pango_font_description_free(font);
    }
    else if (clocks.clock_default_attr.name_font_modified) {
        font = pango_font_description_from_string(
                clocks.clock_default_attr.name_font->str);
        gtk_widget_modify_font(clockp->name_label, font);
        pango_font_description_free(font);
    }
    else 
        gtk_widget_modify_font(clockp->name_label, NULL);

/*********** Clock Time font ***********/
    if (clockp->clock_attr.time_font_modified) {
        font = pango_font_description_from_string(
                clockp->clock_attr.time_font->str);
        gtk_widget_modify_font(clockp->time_label, font);
        pango_font_description_free(font);
    }
    else if (clocks.clock_default_attr.time_font_modified) {
        font = pango_font_description_from_string(
                clocks.clock_default_attr.time_font->str);
        gtk_widget_modify_font(clockp->time_label, font);
        pango_font_description_free(font);
    }
    else 
        gtk_widget_modify_font(clockp->time_label, NULL);

/*********** Clock background ***********/
    if (clockp->clock_attr.clock_bg_modified)
        gtk_widget_modify_bg(clockp->clock_ebox, GTK_STATE_NORMAL
                , clockp->clock_attr.clock_bg);
    else if (clocks.clock_default_attr.clock_bg_modified)
        gtk_widget_modify_bg(clockp->clock_ebox, GTK_STATE_NORMAL
                , clocks.clock_default_attr.clock_bg);
    else
        gtk_widget_modify_bg(clockp->clock_ebox, GTK_STATE_NORMAL, NULL);

/*********** Clock foreground ***********/
    if (clockp->clock_attr.clock_fg_modified) {
        gtk_widget_modify_fg(clockp->name_label, GTK_STATE_NORMAL
                , clockp->clock_attr.clock_fg);
        gtk_widget_modify_fg(clockp->time_label, GTK_STATE_NORMAL
                , clockp->clock_attr.clock_fg);
    }
    else if (clocks.clock_default_attr.clock_fg_modified) {
        gtk_widget_modify_fg(clockp->name_label, GTK_STATE_NORMAL
                , clocks.clock_default_attr.clock_fg);
        gtk_widget_modify_fg(clockp->time_label, GTK_STATE_NORMAL
                , clocks.clock_default_attr.clock_fg);
    }
    else {
        gtk_widget_modify_fg(clockp->time_label, GTK_STATE_NORMAL, NULL);
        gtk_widget_modify_fg(clockp->name_label, GTK_STATE_NORMAL, NULL);
    }

/*********** timezone tooltip ***********/
    g_snprintf (tmp, sizeof (tmp), _("%s\nclick to modify clock"),
                clockp->tz->str);
    gtk_widget_set_tooltip_text(clockp->clock_ebox, tmp);

/*********** clock size even or varying */
    gtk_box_set_homogeneous(GTK_BOX(clocks.clocks_hbox), clocks.expand);
    gtk_box_set_child_packing(GTK_BOX(clocks.clocks_hbox)
            , clockp->clock_hbox
            , clocks.expand, clocks.expand, 0, GTK_PACK_START);
    gtk_box_set_child_packing(GTK_BOX(clockp->clock_hbox)
            , clockp->clock_separator
            , FALSE, FALSE, 0, GTK_PACK_START);
    gtk_box_set_child_packing(GTK_BOX(clockp->clock_hbox)
            , clockp->clock_ebox
            , clocks.expand, clocks.expand, 0, GTK_PACK_START);

    gtk_window_resize(GTK_WINDOW(clocks.window), 10, 10);
}

void show_clock(clock_struct *clockp, gint *pos)
{
    #warning "TODO replace box with grid"
    clockp->clock_hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(clocks.clocks_hbox), clockp->clock_hbox
                        , clocks.expand, clocks.expand, 0);
    if (*pos !=- 1) /* -1=to the end (default) */
        gtk_box_reorder_child(GTK_BOX(clocks.clocks_hbox)
                        , clockp->clock_hbox, *pos);
    gtk_widget_show(clockp->clock_hbox);

    clockp->clock_separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(clockp->clock_hbox)
                    , clockp->clock_separator, FALSE, FALSE, 0);
    gtk_widget_show(clockp->clock_separator);

    clockp->clock_ebox = gtk_event_box_new();
    gtk_box_pack_start(GTK_BOX(clockp->clock_hbox), clockp->clock_ebox
                    , clocks.expand, clocks.expand, 0);
    g_signal_connect(G_OBJECT(clockp->clock_ebox), "button_press_event"
                    , G_CALLBACK(clock_button_pressed), clockp);
    gtk_widget_show(clockp->clock_ebox);
    clockp->clock_vbox = gtk_grid_new ();
    gtk_container_set_border_width(GTK_CONTAINER(clockp->clock_vbox), 1);
    gtk_container_add(GTK_CONTAINER(clockp->clock_ebox), clockp->clock_vbox);
    gtk_widget_show(clockp->clock_vbox);

/*********** Clock Name ***********/
    clockp->name_label = gtk_label_new(NULL);
    show_clock_format_name(clockp);
    gtk_box_pack_start(GTK_BOX(clockp->clock_vbox), clockp->name_label
            , FALSE, FALSE, 0);
    gtk_grid_attach_next_to (GTK_GRID (clockp->clock_vbox), clockp->name_label,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    gtk_widget_show(clockp->name_label);

/*********** Timezone and actual time ***********/
    clockp->time_label = gtk_label_new(NULL);
    show_clock_format_time(clockp);
    gtk_grid_attach_next_to (GTK_GRID (clockp->clock_vbox), clockp->time_label,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    gtk_widget_show(clockp->time_label);

/*********** font and color ***************/
    show_clock_format_clock(clockp);
}


static gboolean preferences_button_pressed(GtkWidget *widget
        , GdkEventButton *event, gpointer dummy)
{
    struct tm *now;

    if (event->button == 2) { /* toggle spinbox sensitivity */
        if (clocks.time_adj_act) { /* end it */
            gtk_widget_hide(clocks.hdr_adj_mm);
            gtk_widget_hide(clocks.hdr_adj_sep);
            gtk_widget_hide(clocks.hdr_adj_hh);
            clocks.time_adj_act = FALSE;
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(clocks.hdr_adj_hh)
                    , (gdouble)0);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(clocks.hdr_adj_mm)
                    , (gdouble)0);
            clocks.mm_adj = 0;
            clocks.hh_adj = 0;
            clocks.previous_secs = 70; /* trick to refresh clocks once */
            gtk_window_resize(GTK_WINDOW(clocks.window), 10, 10);
        }
        else { /* start hour adjusting mode */
            /* let's try to guess good starting value = to get times to 
             * 0 or 30 minutes. */
            now = gmtime(&clocks.previous_t);
            clocks.mm_adj = (now->tm_min < 30 ? 0 : 60) - now->tm_min;
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(clocks.hdr_adj_mm)
                    , (gdouble)clocks.mm_adj);
            gtk_widget_show(clocks.hdr_adj_mm);
            gtk_widget_show(clocks.hdr_adj_sep);
            gtk_widget_show(clocks.hdr_adj_hh);
            clocks.time_adj_act = TRUE;
        }
        return(FALSE);
    }
    else {
        return(default_preferences(widget));
    }
    
    (void)dummy;
}

static void add_default_clock(void)
{
    clock_struct *clockp;

    clockp = g_new(clock_struct, 1);
    clockp->tz = g_string_new(clocks.local_tz->str);
    clockp->name = g_string_new(_("Localtime"));
    clockp->modified = FALSE;
    init_attr(&clockp->clock_attr);
    clocks.clock_list = g_list_append(clocks.clock_list, clockp);
    write_file();
}

static void upd_clock(clock_struct *clockp, gpointer user_data)
{
    struct tm *now;

    now = get_time(clockp->tz->str);
    if (clocks.local_mday == now->tm_mday) 
    {
        g_snprintf (clocks.time_now, sizeof (clocks.time_now), "%02d:%02d",
                    now->tm_hour, now->tm_min);
    }
    else if (clocks.local_mday > now->tm_mday)
    {
        g_snprintf (clocks.time_now, sizeof (clocks.time_now), "%02d:%02d-",
                    now->tm_hour, now->tm_min);
    }
    else
    {
        g_snprintf (clocks.time_now, sizeof (clocks.time_now), "%02d:%02d+",
                    now->tm_hour, now->tm_min);
    }
    
    show_clock_format_time(clockp);
    if (clocks.modified > 0) {
        show_clock_format_name(clockp);
        show_clock_format_clock(clockp);
    }
    
    (void)user_data;
}

static gboolean upd_clocks (gpointer user_data)
{
    struct tm *now;
    gint secs_now;

    if (clocks.no_update)
        return(TRUE);

    now = get_time(clocks.local_tz->str); /* GMT is returmed if not found */
    if (!clocks.time_adj_act) {
        time(&clocks.previous_t);
        clocks.local_mday = now->tm_mday;
        secs_now = now->tm_sec;
    }
    else {
        secs_now = 70; /* force update after time adjust ends */
    }

/* the following if is just for saving CPU: update clocks only when needed */
    if ((clocks.modified > 0) || (clocks.time_adj_act)
            || (secs_now  < clocks.previous_secs))
        /* minute changed => need to update visible clocks */
        g_list_foreach(clocks.clock_list, (GFunc) upd_clock, NULL);
    clocks.previous_secs = secs_now;
    
    (void)user_data;
    
    return(TRUE);
}

static void adj_hh_changed(GtkSpinButton *cb, gpointer user_data)
{
    clocks.hh_adj = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cb));
    
    (void)user_data;
}

static void adj_mm_changed(GtkSpinButton *cb, gpointer user_data)
{
    clocks.mm_adj = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cb));
    
    (void)user_data;
}

static void init_hdr_button(void)
{
    GtkWidget *image;

    /* this shows also text, which we do not want:
    clocks.hdr_button = gtk_button_new_from_stock(GTK_STOCK_PREFERENCES);
    */
    clocks.hdr_hbox = gtk_grid_new ();
    gtk_grid_attach_next_to (GTK_GRID (clocks.main_hbox), clocks.hdr_hbox, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_show(clocks.hdr_hbox);

    clocks.hdr_button = gtk_button_new();
    gtk_grid_attach_next_to (GTK_GRID (clocks.hdr_hbox), clocks.hdr_button,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(clocks.hdr_button
            , _("button 1 to change preferences \nbutton 2 to adjust time of clocks"));
    g_signal_connect(clocks.hdr_button, "button_press_event"
            , G_CALLBACK(preferences_button_pressed), NULL);
    image = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES
            , GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(clocks.hdr_button), GTK_WIDGET(image));
    gtk_widget_show(image);
    gtk_widget_show(clocks.hdr_button);

    clocks.hdr_adj_hh = gtk_spin_button_new_with_range(-24, 24, 1);
    gtk_grid_attach_next_to (GTK_GRID (clocks.hdr_hbox), clocks.hdr_adj_hh,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(clocks.hdr_adj_hh), (gdouble)0);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(clocks.hdr_adj_hh), TRUE);
    gtk_widget_set_size_request(clocks.hdr_adj_hh, 45, -1);
    gtk_widget_set_tooltip_text(clocks.hdr_adj_hh, _("adjust to change hour"));
    g_signal_connect((gpointer) clocks.hdr_adj_hh, "changed"
            , G_CALLBACK(adj_hh_changed), NULL);

    clocks.hdr_adj_sep = gtk_label_new(":");
    gtk_grid_attach_next_to (GTK_GRID (clocks.hdr_hbox), clocks.hdr_adj_sep,
                             NULL, GTK_POS_RIGHT, 1, 1);

    clocks.hdr_adj_mm = gtk_spin_button_new_with_range(-60, 60, 1);
    gtk_grid_attach_next_to (GTK_GRID (clocks.hdr_hbox), clocks.hdr_adj_mm,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(clocks.hdr_adj_mm), (gdouble)0);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(clocks.hdr_adj_mm), TRUE);
    gtk_spin_button_set_increments(GTK_SPIN_BUTTON(clocks.hdr_adj_mm), 30, 1);
    gtk_widget_set_size_request(clocks.hdr_adj_mm, 45, -1);
    gtk_widget_set_tooltip_text(clocks.hdr_adj_mm
            , _("adjust to change minute. Click arrows with button 2 to change only 1 minute."));
    g_signal_connect((gpointer) clocks.hdr_adj_mm, "changed"
            , G_CALLBACK(adj_mm_changed), NULL);
    /* We want it to be hidden initially, it is special thing to do...
    gtk_widget_show(clocks.hdr_adj);
    */
}

static gboolean clean_up(GtkWidget *obj, GdkEvent *event, gpointer data)
{
    write_file();
    
    (void)obj;
    (void)event;
    (void)data;
    
    return(FALSE);
}

static void create_icon(void)
{
    GdkPixbuf *clock_logo;
    GtkIconTheme *icon_theme;

    icon_theme = gtk_icon_theme_get_default();
    clock_logo = gtk_icon_theme_load_icon(icon_theme, "orage_globaltime", 0
            , GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
    gtk_window_set_default_icon(clock_logo);
    gtk_window_set_icon(GTK_WINDOW(clocks.window), clock_logo);
    g_object_unref(clock_logo);
}

static void initialize_clocks(void)
{
    GtkWidget *button;

    clocks.clock_list = NULL;
    g_strlcpy (clocks.time_now, "88:88", sizeof (clocks.time_now));
    clocks.previous_secs = 61;      
    clocks.time_adj_act = FALSE;      
    clocks.no_update = FALSE;
    clocks.hh_adj = 0;      
    clocks.mm_adj = 0;      
    /* done in read_file
    clocks.local_tz= g_string_new("/etc/localtime");
    clocks.x = 0;
    clocks.y = 0;
    clocks.decorations = TRUE;
    clocks.expand = FALSE;
    */
    clocks.local_mday = 0;
    clocks.modified = 0;

    /* Standard window-creating stuff */
    clocks.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(clocks.window), _("Global Time"));

    clocks.main_hbox = gtk_grid_new ();
    gtk_container_add(GTK_CONTAINER(clocks.window), clocks.main_hbox);
    gtk_widget_show(clocks.main_hbox);

    init_hdr_button();

    #warning "TODO replace box with grid"
    clocks.clocks_hbox = gtk_hbox_new(clocks.expand, 1);
    gtk_grid_attach_next_to (GTK_GRID (clocks.main_hbox), clocks.clocks_hbox,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_widget_show(clocks.clocks_hbox);

    clocks.clock_default_attr.clock_fg = g_new0(GdkColor,1);
    clocks.clock_default_attr.clock_fg_modified = FALSE;
    clocks.clock_default_attr.clock_bg = g_new0(GdkColor,1);
    clocks.clock_default_attr.clock_fg_modified = FALSE;
    clocks.clock_default_attr.name_font = g_string_new("");
    clocks.clock_default_attr.name_font_modified = FALSE;
    clocks.clock_default_attr.name_underline = g_string_new(attr_underline[0]);
    clocks.clock_default_attr.name_underline_modified = FALSE;
    clocks.clock_default_attr.time_font = g_string_new("");
    clocks.clock_default_attr.time_font_modified = FALSE;
    clocks.clock_default_attr.time_underline = g_string_new(attr_underline[0]);
    clocks.clock_default_attr.time_underline_modified = FALSE;
    /* find default font */
    button = gtk_font_button_new();
    g_string_assign(clocks.clock_default_attr.name_font
            , gtk_font_button_get_font_name((GtkFontButton *)button));
    g_string_assign(clocks.clock_default_attr.time_font
            , clocks.clock_default_attr.name_font->str);
    g_object_ref_sink(button);

    g_signal_connect(G_OBJECT(clocks.window), "delete-event"
            , G_CALLBACK(clean_up), NULL);
    g_signal_connect(G_OBJECT(clocks.window), "destroy"
            , G_CALLBACK(gtk_main_quit), NULL);
}

static void create_global_time(void)
{
    gint pos = -1; /* to the end */
    GdkAtom atom;
    GdkWindow *window;

    /* first check if we are active already */
    if (global_time_active_already(&atom))
        exit(1);

    /* mark us active */
    clocks.hidden = gtk_invisible_new();
    gtk_widget_show(clocks.hidden);

    window = gtk_widget_get_window (clocks.hidden);
    if (!gdk_selection_owner_set(window, atom,
            gdk_x11_get_server_time(window), FALSE)) {
        g_warning("Unable acquire ownership of selection");
    }

    initialize_clocks();
    create_icon();
    read_file(); /* fills clock_list non GTK members */

    if (g_list_length(clocks.clock_list) == 0)
        add_default_clock();

    /*
    get_time("GMT");
    */
    g_timeout_add(500, (GSourceFunc)upd_clocks, NULL);

    g_list_foreach(clocks.clock_list, (GFunc) show_clock, &pos);

    gtk_window_move(GTK_WINDOW(clocks.window), clocks.x, clocks.y);
    gtk_window_set_decorated(GTK_WINDOW(clocks.window), clocks.decorations);
    gtk_widget_show(clocks.window);

    g_signal_connect(clocks.hidden, "client-event"
            , G_CALLBACK(client_message_received), NULL);
}

int main(int argc, char *argv[])
{
    /* init i18n = nls to use gettext */
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif
    textdomain(GETTEXT_PACKAGE);

    gtk_init(&argc, &argv);
    create_global_time();
    gtk_main();

    return(0);
}
