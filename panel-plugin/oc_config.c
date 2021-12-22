/* vim: set expandtab ts=4 sw=4: */
/*
 *
 *  Copyright © 2006-2011 Juha Kautto <juha@xfce.org>
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
 *      Based on Xfce panel plugin clock and date-time plugin
 */

#include <config.h>
#include <sys/stat.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gdk/gdkevents.h>

#include <libxfce4panel/libxfce4panel.h>

#include "../src/orage-i18n.h"
#include "../src/functions.h"
#include "xfce4-orageclock-plugin.h"
#include "../src/tz_zoneinfo_read.h"
#include "timezone_selection.h"


void oc_properties_options(GtkWidget *dlg, OragePlugin *plugin);

/* -------------------------------------------------------------------- *
 *                        Configuration Dialog                          *
 * -------------------------------------------------------------------- */

static void oc_show_frame_toggled(GtkToggleButton *cb, OragePlugin *plugin)
{
    /* FIXME: following Gtk-WARNING will be printed when frame is enabled:
     * "Negative content width -1 (allocation 1, extents 1x1) while allocating gadget (node border, owner GtkFrame)"
     */
    plugin->show_frame = gtk_toggle_button_get_active(cb);
    oc_show_frame_set(plugin);
}

static void oc_set_fg_toggled(GtkToggleButton *cb, OragePlugin *plugin)
{
    plugin->fg_set = gtk_toggle_button_get_active(cb);
    oc_fg_set(plugin);
}

static void oc_fg_color_changed (GtkColorButton *color_button,
                                 OragePlugin *plugin)
{
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (color_button), &plugin->fg);
    oc_fg_set(plugin);
}

static void oc_set_bg_toggled(GtkToggleButton *cb, OragePlugin *plugin)
{
    plugin->bg_set = gtk_toggle_button_get_active(cb);
    oc_bg_set(plugin);
}

static void oc_bg_color_changed (GtkColorButton *color_button,
                                 OragePlugin *plugin)
{
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (color_button), &plugin->bg);
    oc_bg_set(plugin);
}

static void oc_set_height_toggled(GtkToggleButton *cb, OragePlugin *plugin)
{
    plugin->height_set = gtk_toggle_button_get_active(cb);
    oc_size_set(plugin);
}

static void oc_set_height_changed(GtkSpinButton *sb, OragePlugin *plugin)
{
    plugin->height = gtk_spin_button_get_value_as_int(sb);
    oc_size_set(plugin);
}

static void oc_set_width_toggled(GtkToggleButton *cb, OragePlugin *plugin)
{
    plugin->width_set = gtk_toggle_button_get_active(cb);
    oc_size_set(plugin);
}

static void oc_set_width_changed(GtkSpinButton *sb, OragePlugin *plugin)
{
    plugin->width = gtk_spin_button_get_value_as_int(sb);
    oc_size_set(plugin);
}

static void oc_rotation_changed(GtkComboBox *cb, OragePlugin *plugin)
{
    GList *tmp_list;

    plugin->rotation = gtk_combo_box_get_active(cb);
    for (tmp_list = g_list_first(plugin->lines);
            tmp_list;
         tmp_list = g_list_next(tmp_list)) {
        oc_line_rotate(plugin, tmp_list->data);
    }
}

static void oc_lines_vertically_toggled (GtkToggleButton *cb,
                                         OragePlugin *plugin)
{
    plugin->lines_vertically = gtk_toggle_button_get_active(cb);

    oc_reorganize_lines(plugin);
}

static void oc_timezone_selected(GtkButton *button, OragePlugin *plugin)
{
    GtkWidget *dialog;
    gchar *filename = NULL;

    dialog = g_object_get_data (G_OBJECT (plugin), "dialog");
    if (orage_timezone_button_clicked(button, GTK_WINDOW(dialog)
            , &filename, FALSE, NULL)) {
        g_string_assign(plugin->timezone, filename);
        oc_timezone_set(plugin);
        g_free(filename);
    }
}

static void oc_hib_timing_toggled(GtkToggleButton *cb, OragePlugin *plugin)
{
    plugin->hib_timing = gtk_toggle_button_get_active(cb);
}

static gboolean oc_line_changed(GtkWidget *entry, GdkEventKey *key
        , GString *data)
{
    g_string_assign(data, gtk_entry_get_text(GTK_ENTRY(entry)));
    
    (void)key;

    return(FALSE);
}

static void oc_line_font_changed(GtkWidget *widget, ClockLine *line)
{
    gchar *font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (widget));
    g_string_assign (line->font, font);
    g_free (font);
    oc_line_font_set (line);
}

static void oc_recreate_properties_options(OragePlugin *plugin)
{
    GtkWidget *dialog, *frame;

    dialog = g_object_get_data (G_OBJECT (plugin), "dialog");
    frame  = g_object_get_data (G_OBJECT (plugin), "properties_frame");
    gtk_widget_destroy(frame);
    oc_properties_options(dialog, plugin);
    gtk_widget_show_all(dialog);
}

static void oc_new_line(GtkToolButton *toolbutton, ClockLine *line)
{
    OragePlugin *plugin = line->clock;
    ClockLine *new_line;
    gint pos;
    pos = g_list_index(plugin->lines, line);
    new_line = oc_add_new_line(plugin, "%X", "", pos+1);
    oc_set_line(plugin, new_line, pos+1);
    oc_fg_set(plugin);

    oc_recreate_properties_options(plugin);
    
    (void)toolbutton;
}

static void oc_delete_line(GtkToolButton *toolbutton, ClockLine *line)
{
    OragePlugin *plugin = line->clock;

    /* remove the data from the list and from the panel */
    g_string_free(line->data, TRUE);
    g_string_free(line->font, TRUE);
    gtk_widget_destroy(line->label);
    plugin->lines = g_list_remove(plugin->lines, line);
    g_free(line);

    oc_recreate_properties_options(plugin);
    
    (void)toolbutton;
}

static void oc_move_up_line(GtkToolButton *toolbutton, ClockLine *line)
{
    OragePlugin *plugin = line->clock;
    gint pos;

    pos = g_list_index(plugin->lines, line);
    pos--;
    gtk_box_reorder_child(GTK_BOX(plugin->mbox), line->label, pos);
    plugin->lines = g_list_remove(plugin->lines, line);
    plugin->lines = g_list_insert(plugin->lines, line, pos);

    oc_recreate_properties_options(plugin);
    
    (void)toolbutton;
}

static void oc_move_down_line(GtkToolButton *toolbutton, ClockLine *line)
{
    OragePlugin *plugin = line->clock;
    gint pos, line_cnt;

    line_cnt = g_list_length(plugin->lines);
    pos = g_list_index(plugin->lines, line);
    pos++;
    if (pos == line_cnt)
        pos = 0;
    gtk_box_reorder_child(GTK_BOX(plugin->mbox), line->label, pos);
    plugin->lines = g_list_remove(plugin->lines, line);
    plugin->lines = g_list_insert(plugin->lines, line, pos);

    oc_recreate_properties_options(plugin);
    
    (void)toolbutton;
}

static void oc_table_add (GtkWidget *table, GtkWidget *widget,
                          const int col, const int row)
{
    g_object_set (widget, "expand", TRUE, NULL);
    gtk_grid_attach (GTK_GRID (table), widget, col, row, 1, 1);
}

static void oc_properties_appearance(GtkWidget *dlg, OragePlugin *plugin)
{
    GtkWidget *frame, *cb, *color, *sb, *vbox;
    GdkRGBA def_fg, def_bg;
    GtkWidget *table;
    const gchar *clock_rotation_array[3] =
    { _("No rotation"), _("Rotate left") , _("Rotate right")};

    table = gtk_grid_new ();
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    g_object_set (table, "row-spacing", 6, "column-spacing", 6,  NULL);
    
    frame = orage_create_framebox_with_content(_("Appearance"), GTK_SHADOW_NONE,
                                               table);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 6);
    vbox = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    /* show frame */
    cb = gtk_check_button_new_with_mnemonic(_("Show _frame"));
    oc_table_add(table, cb, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), plugin->show_frame);
    g_signal_connect(cb, "toggled", G_CALLBACK(oc_show_frame_toggled), plugin);

    /* foreground color */
    cb = gtk_check_button_new_with_mnemonic(_("set foreground _color:"));
    oc_table_add(table, cb, 0, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), plugin->fg_set);
    g_signal_connect(cb, "toggled", G_CALLBACK(oc_set_fg_toggled), plugin);

    if (!plugin->fg_set)
    {
        gdk_rgba_parse (&def_fg, "black");
        plugin->fg = def_fg;
    }
    color = gtk_color_button_new_with_rgba (&plugin->fg);
    oc_table_add(table, color, 1, 1);
    g_signal_connect(G_OBJECT(color), "color-set"
            , G_CALLBACK(oc_fg_color_changed), plugin);

    /* background color */
    cb = gtk_check_button_new_with_mnemonic(_("set _background color:"));
    oc_table_add(table, cb, 2, 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), plugin->bg_set);
    g_signal_connect(cb, "toggled", G_CALLBACK(oc_set_bg_toggled), plugin);

    if (!plugin->bg_set)
    {
        gdk_rgba_parse (&def_bg, "white");
        plugin->bg = def_bg;
    }
    color = gtk_color_button_new_with_rgba (&plugin->bg);
    oc_table_add(table, color, 3, 1);
    g_signal_connect(G_OBJECT(color), "color-set"
            , G_CALLBACK(oc_bg_color_changed), plugin);

    /* clock size (=mbox size): height and width */
    cb = gtk_check_button_new_with_mnemonic(_("set _height:"));
    oc_table_add(table, cb, 0, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), plugin->height_set);
    g_signal_connect(cb, "toggled", G_CALLBACK(oc_set_height_toggled), plugin);
    sb = gtk_spin_button_new_with_range(10, 200, 1);
    oc_table_add(table, sb, 1, 2);
    if (!plugin->height_set)
        plugin->height = 32;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb), (gdouble)plugin->height);
    gtk_widget_set_tooltip_text (GTK_WIDGET(cb),
            _("Note that you can not change the height of horizontal panels"));
    g_signal_connect((gpointer) sb, "value-changed",
            G_CALLBACK(oc_set_height_changed), plugin);

    cb = gtk_check_button_new_with_mnemonic(_("set _width:"));
    oc_table_add(table, cb, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), plugin->width_set);
    g_signal_connect(cb, "toggled", G_CALLBACK(oc_set_width_toggled), plugin);
    sb = gtk_spin_button_new_with_range(10, 400, 1);
    oc_table_add(table, sb, 3, 2);
    if (!plugin->width_set)
        plugin->width = 70;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb), (gdouble)plugin->width);
    gtk_widget_set_tooltip_text (GTK_WIDGET(cb),
            _("Note that you can not change the width of vertical panels"));
    g_signal_connect((gpointer) sb, "value-changed",
            G_CALLBACK(oc_set_width_changed), plugin);

    /* rotation and line (=box) position (top / left) */
    cb = orage_create_combo_box_with_content(clock_rotation_array, 3);
    oc_table_add(table, cb, 0, 3);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cb), plugin->rotation);
    g_signal_connect(cb, "changed", G_CALLBACK(oc_rotation_changed), plugin);

    cb = gtk_check_button_new_with_mnemonic(_("Show lines _vertically"));
    oc_table_add(table, cb, 2, 3);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), plugin->lines_vertically);
    g_signal_connect(cb, "toggled", G_CALLBACK(oc_lines_vertically_toggled), plugin);
}

void oc_properties_options(GtkWidget *dlg, OragePlugin *plugin)
{
    GtkWidget *frame, *cb, *label, *entry, *font, *button;
    gchar tmp[100];
    GtkWidget *table, *toolbar, *vbox;
    GtkWidget *tool_button;
    gint line_cnt, cur_line;
    ClockLine *line;
    GList   *tmp_list;

    line_cnt = g_list_length(plugin->lines);
    table = gtk_grid_new ();
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    g_object_set (table, "row-spacing", 6, "column-spacing", 6,  NULL);

    frame = orage_create_framebox_with_content(_("Clock Options"),
                                               GTK_SHADOW_NONE, table);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 6);
    vbox = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    /* we sometimes call this function again, so we need to put it to the 
       correct position */
    gtk_box_reorder_child(GTK_BOX(vbox), frame, 2);
    /* this is needed when we restructure this frame */
    g_object_set_data (G_OBJECT (plugin), "properties_frame", frame);

    /* timezone */
    label = gtk_label_new(_("set timezone to:"));
    oc_table_add(table, label, 0, 0);

    button = orage_util_image_button ("document-open", _("_Open"));
    if (plugin->timezone->str && plugin->timezone->len)
         gtk_button_set_label(GTK_BUTTON(button), _(plugin->timezone->str));
    oc_table_add(table, button, 1, 0);
    g_signal_connect(G_OBJECT(button), "clicked"
            , G_CALLBACK(oc_timezone_selected), plugin);

    /* lines */
    line_cnt = g_list_length(plugin->lines);

    cur_line = 0;
    for (tmp_list = g_list_first(plugin->lines);
            tmp_list;
         tmp_list = g_list_next(tmp_list)) {
        cur_line++;
        line = tmp_list->data;
        g_snprintf (tmp, sizeof (tmp), _("Line %d:"), cur_line);
        label = gtk_label_new(tmp);
        oc_table_add(table, label, 0, cur_line);

        entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), line->data->str); 
        g_signal_connect(entry, "key-release-event", G_CALLBACK(oc_line_changed)
                , line->data);
        if (cur_line == 1)
            gtk_widget_set_tooltip_text (GTK_WIDGET(entry),
                    _("Enter any valid strftime function parameter."));
        oc_table_add(table, entry, 1, cur_line);

        if (line->font->len)
            font = gtk_font_button_new_with_font (line->font->str);
        else
            font = gtk_font_button_new ();
        
        g_signal_connect(G_OBJECT(font), "font-set"
                , G_CALLBACK(oc_line_font_changed), line);
        oc_table_add(table, font, 2, cur_line);

        toolbar = gtk_toolbar_new();
        if (line_cnt < OC_MAX_LINES) { /* no real reason to limit this though */
            tool_button = orage_toolbar_append_button (toolbar, "list-add",
                                                       "Add line", -1);
            g_signal_connect(tool_button, "clicked"
                    , G_CALLBACK(oc_new_line), line);
        }
        if (line_cnt > 1) { /* do not delete last line */
            tool_button = orage_toolbar_append_button (toolbar, "list-remove",
                                                       "Remove line", -1);
            g_signal_connect(tool_button, "clicked"
                    , G_CALLBACK(oc_delete_line), line);
            /* we do not need these if we only have 1 line */
            tool_button = orage_toolbar_append_button (toolbar, "go-up",
                                                       "Move up", -1);
            g_signal_connect(tool_button, "clicked"
                    , G_CALLBACK(oc_move_up_line), line);
            tool_button = orage_toolbar_append_button (toolbar, "go-down",
                                                       "Move down", -1);
            g_signal_connect(tool_button, "clicked"
                    , G_CALLBACK(oc_move_down_line), line);
        }
        oc_table_add(table, toolbar, 3, cur_line);
    }

    /* Tooltip hint */
    label = gtk_label_new(_("Tooltip:"));
    oc_table_add(table, label, 0, line_cnt+1);

    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), plugin->tooltip_data->str); 
    oc_table_add(table, entry, 1, line_cnt+1);
    g_signal_connect(entry, "key-release-event", G_CALLBACK(oc_line_changed)
            , plugin->tooltip_data);
    
    /* special timing for SUSPEND/HIBERNATE */
    cb = gtk_check_button_new_with_mnemonic(_("fix time after suspend/hibernate"));
    oc_table_add(table, cb, 1, line_cnt+2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), plugin->hib_timing);
    gtk_widget_set_tooltip_text (GTK_WIDGET(cb),
            _("You only need this if you do short term (less than 5 hours) suspend or hibernate and your visible time does not include seconds. Under these circumstances it is possible that Orageclock shows time inaccurately unless you have this selected. (Selecting this prevents cpu and interrupt saving features from working.)"));
    g_signal_connect(cb, "toggled", G_CALLBACK(oc_hib_timing_toggled), plugin);
}

void oc_instructions(GtkWidget *dlg, OragePlugin *plugin)
{
    GtkWidget *hbox, *vbox, *label, *image;

    /* Instructions */
    hbox = gtk_grid_new ();
    vbox = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

    image = gtk_image_new_from_icon_name ("dialog-information",
                                          GTK_ICON_SIZE_DND);
    g_object_set (image, "xalign", 0.5f, "yalign", 0.0f, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), image, NULL, GTK_POS_RIGHT, 1, 1);

    label = gtk_label_new(_("This program uses strftime function to get time.\nUse any valid code to get time in the format you prefer.\nSome common codes are:\n\t%A = weekday\t\t\t%B = month\n\t%c = date & time\t\t%R = hour & minute\n\t%V = week number\t\t%Z = timezone in use\n\t%H = hours \t\t\t\t%M = minute\n\t%X = local time\t\t\t%x = local date"));
    g_object_set (label, "xalign", 0.0, "yalign", 0.0,
                         "hexpand", TRUE, "halign", GTK_ALIGN_FILL, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    
    (void)plugin;
}

static void oc_dialog_response (GtkWidget *dlg, int response,
                                OragePlugin *plugin)
{
    XfcePanelPlugin *panel_plugin = XFCE_PANEL_PLUGIN (plugin);
    
    g_object_set_data (G_OBJECT (plugin), "dialog", NULL);
    g_object_set_data (G_OBJECT (plugin), "properties_frame", NULL);
    gtk_widget_destroy(dlg);
    xfce_panel_plugin_unblock_menu (panel_plugin);
    oc_write_rc_file (panel_plugin);
    oc_init_timer (plugin);
    
    (void)response;
}

void oc_properties_dialog (XfcePanelPlugin *plugin)
{
    OragePlugin *orage_plugin = XFCE_ORAGE_PLUGIN (plugin);
    GtkWidget *dlg;

    xfce_panel_plugin_block_menu(plugin);
    
    /* change interval to show quick feedback on panel */
    orage_plugin->interval = OC_CONFIG_INTERVAL; 
    oc_start_timer(orage_plugin);

    dlg = gtk_dialog_new_with_buttons(_("Orage clock Preferences"), 
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))),
            GTK_DIALOG_DESTROY_WITH_PARENT |
            GTK_UI_MANAGER_SEPARATOR,
            _("Close"), GTK_RESPONSE_OK,
            NULL);
    
    g_object_set_data(G_OBJECT(plugin), "dialog", dlg);
    gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(dlg), 2);
    g_signal_connect(dlg, "response", G_CALLBACK(oc_dialog_response),
                     orage_plugin);
    
    oc_properties_appearance(dlg, orage_plugin);
    oc_properties_options(dlg, orage_plugin);
    oc_instructions(dlg, orage_plugin);

    gtk_widget_show_all(dlg);
}
