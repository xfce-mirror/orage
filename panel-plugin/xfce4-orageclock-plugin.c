/* vim: set expandtab ts=4 sw=4: */
/*
 *
 *  Copyright © 2006-2015 Juha Kautto <juha@xfce.org>
 *
 *  it under the terms of the GNU Library General Public License as published 
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Authors:
 *      Juha Kautto <juha@xfce.org>
 *      Based on XFce panel plugin clock and date-time plugin
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-plugin-provider.h>

#include "../globaltime/globaltime.h"
#include "../src/functions.h"
#include "xfce4-orageclock-plugin.h"

/* -------------------------------------------------------------------- *
 *                               Clock                                  *
 * -------------------------------------------------------------------- */

static void oc_utf8_strftime(char *res, int res_l, char *format, struct tm *tm)
{
    char *tmp = NULL;

    /* strftime is nasty. It returns formatted characters (%A...) in utf8
     * but it does not convert plain characters so they will be in locale 
     * charset. 
     * It expects format to be in locale charset, so we need to convert 
     * that first (it may contain utf8).
     * We need then convert the results finally to utf8.
     * */
    tmp = g_locale_from_utf8(format, -1, NULL, NULL, NULL);
    strftime(res, res_l, tmp, tm);
    g_free(tmp);
    /* Then convert to utf8 if needed */
    if (!g_utf8_validate(res, -1, NULL)) {
        tmp = g_locale_to_utf8(res, -1, NULL, NULL, NULL);
        if (tmp) {
            g_strlcpy(res, tmp, res_l);
            g_free(tmp);
        }
    }
}

void oc_line_font_set(ClockLine *line)
{
    PangoFontDescription *font;
    PangoAttribute *attr;
    PangoAttrList *attrlist;

    if (line->font->str) {
        font = pango_font_description_from_string(line->font->str);
        attr = pango_attr_font_desc_new (font);
        pango_font_description_free(font);
        attrlist = pango_attr_list_new ();
        pango_attr_list_insert (attrlist, attr);
        gtk_label_set_attributes (GTK_LABEL(line->label), attrlist);
        pango_attr_list_unref (attrlist);
    }
    else
        gtk_label_set_attributes (GTK_LABEL(line->label), NULL);
}

void oc_line_rotate(OragePlugin *plugin, ClockLine *line)
{
    switch (plugin->rotation) {
        case 0:
            gtk_label_set_angle(GTK_LABEL(line->label), 0);
            break;
        case 1:
            gtk_label_set_angle(GTK_LABEL(line->label), 90);
            break;
        case 2:
            gtk_label_set_angle(GTK_LABEL(line->label), 270);
            break;
    }
}

void oc_set_line(OragePlugin *plugin, ClockLine *clock_line, int pos)
{
    clock_line->label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(plugin->mbox), clock_line->label
            , FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(plugin->mbox), clock_line->label, pos);
    oc_line_font_set(clock_line);
    oc_line_rotate(plugin, clock_line);
    gtk_widget_show(clock_line->label);
    /* clicking does not work after this
    gtk_label_set_selectable(GTK_LABEL(clock_line->label), TRUE);
    */
}

static void oc_set_lines_to_panel(OragePlugin *plugin)
{
    ClockLine *clock_line;
    GList   *tmp_list;

    if (plugin->lines_vertically)
        plugin->mbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    else
        plugin->mbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    gtk_widget_show(plugin->mbox);
    gtk_container_add(GTK_CONTAINER(plugin->frame), plugin->mbox);

    for (tmp_list = g_list_first(plugin->lines);
            tmp_list;
         tmp_list = g_list_next(tmp_list)) {
        clock_line = tmp_list->data;
        /* make sure clock face is updated */
        g_strlcpy (clock_line->prev, "New line", sizeof (clock_line->prev));
        oc_set_line(plugin, clock_line, -1);
    }
}

void oc_reorganize_lines(OragePlugin *plugin)
{
    /* let's just do this easily as it is very seldom called: 
       delete and recreate lines */
    gtk_widget_destroy(GTK_WIDGET(plugin->mbox));
    oc_set_lines_to_panel(plugin);
    oc_fg_set(plugin);
    oc_size_set(plugin);
}

static void oc_tooltip_set(OragePlugin *plugin)
{
    char res[OC_MAX_LINE_LENGTH-1];

    oc_utf8_strftime(res, sizeof(res), plugin->tooltip_data->str, &plugin->now);
    if (strcmp(res,  plugin->tooltip_prev)) {
        gtk_widget_set_tooltip_text (GTK_WIDGET (plugin), res);
        g_strlcpy (plugin->tooltip_prev, res, sizeof (plugin->tooltip_prev));
    }
}

static gboolean oc_get_time(OragePlugin *plugin)
{
    time_t  t;
    char    res[OC_MAX_LINE_LENGTH-1];
    ClockLine *line;
    GList   *tmp_list;

    time(&t);
    localtime_r(&t, &plugin->now);
    for (tmp_list = g_list_first(plugin->lines);
            tmp_list;
         tmp_list = g_list_next(tmp_list)) {
        line = tmp_list->data;
        oc_utf8_strftime(res, sizeof(res), line->data->str, &plugin->now);
        /* gtk_label_set_text call takes almost
         * 100 % of the time used in this procedure.
         * Note that even though we only wake up when needed, we 
         * may not have to update all lines, so this check still
         * probably is worth doing. Specially after we added the
         * hibernate update option.
         * */
        if (strcmp(res, line->prev)) {
            gtk_label_set_text(GTK_LABEL(line->label), res);
            g_strlcpy (line->prev, res, sizeof (line->prev));
        }
    }
    oc_tooltip_set(plugin);

    return(TRUE);
}

static gboolean oc_get_time_and_tune(OragePlugin *plugin)
{
    oc_get_time(plugin);
    if (plugin->now.tm_sec > 1) {
        /* we are more than 1 sec off => fix the timing */
        oc_start_timer(plugin);
    }
    else if (plugin->interval > 60000 && plugin->now.tm_min != 0) {
        /* we need to check also minutes if we are using hour timer */
        oc_start_timer(plugin);
    }
    return(TRUE);
}

static gboolean oc_get_time_delay(OragePlugin *plugin)
{
    oc_get_time(plugin); /* update clock */
    /* now we really start the clock */
    plugin->timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
                                             plugin->interval,
                                             (GSourceFunc)oc_get_time_and_tune,
                                             plugin, NULL);
    return(FALSE); /* this is one time only timer */
}

void oc_start_timer(OragePlugin *plugin)
{
    gint delay_time; /* this is used to set the clock start time correct */

#if 0
    g_debug ("oc_start_timer: (%s) interval %d  %d:%d:%d", plugin->tooltip_prev,
             plugin->interval, plugin->now.tm_hour, plugin->now.tm_min,
             plugin->now.tm_sec);
#endif
    /* stop the clock refresh since we will start it again here soon */
    if (plugin->timeout_id) {
        g_source_remove(plugin->timeout_id);
        plugin->timeout_id = 0;
    }
    if (plugin->delay_timeout_id) {
        g_source_remove(plugin->delay_timeout_id);
        plugin->delay_timeout_id = 0;
    }
    oc_get_time(plugin); /* put time on the clock and also fill clock->now */
    /* if we are using longer than 1 second (== 1000) interval, we need
     * to delay the first start so that clock changes when minute or hour
     * changes */
    if (plugin->interval <= 1000) { /* no adjustment needed, we show seconds */
        plugin->timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE
                , plugin->interval, (GSourceFunc)oc_get_time, plugin, NULL);
    }
    else { /* need to tune time */
        if (plugin->interval <= 60000) /* adjust to next full minute */
            delay_time = (plugin->interval - plugin->now.tm_sec*1000);
        else /* adjust to next full hour */
            delay_time = (plugin->interval -
                    (plugin->now.tm_min*60000 + plugin->now.tm_sec*1000));

        plugin->delay_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE
                , delay_time, (GSourceFunc)oc_get_time_delay, plugin, NULL);
    }
}

static gboolean oc_check_if_same(OragePlugin *plugin, int diff)
{
    /* we compare if clock would change after diff seconds */
    /* instead of waiting for the time to really pass, we just move the clock
     * and see what would happen in the future. No need to wait for hours. */
    time_t  t, t_next;
    struct tm tm, tm_next;
    char    res[OC_MAX_LINE_LENGTH-1], res_next[OC_MAX_LINE_LENGTH-1];
    int     max_len;
    ClockLine *line;
    GList   *tmp_list;
    gboolean same_time = TRUE, first_check = TRUE, result_known = FALSE;
    
    max_len = sizeof(res); 
    while (!result_known) {
        time(&t);
        t_next = t + diff;  /* diff secs forward */
        localtime_r(&t, &tm);
        localtime_r(&t_next, &tm_next);
        for (tmp_list = g_list_first(plugin->lines);
                (tmp_list && same_time);
             tmp_list = g_list_next(tmp_list)) {
            line = tmp_list->data;
            oc_utf8_strftime(res, max_len, line->data->str, &tm);
            oc_utf8_strftime(res_next, max_len, line->data->str, &tm_next);
            if (strcmp(res, res_next)) { /* differ */
                same_time = FALSE;
            }
        }
        /* Need to check also tooltip */
        if (same_time) { /* we only check tooltip if needed */
            oc_utf8_strftime(res, max_len, plugin->tooltip_data->str, &tm);
            oc_utf8_strftime(res_next, max_len, plugin->tooltip_data->str
                    , &tm_next);
            if (strcmp(res, res_next)) { /* differ */
                same_time = FALSE;
            }
        }

        if (!same_time) {
            if (first_check) {
                /* change detected, but it can be that bigger unit 
                 * like hour or day happened to change, so we need to check 
                 * again to be sure */
                first_check = FALSE;
                same_time = TRUE;
            }
            else { /* second check, now we are sure the clock has changed */
                result_known = TRUE;   /* no need to check more */
            }
        }
        else { /* clock did not change */
            result_known = TRUE;   /* no need to check more */
        }
    }
    return(same_time);
}

static void oc_tune_interval(OragePlugin *plugin)
{
    /* check if clock changes after 2 secs */
    if (oc_check_if_same(plugin, 2)) { /* Continue checking */
        /* We know now that clock does not change every second. 
         * Let's check 2 minutes next: */
        if (oc_check_if_same(plugin, 2*60)) {
            /* We know now that clock does not change every minute. 
             * We could check hours next, but cpu saving between 1 hour and 24
             * hours would be minimal. But keeping 24 hour wake up time clock
             * in accurate time would be much more difficult, so we end here 
             * and schedule clock to fire every hour. */
            plugin->interval = 3600000;
        }
        else { /* we schedule clock to fire every minute */
            plugin->interval = 60000;
        }
    }
}

void oc_init_timer(OragePlugin *plugin)
{
    /* Fix for bug 7232. Need to make sure timezone is correct. */
    tzset(); 
    plugin->interval = OC_BASE_INTERVAL;
    if (!plugin->hib_timing) /* using suspend/hibernate, do not tune time */
        oc_tune_interval(plugin);
    oc_start_timer(plugin);
}

static void oc_update_size (OragePlugin *plugin, gint size)
{
    if (size > 26) {
        /* FIXME: as plugin is not yet displayed, and width/heigh is not
         * calculated, then following line give negative width warning
         * 'Gtk-WARNING **: 10:00:26.812: gtk_widget_size_allocate(): attempt to allocate widget with width -3 and height 25'
         */
        gtk_container_set_border_width(GTK_CONTAINER(plugin->frame), 2);
    }
    else {
        gtk_container_set_border_width(GTK_CONTAINER(plugin->frame), 0);
    }
}

static gboolean popup_program (G_GNUC_UNUSED GtkWidget *widget, gchar *program,
                               OragePlugin *plugin, guint event_time)
{
    XEvent xevent;
    GdkAtom atom;
    Window xwindow;
    Display *display;
    GError *error = NULL;
    gchar *check, *popup; /* atom names to use */

    if (strcmp(program, "orage") == 0) {
        check = CALENDAR_RUNNING;
        popup = CALENDAR_TOGGLE_EVENT;
    }
    else if (strcmp(program, "globaltime") == 0) {
        check = GLOBALTIME_RUNNING;
        popup = GLOBALTIME_TOGGLE;
    }
    else {
        g_warning("unknown program to start %s", program);
        return(FALSE);
    }

    /* send message to program to check if it is running */
    atom = gdk_atom_intern (check, FALSE);
    display = gdk_x11_get_default_xdisplay ();
    xwindow = XGetSelectionOwner (display, gdk_x11_atom_to_xatom (atom));

    if (xwindow != None)
    {
        /* yes, then toggle */
        xevent.xclient.type = ClientMessage;
        xevent.xclient.format = 8;
        xevent.xclient.display = display;
        xevent.xclient.window = xwindow;
        xevent.xclient.send_event = TRUE;
        xevent.xclient.message_type = XInternAtom (display, popup, FALSE);

        if (!XSendEvent (display, xwindow, FALSE, NoEventMask, &xevent))
            g_warning ("%s: send message to %s failed", OC_NAME, program);

        (void)XFlush (display);
        return TRUE;
    }
    else { /* not running, let's try to start it. Need to reset TZ! */
        static guint prev_event_time = 0; /* prevents double start (BUG 4096) */

        if (prev_event_time && ((event_time - prev_event_time) < 1000)) {
            g_message("%s: double start of %s prevented", OC_NAME, program);
            return(FALSE);
        }

        prev_event_time = event_time;
        if (plugin->TZ_orig != NULL)  /* we had TZ when we started */
            g_setenv("TZ", plugin->TZ_orig, 1);
        else  /* TZ was not set so take it out */
            g_unsetenv("TZ");
        tzset();

        if (!orage_exec(program, FALSE, &error))
            g_message("%s: start of %s failed", OC_NAME, program);

        if ((plugin->timezone->str != NULL) && (plugin->timezone->len > 0)) {
        /* user has set timezone, so let's set TZ */
            g_setenv("TZ", plugin->timezone->str, 1);
            tzset();
        }

        return(TRUE);
    }

    return(FALSE);
}

static gboolean on_button_press_event_cb(GtkWidget *widget
        , GdkEventButton *event, OragePlugin *plugin)
{
    /* Fix for bug 7232. Need to make sure timezone is correct. */
    tzset(); 
    if (event->type != GDK_BUTTON_PRESS) /* double or triple click */
        return(FALSE); /* ignore */
    if (event->button == 1)
        return(popup_program(widget, "orage", plugin, event->time));
    else if (event->button == 2)
        return(popup_program(widget, "globaltime", plugin, event->time));

    return(FALSE);
}


/* -------------------------------------------------------------------- *
 *                     Panel Plugin Interface                           *
 * -------------------------------------------------------------------- */


/* Interface Implementation */

static gboolean oc_set_size (XfcePanelPlugin *plugin, gint size)
{
    OragePlugin *orage_plugin = XFCE_ORAGE_PLUGIN (plugin);

    oc_update_size(orage_plugin, size);
    if (orage_plugin->first_call) {
    /* default is horizontal panel. 
       we need to check and change if it is vertical */
        if (xfce_panel_plugin_get_mode(plugin) 
                == XFCE_PANEL_PLUGIN_MODE_VERTICAL) {
            orage_plugin->lines_vertically = FALSE;
        /* check rotation handling from oc_rotation_changed in oc_config.c */
            if (xfce_screen_position_is_right(
                        xfce_panel_plugin_get_screen_position(plugin)))
                orage_plugin->rotation = 2;
            else
                orage_plugin->rotation = 1;
            oc_reorganize_lines(orage_plugin);
        }

    }

    return(TRUE);
}

static void oc_free_data (XfcePanelPlugin *plugin)
{
    OragePlugin *orage_plugin = XFCE_ORAGE_PLUGIN (plugin);
    GtkWidget *dlg = g_object_get_data(G_OBJECT(plugin), "dialog");

    if (dlg)
        gtk_widget_destroy(dlg);

    if (orage_plugin->timeout_id) {
        g_source_remove(orage_plugin->timeout_id);
    }
    g_list_free(orage_plugin->lines);
    g_free(orage_plugin->TZ_orig);
}

static GdkRGBA oc_rc_read_color (XfceRc *rc, char *par, char *def)
{
    const gchar *ret;
    GdkRGBA color;

    ret = xfce_rc_read_entry(rc, par, def);
    if (orgage_str_to_rgba (ret, &color, def) == FALSE)
    {
        g_warning ("unable to read %s colour from rc file ret=(%s) def=(%s)",
                   par, ret, def);
    }
    return(color);
}

ClockLine * oc_add_new_line (OragePlugin *plugin, const char *data,
                             const char *font, int pos)
{
    ClockLine *clock_line = g_new0(ClockLine, 1);

    clock_line->data = g_string_new(data);
    clock_line->font = g_string_new(font);
    g_strlcpy (clock_line->prev, "New line", sizeof (clock_line->prev));
    clock_line->clock = plugin;
    plugin->lines = g_list_insert(plugin->lines, clock_line, pos);
    return(clock_line);
}

static void oc_read_rc_file(XfcePanelPlugin *plugin, OragePlugin *orage_plugin)
{
    gchar  *file;
    XfceRc *rc;
    const gchar  *ret, *data, *font;
    gchar tmp[100];
    gboolean more_lines;
    gint i;

    if (!(file = xfce_panel_plugin_lookup_rc_file(plugin)))
        return; /* if it does not exist, we use defaults from orage_oc_new */
    if (!(rc = xfce_rc_simple_open(file, TRUE))) {
        g_warning("unable to read-open rc file (%s)", file);
        return;
    }
    orage_plugin->first_call = FALSE;

    orage_plugin->show_frame = xfce_rc_read_bool_entry(rc, "show_frame", TRUE);

    orage_plugin->fg_set = xfce_rc_read_bool_entry(rc, "fg_set", FALSE);
    if (orage_plugin->fg_set) {
        orage_plugin->fg = oc_rc_read_color(rc, "fg", "black");
    }

    orage_plugin->bg_set = xfce_rc_read_bool_entry(rc, "bg_set", FALSE);
    if (orage_plugin->bg_set) {
        orage_plugin->bg = oc_rc_read_color(rc, "bg", "white");
    }
    g_free(file);

    ret = xfce_rc_read_entry(rc, "timezone", NULL);
    g_string_assign(orage_plugin->timezone, ret);

    orage_plugin->width_set = xfce_rc_read_bool_entry(rc, "width_set", FALSE);
    if (orage_plugin->width_set) {
        orage_plugin->width = xfce_rc_read_int_entry(rc, "width", -1);
    }
    orage_plugin->height_set = xfce_rc_read_bool_entry(rc, "height_set", FALSE);
    if (orage_plugin->height_set) {
        orage_plugin->height = xfce_rc_read_int_entry(rc, "height", -1);
    }

    orage_plugin->lines_vertically = xfce_rc_read_bool_entry(rc, "lines_vertically", FALSE);
    orage_plugin->rotation = xfce_rc_read_int_entry(rc, "rotation", 0);
    
    for (i = 0, more_lines = TRUE; more_lines; i++) {
        g_snprintf (tmp, sizeof (tmp), "data%d", i);
        data = xfce_rc_read_entry(rc, tmp, NULL);
        if (data) { /* let's add it */
            g_snprintf (tmp, sizeof (tmp), "font%d", i);
            font = xfce_rc_read_entry(rc, tmp, NULL);
            oc_add_new_line(orage_plugin, data, font, -1);
        }
        else { /* no more clock lines */
            more_lines = FALSE;
        }
    }
    orage_plugin->orig_line_cnt = i;

    if ((ret = xfce_rc_read_entry(rc, "tooltip", NULL)))
        g_string_assign(orage_plugin->tooltip_data, ret);

    orage_plugin->hib_timing = xfce_rc_read_bool_entry(rc, "hib_timing", FALSE);

    xfce_rc_close(rc);
}

void oc_write_rc_file (XfcePanelPlugin *plugin)
{
    gchar  *file;
    gchar  *colour_str;
    XfceRc *rc;
    gchar   tmp[100];
    int     i;
    ClockLine *line;
    GList   *tmp_list;
    OragePlugin *orage_plugin = XFCE_ORAGE_PLUGIN (plugin);

    if (!(file = xfce_panel_plugin_save_location(plugin, TRUE))) {
        g_warning("unable to write rc file");
        return;
    }
    if (!(rc = xfce_rc_simple_open(file, FALSE))) {
        g_warning("unable to read-open rc file (%s)", file);
        return;
    }
    g_free(file);

    xfce_rc_write_bool_entry(rc, "show_frame", orage_plugin->show_frame);

    xfce_rc_write_bool_entry(rc, "fg_set", orage_plugin->fg_set);
    if (orage_plugin->fg_set) {
        colour_str = gdk_rgba_to_string (&orage_plugin->fg);
        xfce_rc_write_entry (rc, "fg", colour_str);
        g_free (colour_str);
    }
    else {
        xfce_rc_delete_entry(rc, "fg", TRUE);
    }

    xfce_rc_write_bool_entry(rc, "bg_set", orage_plugin->bg_set);
    if (orage_plugin->bg_set) {
        colour_str = gdk_rgba_to_string (&orage_plugin->bg);
        xfce_rc_write_entry (rc, "bg", colour_str);
        g_free (colour_str);
    }
    else {
        xfce_rc_delete_entry(rc, "bg", TRUE);
    }

    xfce_rc_write_entry(rc, "timezone",  orage_plugin->timezone->str);

    xfce_rc_write_bool_entry(rc, "width_set", orage_plugin->width_set);
    if (orage_plugin->width_set) {
        xfce_rc_write_int_entry(rc, "width", orage_plugin->width);
    }
    else {
        xfce_rc_delete_entry(rc, "width", TRUE);
    }

    xfce_rc_write_bool_entry(rc, "height_set", orage_plugin->height_set);
    if (orage_plugin->height_set) {
        xfce_rc_write_int_entry(rc, "height", orage_plugin->height);
    }
    else {
        xfce_rc_delete_entry(rc, "height", TRUE);
    }

    xfce_rc_write_bool_entry(rc, "lines_vertically", orage_plugin->lines_vertically);
    xfce_rc_write_int_entry(rc, "rotation", orage_plugin->rotation);

    for (i = 0, tmp_list = g_list_first(orage_plugin->lines);
            tmp_list;
         i++, tmp_list = g_list_next(tmp_list)) {
        line = tmp_list->data;
        g_snprintf (tmp, sizeof (tmp), "data%d", i);
        xfce_rc_write_entry(rc, tmp,  line->data->str);
        g_snprintf (tmp, sizeof (tmp), "font%d", i);
        xfce_rc_write_entry(rc, tmp,  line->font->str);
    }
    /* delete extra lines */
    for (; i <= orage_plugin->orig_line_cnt; i++) {
        g_snprintf (tmp, sizeof (tmp), "data%d", i);
        xfce_rc_delete_entry(rc, tmp,  FALSE);
        g_snprintf (tmp, sizeof (tmp), "font%d", i);
        xfce_rc_delete_entry(rc, tmp,  FALSE);
    }

    xfce_rc_write_entry(rc, "tooltip",  orage_plugin->tooltip_data->str);

    xfce_rc_write_bool_entry(rc, "hib_timing", orage_plugin->hib_timing);

    xfce_rc_close(rc);
}

/* Create widgets and connect to signals */
static OragePlugin *orage_oc_new(XfcePanelPlugin *plugin)
{
    OragePlugin *orage_plugin = XFCE_ORAGE_PLUGIN (plugin);

    orage_plugin->first_call = TRUE; /* this is starting point */

    orage_plugin->ebox = gtk_event_box_new();
    gtk_widget_show(orage_plugin->ebox);

    orage_plugin->frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(orage_plugin->ebox), orage_plugin->frame);
    gtk_widget_show(orage_plugin->frame);

    gtk_container_add(GTK_CONTAINER(plugin), orage_plugin->ebox);

    orage_plugin->show_frame = TRUE;
    orage_plugin->fg_set = FALSE;
    orage_plugin->bg_set = FALSE;
    orage_plugin->width_set = FALSE;
    orage_plugin->height_set = FALSE;
    orage_plugin->lines_vertically = TRUE;
    orage_plugin->rotation = 0; /* no rotation */

    orage_plugin->timezone = g_string_new(""); /* = not set */
    orage_plugin->TZ_orig = g_strdup(g_getenv("TZ"));

    orage_plugin->lines = NULL; /* no lines yet */
    orage_plugin->orig_line_cnt = 0;

    /* TRANSLATORS: Use format characters from strftime(3)
     * to get the proper string for your locale.
     * I used these:
     * %A  : full weekday name
     * %d  : day of the month
     * %B  : full month name
     * %Y  : four digit year
     * %V  : ISO week number
     */
    orage_plugin->tooltip_data = g_string_new(_("%A %d %B %Y/%V"));

    orage_plugin->hib_timing = FALSE;

    return(orage_plugin);
}

void oc_show_frame_set(OragePlugin *plugin)
{
    gtk_frame_set_shadow_type(GTK_FRAME(plugin->frame)
            , plugin->show_frame ? GTK_SHADOW_IN : GTK_SHADOW_NONE);
}

void oc_fg_set(OragePlugin *plugin)
{
    GdkRGBA *fg = NULL;
    ClockLine *line;
    GList   *tmp_list;

    if (plugin->fg_set)
        fg = &plugin->fg;

    for (tmp_list = g_list_first(plugin->lines);
         tmp_list;
         tmp_list = g_list_next(tmp_list))
    {
        line = tmp_list->data;
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_widget_override_color (line->label, GTK_STATE_FLAG_NORMAL, fg);
        G_GNUC_END_IGNORE_DEPRECATIONS
    }
}

void oc_bg_set(OragePlugin *plugin)
{
    GdkRGBA *bg = NULL;

    if (plugin->bg_set)
        bg = &plugin->bg;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_widget_override_background_color (plugin->ebox,
                                          GTK_STATE_FLAG_NORMAL, bg);
    G_GNUC_END_IGNORE_DEPRECATIONS
}

void oc_timezone_set(OragePlugin *plugin)
{
    if ((plugin->timezone->str != NULL) && (plugin->timezone->len > 0)) {
        /* user has set timezone, so let's set TZ */
        g_setenv("TZ", plugin->timezone->str, 1);
    }
    else if (plugin->TZ_orig != NULL) { /* we had TZ when we started */
        g_setenv("TZ", plugin->TZ_orig, 1);
    }
    else { /* TZ was not set so take it out */
        g_unsetenv("TZ");
    }
    tzset();
}

void oc_size_set(OragePlugin *plugin)
{
    gint w, h;

    w = plugin->width_set ? plugin->width : -1;
    h = plugin->height_set ? plugin->height : -1;
    gtk_widget_set_size_request(plugin->mbox, w, h);
}

void oc_construct(XfcePanelPlugin *plugin)
{
    OragePlugin *clock;

    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    clock = orage_oc_new(plugin);

    oc_read_rc_file(plugin, clock);
    if (clock->lines == NULL) { /* Let's setup a default clock_line */
        oc_add_new_line(clock, "%X", "", -1);
    }

    oc_set_lines_to_panel(clock);
    oc_show_frame_set(clock);
    oc_fg_set(clock);
    oc_bg_set(clock);
    oc_timezone_set(clock);
    oc_size_set(clock);

    oc_init_timer(clock);

    /* we are called through size-changed trigger */
#if 0
    oc_update_size(clock, xfce_panel_plugin_get_size(plugin));
#endif

    xfce_panel_plugin_add_action_widget(plugin, clock->ebox);

    xfce_panel_plugin_menu_show_configure(plugin);

    /* callback for calendar and globaltime popup */
    g_signal_connect(clock->ebox, "button-press-event",
            G_CALLBACK(on_button_press_event_cb), clock);
}

static void orage_plugin_class_init (OragePluginClass *klass)
{
    XfcePanelPluginClass *plugin_class;

    plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
    plugin_class->construct = oc_construct;
    plugin_class->free_data = oc_free_data;
    plugin_class->size_changed = oc_set_size;
    plugin_class->configure_plugin = oc_properties_dialog;
    plugin_class->save = oc_write_rc_file;
}

static void orage_plugin_init (G_GNUC_UNUSED OragePlugin *plugin)
{
}

XFCE_PANEL_DEFINE_PLUGIN (OragePlugin, orage_plugin)
