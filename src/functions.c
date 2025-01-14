/*
 * Copyright (c) 2021-2025 Erkki Moorits
 * Copyright (c) 2005-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2003-2005 Mickael Graf (korbinus at xfce.org)
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

#define _XOPEN_SOURCE /* glibc2 needs this */
#define _XOPEN_SOURCE_EXTENDED 1 /* strptime needs this in posix systems */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <gdk/gdk.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "functions.h"
#include "orage-i18n.h"
#include "parameters.h"
#include "tz_zoneinfo_read.h"

#ifdef HAVE_LIBXFCE4UI
#include <libxfce4ui/libxfce4ui.h>
#endif

/**************************************
 *  General purpose helper functions  *
 **************************************/

gint orage_gdatetime_compare_date (GDateTime *gdt1, GDateTime *gdt2)
{
    gint y1;
    gint y2;
    gint m1;
    gint m2;
    gint d1;
    gint d2;

    g_date_time_get_ymd (gdt1, &y1, &m1, &d1);
    g_date_time_get_ymd (gdt2, &y2, &m2, &d2);

    if (d1 < d2)
        return -1;
    else if (d1 > d2)
        return 1;
    else if (m1 < m2)
        return -1;
    else if (m1 > m2)
        return 1;
    else if (y1 < y2)
        return -1;
    else if (y1 > y2)
        return 1;
    else
        return 0;
}

GtkWidget *orage_create_combo_box_with_content (const gchar *text[],
                                                const int size)
{
    int i;
    GtkWidget *combo_box;

    combo_box = gtk_combo_box_text_new ();
    for (i = 0; i < size; i++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), text[i]);

    return combo_box;
}

gboolean orage_date_button_clicked (GtkWidget *button, GtkWidget *selDate_dialog)
{
#if 0
    GtkWidget *selDate_dialog;
#endif
    GtkWidget *selDate_calendar;
    gint result;
    gboolean changed;
    gchar *time_str;
    GDateTime *gdt;
    GDateTime *gdt_new_date;

#if 0
    /* For some unknown reason NLS does not work in this file, so this has to be
     * done in the main code:
     */
    selDate_dialog = gtk_dialog_new_with_buttons(
            _("Pick the date"), GTK_WINDOW(win),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("Today"),
            1,
            "_OK",
            GTK_RESPONSE_ACCEPT,
            NULL);
#endif

    selDate_calendar = gtk_calendar_new();
    gtk_container_add(
        GTK_CONTAINER(gtk_dialog_get_content_area (GTK_DIALOG(selDate_dialog)))
            , selDate_calendar);

    gdt = g_object_get_data (G_OBJECT (button), DATE_KEY);

    if (gdt == NULL)
    {
        /* something was wrong. let's return some valid value */
        gdt = g_date_time_new_now_local ();
    }
    else
        g_date_time_ref (gdt);

    orage_select_date (GTK_CALENDAR (selDate_calendar), gdt);
    gtk_widget_show_all(selDate_dialog);

    result = gtk_dialog_run(GTK_DIALOG(selDate_dialog));
    switch(result){
        case GTK_RESPONSE_ACCEPT:
            gdt_new_date = orage_cal_to_gdatetime (GTK_CALENDAR (selDate_calendar), 1, 1);
            break;

        case 1:
            gdt_new_date = g_date_time_new_now_local ();
            break;

        case GTK_RESPONSE_DELETE_EVENT:
        default:
            gdt_new_date = g_date_time_ref (gdt);
            break;
    }

    changed = orage_gdatetime_compare_date (gdt_new_date, gdt) ? TRUE : FALSE;
    g_date_time_unref (gdt);
    time_str = orage_gdatetime_to_i18_time (gdt_new_date, TRUE);
    gtk_button_set_label (GTK_BUTTON(button), time_str);
    g_free (time_str);
    g_object_set_data_full (G_OBJECT (button),
                            DATE_KEY, gdt_new_date,
                            (GDestroyNotify)g_date_time_unref);

    gtk_widget_destroy(selDate_dialog);
    return(changed);
}

static void child_setup_async (G_GNUC_UNUSED gpointer user_data)
{
#if defined(HAVE_SETSID) && !defined(G_OS_WIN32)
    setsid();
#endif
}

static void child_watch_cb (GPid pid, G_GNUC_UNUSED gint status, gpointer data)
{
    gboolean *cmd_active = (gboolean *)data;

    waitpid(pid, NULL, 0);
    g_spawn_close_pid(pid);
    *cmd_active = FALSE;
}

gboolean orage_exec(const gchar *cmd, gboolean *cmd_active, GError **error)
{
    gchar **argv;
    gboolean success;
    int spawn_flags = G_SPAWN_SEARCH_PATH;
    GPid pid;

    g_return_val_if_fail (cmd != NULL, FALSE);

    if (G_UNLIKELY (!g_shell_parse_argv(cmd, NULL, &argv, error)))
        return(FALSE);

    if (cmd_active) {
        spawn_flags |= G_SPAWN_DO_NOT_REAP_CHILD;
        success = g_spawn_async(NULL, argv, NULL, spawn_flags
                , child_setup_async, NULL, &pid, error);
        *cmd_active = success;
        if (success)
            g_child_watch_add(pid, child_watch_cb, cmd_active);
    }
    else
        success = g_spawn_async(NULL, argv, NULL, spawn_flags
                , child_setup_async, NULL, NULL, error);

    g_strfreev(argv);

    return(success);
}

GtkWidget *orage_toolbar_append_button (GtkWidget *toolbar,
                                        const gchar *icon_name,
                                        const gchar *tooltip_text,
                                        const gint pos)
{
    GtkWidget *button;
    GtkWidget *image;

    image = gtk_image_new_from_icon_name (icon_name,
                                          GTK_ICON_SIZE_SMALL_TOOLBAR);

    button = (GtkWidget *)gtk_tool_button_new (image, tooltip_text);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(button), tooltip_text);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(button), pos);
    return button;
}

GtkWidget *orage_toolbar_append_separator(GtkWidget *toolbar, gint pos)
{
    GtkWidget *separator;

    separator = (GtkWidget *)gtk_separator_tool_item_new();
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(separator), pos);

    return separator;
}

GtkWidget *orage_table_new (const guint border)
{
    GtkWidget *grid;

    grid = gtk_grid_new ();
    gtk_container_set_border_width (GTK_CONTAINER (grid), border);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

    return grid;
}

void orage_table_add_row (GtkWidget *table, GtkWidget *label, GtkWidget *input,
                          const guint row,
                          const OrageTableAttachOptions input_x_option,
                          const OrageTableAttachOptions input_y_option)
{
    if (label) {
        gtk_grid_attach (GTK_GRID (table), label, 0, row, 1, 1);
        g_object_set (label, "xalign", 0.0, "yalign", 0.5,
                             "halign", GTK_ALIGN_FILL,
                             NULL);
    }

    if (input) {
        gtk_grid_attach (GTK_GRID (table), input, 1, row, 1, 1);

        if (input_x_option & OTBL_FILL)
            g_object_set (input, "halign", GTK_ALIGN_FILL, NULL);
        if (input_x_option & OTBL_EXPAND)
            g_object_set (input, "hexpand", TRUE, NULL);
        if (input_x_option & OTBL_SHRINK)
        {
            g_object_set (input, "halign", GTK_ALIGN_CENTER,
                                 "hexpand", FALSE, NULL);
        }

        if (input_y_option & OTBL_FILL)
            g_object_set (input, "valign", GTK_ALIGN_FILL, NULL);
        if (input_y_option & OTBL_EXPAND)
            g_object_set (input, "vexpand", TRUE, NULL);
        if (input_y_option & OTBL_SHRINK)
        {
            g_object_set (input, "valign", GTK_ALIGN_CENTER,
                                 "vexpand", FALSE, NULL);
        }
    }
}

GtkWidget *orage_menu_new(const gchar *menu_header_title
    , GtkWidget *menu_bar)
{
    GtkWidget *menu_header, *menu;

    menu_header = gtk_menu_item_new_with_mnemonic(menu_header_title);
    gtk_container_add(GTK_CONTAINER(menu_bar), menu_header);

    menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_header), menu);

    return menu;
}

GtkWidget *orage_separator_menu_item_new(GtkWidget *menu)
{
    GtkWidget *menu_item;

    menu_item = gtk_separator_menu_item_new();
    gtk_container_add(GTK_CONTAINER(menu), menu_item);
    return menu_item;
}

GtkWidget *orage_menu_item_new_with_mnemonic (const gchar *label,
                                              GtkWidget *menu)
{
    GtkWidget *menu_item;

    menu_item = gtk_menu_item_new_with_mnemonic (label);
    gtk_container_add (GTK_CONTAINER (menu), menu_item);
    return menu_item;
}

/* replace old with new string in text.
   if changes are done, returns newly allocated char *, which needs to be freed
   if there are no changes, it returns the original string without freeing it.
   You can always use this like 
   str=orage_replace_text(str, old, new);
   but it may point to new place.
*/
char *orage_replace_text(char *text, char *old, char *new)
{
    /* these point to the original string and travel it until no more olds 
     * are found:
     * cur points to the current head (after each old if any found)
     * cmd points to the start of next old in text */
    char *cur, *cmd;
    char *beq=NULL; /* beq is the total new string. */
    char *tmp;      /* temporary pointer to handle freeing */

    for (cur = text; cur && (cmd = strstr(cur, old)); cur = cmd + strlen(old)) {
        cmd[0] = '\0'; /* temporarily */
        if (beq) { /* we already have done changes */
            tmp = beq;
            beq = g_strconcat(tmp, cur, new, NULL);
            g_free(tmp);
        }
        else /* first round */
            beq = g_strconcat(cur, new, NULL);
        cmd[0] = old[0]; /* back to real value */
    }

    if (beq) {
        /* we found and processed at least one command, 
         * add the remaining fragment and return it */
        tmp = beq;
        beq = g_strconcat(tmp, cur, NULL);
        g_free(tmp);
        g_free(text); /* free original string as we changed it */
    }
    else {
        /* we did not find any commands,
         * so just return the original string */
        beq = text;
    }
    return(beq);
}

/* This is helper function for orage_limit_text. It takes char pointer
   to string start and length and adds that "line" to the beginning of 
   old_result and returns the new_result.
*/
static char* add_line(char *old_result, char *start, int line_len
        , int max_line_len)
{
    char *tmp;
    char *line;          /* new line to add */
    char *new_result;

    if (line_len > max_line_len) {
        tmp = g_strndup(start, max_line_len-3);
        if (*(start+line_len-1) == '\n') { /* need to add line change */
            line = g_strjoin("", tmp, "...\n", NULL);
        }
        else {
            line = g_strjoin("", tmp, "...", NULL);
        }
        g_free(tmp);
    }
    else {
        line = g_strndup(start, line_len);
    }
    /* note that old_result can be NULL */
    new_result = g_strjoin("", line, old_result, NULL);
    g_free(line);
    g_free(old_result);
    return(new_result);
}

/* cuts text to fit in a box defined with max_line_len and max_lines.
   This is usefull for example in tooltips to avoid too broad and too may lines
   to be visible.
   You can always use this like 
   str=orage_limit_text(str, max_line_len, max_lines);
   but it may point to new place.
*/
char *orage_limit_text(char *text, int max_line_len, int max_lines)
{
    /* these point to the original string and travel it until no more cuts 
     * are needed:
     * cur points to the current end of text character.
     * eol is pointing to the end of current line */
    char *cur, *eol;
    char *result=NULL;   /* end result is collected to this */
    int text_len=strlen(text);
    int line_cnt=0;

    if (text_len < 2) /* nothing to do */
        return(text);
    /* we start from the end and collect upto max_lines to a new buffer, but
     * cut them to max_line_len before accepting */
    for (cur = text+text_len-2, eol = text+text_len-1;
         (cur > text) && (line_cnt < max_lines); cur--) {
        if (*cur == '\n') {
            /* line found, collect the line */
            result = add_line(result, cur+1, eol-cur, max_line_len);
            line_cnt++;
            eol = cur;
        }
    }
    /* Need to process first line also as it does not start with line end */
    if ((cur == text) && (line_cnt < max_lines)) {
        result = add_line(result, cur, eol-cur+1, max_line_len);
    }
    if (result) {
        g_free(text);
        return(result);
    }
    else 
        return(text);
}

/* this will change <&Xnnn> type commands to numbers or text as defined.
 * Currently the only command is 
 * <&Ynnnn> which calculates years between current year and nnnn */
gchar *orage_process_text_commands (const gchar *text)
{
    /* these point to the original string and travel it until no more commands 
     * are found:
     * cur points to the current head
     * cmd points to the start of new command
     * end points to the end of new command */
    const gchar *cur;
    char *end, *cmd;
    /* these point to the new string, which has commands in processed form:
     * new is the new fragment to be added
     * beq is the total new string. */
    char *new=NULL, *beq=NULL;
    char *tmp; /* temporary pointer to handle freeing */
    int start_year = -1, year_diff, res;
    GDateTime *gdt;

    /**** RULE <&Ynnnn> difference of the nnnn year and current year *****/
    /* This is usefull in birthdays for example: I will be <&Y1980>
     * translates to "I will be 29" if the alarm is raised on 2009 */
    for (cur = text; cur && (cmd = strstr(cur, "<&Y")); cur = end) {
        if ((end = strstr(cmd, ">"))) {
            end[0] = '\0'; /* temporarily. */
            res = sscanf(cmd, "<&Y%d", &start_year);
            end[0] = '>'; /* put it back. */
            if (res == 1 && start_year > 0) { /* we assume success */
                gdt = g_date_time_new_now_local ();
                year_diff = g_date_time_get_year (gdt) - start_year;
                g_date_time_unref (gdt);

                if (year_diff > 0) { /* sane value */
                    end++; /* next char after > */
                    cmd[0] = '\0'; /* temporarily. (this ends cur) */
                    new = g_strdup_printf("%s%d", cur, year_diff);
                    cmd[0] = '<'; /* put it back */
                    if (beq) { /* this is normal round after first */
                        tmp = beq;
                        beq = g_strdup_printf("%s%s", tmp, new);
                        g_free(tmp);
                    }
                    else { /* first round, we do not have beq yet */
                        beq = g_strdup(new);
                    }
                    g_free(new);
                }
                else
                {
                    g_warning ("%s: start year is too big (%d).",
                               G_STRFUNC, start_year);
                }
            }
            else
            {
                g_warning ("%s: failed to understand parameter (%s).",
                           G_STRFUNC, cmd);
            }
        }
        else
            g_warning ("%s: parameter (%s) misses ending >.", G_STRFUNC, cmd);
    }

    if (beq) {
        /* we found and processed at least one command, 
         * add the remaining fragment and return it */
        tmp = beq;
        beq = g_strdup_printf("%s%s", tmp, cur);
        g_free(tmp);
    }
    else {
        /* we did not find any commands,
         * so just return duplicate of the original string */
        beq = g_strdup(text);
    }
    return(beq);
}

/** Create new horizontal filler with given width.
 *  @param width filler width
 */
static GtkWidget *orage_hfiller_new (const gint width)
{
    GtkWidget *label = gtk_label_new ("");

    g_object_set (label, "margin-right", width, NULL);

    return label;
}

GtkWidget *orage_period_hbox_new (const gboolean head_space,
                                  const gboolean tail_space,
                                  GtkWidget *spin_dd, GtkWidget *dd_label,
                                  GtkWidget *spin_hh, GtkWidget *hh_label,
                                  GtkWidget *spin_mm, GtkWidget *mm_label)
{
    const gint field_fill_width = 15;
    const gint spinner_and_label_fill_width = 5;
    GtkWidget *hbox, *filler;

    hbox = gtk_grid_new ();

    if (head_space) {
        filler = orage_hfiller_new (field_fill_width);
        gtk_grid_attach_next_to (GTK_GRID (hbox), filler, NULL,
                                 GTK_POS_RIGHT, 1, 1);
    }

    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_dd), TRUE);
    gtk_grid_attach_next_to (GTK_GRID (hbox), spin_dd, NULL,
                             GTK_POS_RIGHT, 1, 1);

    filler = orage_hfiller_new (spinner_and_label_fill_width);
    gtk_grid_attach_next_to (GTK_GRID (hbox), filler, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (hbox), dd_label, NULL,
                             GTK_POS_RIGHT, 1, 1);

    filler = orage_hfiller_new (field_fill_width);
    gtk_grid_attach_next_to (GTK_GRID (hbox), filler, NULL,
                             GTK_POS_RIGHT, 1, 1);

    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_hh), TRUE);

    gtk_grid_attach_next_to (GTK_GRID (hbox), spin_hh, NULL,
                             GTK_POS_RIGHT, 1, 1);

    filler = orage_hfiller_new (spinner_and_label_fill_width);
    gtk_grid_attach_next_to (GTK_GRID (hbox), filler, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (hbox), hh_label, NULL,
                             GTK_POS_RIGHT, 1, 1);

    filler = orage_hfiller_new (field_fill_width);
    gtk_grid_attach_next_to (GTK_GRID (hbox), filler, NULL,
                             GTK_POS_RIGHT, 1, 1);

    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spin_mm), TRUE);
    gtk_spin_button_set_increments (GTK_SPIN_BUTTON (spin_mm), 5, 10);

    gtk_grid_attach_next_to (GTK_GRID (hbox), spin_mm, NULL,
                             GTK_POS_RIGHT, 1, 1);

    filler = orage_hfiller_new (spinner_and_label_fill_width);
    gtk_grid_attach_next_to (GTK_GRID (hbox), filler, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (hbox), mm_label, NULL,
                             GTK_POS_RIGHT, 1, 1);

    if (tail_space) {
        filler = orage_hfiller_new (field_fill_width);
        gtk_grid_attach_next_to (GTK_GRID (hbox), filler, NULL,
                                 GTK_POS_RIGHT, 1, 1);
    }

    return hbox;
}

GtkWidget *orage_create_framebox_with_content (const gchar *title,
                                               const GtkShadowType shadow_type,
                                               GtkWidget *content)
{
    GtkWidget *framebox;
    gchar *tmp;
    GtkWidget *label;

    framebox = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(framebox), shadow_type);
    gtk_frame_set_label_align(GTK_FRAME(framebox), 0.0, 1.0);
    gtk_widget_show(framebox);

    if (title) {
        tmp = g_strdup_printf("<b>%s</b>", title);
        label = gtk_label_new(tmp);
        gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
        g_object_set (label, "xalign", 0.0, "yalign", 0.5,
                             "margin-left", 5,
                             NULL);
        gtk_widget_show(label);
        gtk_frame_set_label_widget(GTK_FRAME(framebox), label);
        g_free(tmp);
    }

    g_object_set (content, "margin-top", 5,
                           "margin-bottom", 5,
                           "margin-left", 21,
                           "margin-right", 5,
                           NULL);
    gtk_container_add (GTK_CONTAINER (framebox), content);

    return(framebox);
}

/*******************************************************
 * time convert and manipulation functions
 *******************************************************/

GDateTime *orage_cal_to_gdatetime (GtkCalendar *cal,
                                   const gint hh, const gint mm)
{
    guint year;
    guint month;
    guint day;
    GDateTime *gdt;

    gtk_calendar_get_date (cal, &year, &month, &day);
    month += 1;
    gdt = g_date_time_new_local (year, month, day, hh, mm, 0);
    if (gdt == NULL)
    {
        g_error ("%s failed %04d-%02d-%02d %02d:%02d",
                 G_STRFUNC, year, month, day, hh, mm);
    }

    return gdt;
}

GDateTime *orage_icaltime_to_gdatetime (const gchar *icaltime)
{
    struct tm t = {0};
    char *ret;

    ret = strptime (icaltime, "%Y%m%dT%H%M%S", &t);
    if (ret == NULL)
    {
        /* Not all format string matched, so it must be DATE. Depending on OS
         * and libs, it is not always guaranteed that all required fields
         * are filled. Convert only with date formatter.
         */
        if (strptime (icaltime, "%Y%m%d", &t) == NULL)
        {
            g_warning ("%s: icaltime string '%s' conversion failed",
                       G_STRFUNC, icaltime);

            return NULL;
        }

        /* Need to fill missing tm_wday and tm_yday, which are in use in some
         * locale's default date. For example in en_IN. mktime does it.
         */
        if (mktime(&t) == (time_t) -1)
        {
            g_warning ("%s failed %d %d %d",
                       G_STRFUNC, t.tm_year, t.tm_mon, t.tm_mday);

            return NULL;
        }

        t.tm_hour = 0;
        t.tm_min = 0;
        t.tm_sec = 0;
    }
    else if (ret[0] != '\0') { /* icaltime was not processed completely */
        /* UTC times end to Z, which is ok */
        if (ret[0] != 'Z' || ret[1] != '\0') /* real error */
            g_error ("%s icaltime='%s' ret='%s'", G_STRFUNC, icaltime, ret);
    }

    t.tm_year += 1900;
    t.tm_mon += 1;

    return g_date_time_new_local (t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour,
                                  t.tm_min, t.tm_sec);
}

gchar *orage_gdatetime_to_i18_time (GDateTime *gdt, const gboolean date_only)
{
    const gchar *fmt = date_only ? "%x" : "%x %R";

    return g_date_time_format (gdt, fmt);
}

gchar *orage_gdatetime_to_icaltime (GDateTime *gdt, const gboolean date_only)
{
    gint year;
    gint month;
    gint day;
    gchar *str;
    size_t len;

    g_date_time_get_ymd (gdt, &year, &month, &day);

    /* g_date_time_format with format = XFICAL_APPT_TIME_FORMAT does not padd
     * year with leading 0, this is incompatible with ical. To add padding we
     * need to use snprintf.
     */

    if (date_only)
    {
        len = 10;
        str = g_malloc (len);
        g_snprintf (str, len, "%04d%02d%02d", year, month, day);
    }
    else
    {
        len = 17;
        str = g_malloc (len);
        g_snprintf (str, len, "%04d%02d%02dT%02d%02d%02d",
                    year, month, day,
                    g_date_time_get_hour (gdt),
                    g_date_time_get_minute (gdt),
                    g_date_time_get_second (gdt));
    }

    return str;
}

void orage_gdatetime_unref (GDateTime *gdt)
{
    if (gdt == NULL)
        return;

    g_date_time_unref (gdt);
}

static GDate *orage_gdatetime_to_gdate (GDateTime *gdt)
{
    gint year;
    gint month;
    gint day;

    g_date_time_get_ymd (gdt, &year, &month, &day);

    return g_date_new_dmy (day, month, year);
}

gint orage_gdatetime_days_between (GDateTime *gdt1, GDateTime *gdt2)
{
    GDate *g_t1, *g_t2;
    gint dd;

    g_t1 = orage_gdatetime_to_gdate (gdt1);
    g_t2 = orage_gdatetime_to_gdate (gdt2);
    dd = g_date_days_between (g_t1, g_t2);
    g_date_free (g_t1);
    g_date_free (g_t2);

    return dd;
}

void orage_select_date (GtkCalendar *cal, GDateTime *gdt)
{
    guint cur_year, cur_month, cur_mday;
    gint year;
    gint month;
    gint day;

    g_date_time_get_ymd (gdt, &year, &month, &day);
    month -= 1;
    gtk_calendar_get_date(cal, &cur_year, &cur_month, &cur_mday);

    if (((gint)cur_year == year) && ((gint)cur_month == month))
        gtk_calendar_select_day(cal, day);
    else {
        gtk_calendar_select_day(cal, 1); /* need to avoid illegal day/month */
        gtk_calendar_select_month(cal, month, year);
        gtk_calendar_select_day(cal, day);
    }
}

void orage_select_today(GtkCalendar *cal)
{
    GDateTime *gdt;

    gdt = g_date_time_new_now_local ();
    orage_select_date (cal, gdt);
    g_date_time_unref (gdt);
}

/*******************************************************
 * data and config file locations
 *******************************************************/

gboolean orage_copy_file (const gchar *source, const gchar *target)
{
    gchar *text;
    gsize text_len;
    GError *error = NULL;
    gboolean ok = TRUE;

    /* read file */
    if (!g_file_get_contents(source, &text, &text_len, &error)) {
        g_warning ("%s: Could not open file (%s) error:%s", G_STRFUNC, source,
                   error->message);
        g_error_free(error);
        ok = FALSE;
    }
    /* write file */
    if (ok && !g_file_set_contents(target, text, -1, &error)) {
        g_warning ("%s: Could not write file (%s) error:%s", G_STRFUNC, target,
                   error->message);
        g_error_free(error);
        ok = FALSE;
    }
    g_free(text);
    return(ok);
}

static gchar *orage_xdg_system_data_file_location (const gchar *name)
{
    gchar *file_name;
    const gchar * const *base_dirs;
    int i;

    base_dirs = g_get_system_data_dirs();
    for (i = 0; base_dirs[i] != NULL; i++) {
        file_name = g_build_filename(base_dirs[i], name, NULL);
        if (g_file_test(file_name, G_FILE_TEST_IS_REGULAR)) {
            return(file_name);
        }
        g_free(file_name);
    }

    /* no system wide data file */
    return(NULL);
}

/* Returns valid xdg local data file name. 
   If the file did not exist, it checks if there are system defaults 
   and creates it from those */
gchar *orage_data_file_location (const gchar *name)
{
    gchar *file_name, *dir_name, *sys_name;
    const char *base_dir;
    int mode = 0700;

    base_dir = g_get_user_data_dir();
    file_name = g_build_filename(base_dir, name, NULL);
    if (!g_file_test(file_name, G_FILE_TEST_IS_REGULAR)) {
        /* it does not exist, let's try to create it */
        dir_name = g_path_get_dirname (file_name);
        if (g_mkdir_with_parents(dir_name, mode)) {
            g_warning ("%s: (%s) (%s) directory creation failed", G_STRFUNC,
                       base_dir, file_name);
        }
        g_free(dir_name);
        /* now we have the directories ready, let's check for system default */
        sys_name = orage_xdg_system_data_file_location(name);
        if (sys_name) { /* we found it, we need to copy it */
            orage_copy_file(sys_name, file_name);
        }
    }

    return(file_name);
}

/* these are default files and must only be read */
static gchar *orage_xdg_system_config_file_location (const gchar *name)
{
    char *file_name;
    const gchar * const *base_dirs;
    int i;

    base_dirs = g_get_system_config_dirs();
    for (i = 0; base_dirs[i] != NULL; i++) {
        file_name = g_build_filename(base_dirs[i], name, NULL);
        if (g_file_test(file_name, G_FILE_TEST_IS_REGULAR)) {
            return(file_name);
        }
        g_free(file_name);
    }

    /* no system wide config file */
    return(NULL);
}

/* Returns valid xdg local config file name. 
   If the file did not exist, it checks if there are system defaults 
   and creates it from those */
gchar *orage_config_file_location (const gchar *name)
{
    gchar *file_name, *dir_name, *sys_name;
    const char *base_dir;
    int mode = 0700;

    base_dir = g_get_user_config_dir();
    file_name = g_build_filename(base_dir, name, NULL);
    if (!g_file_test(file_name, G_FILE_TEST_IS_REGULAR)) {
        /* it does not exist, let's try to create it */
        dir_name = g_path_get_dirname (file_name);
        if (g_mkdir_with_parents(dir_name, mode)) {
            g_warning ("%s: (%s) (%s) directory creation failed", G_STRFUNC,
                       base_dir, file_name);
        }
        g_free(dir_name);
        /* now we have the directories ready, let's check for system default */
        sys_name = orage_xdg_system_config_file_location(name);
        if (sys_name) { /* we found it, we need to copy it */
            orage_copy_file(sys_name, file_name);
        }
    }

    return(file_name);
}

/*******************************************************
 * dialog functions
 *******************************************************/

static void orage_gen_info_dialog (GtkWindow *parent,
                                   const GtkMessageType message_type,
                                   const gchar *primary_text,
                                   const gchar *secondary_text)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (parent,
                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                     message_type, GTK_BUTTONS_OK, "%s",
                                     primary_text);

    if (secondary_text)
    {
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", secondary_text);
    }

    (void)gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

void orage_info_dialog (GtkWindow *parent,
                        const gchar *primary_text,
                        const gchar *secondary_text)
{
    orage_gen_info_dialog (parent, GTK_MESSAGE_INFO, primary_text,
                           secondary_text);
}

gint orage_warning_dialog (GtkWindow *parent, const gchar *primary_text,
                                              const gchar *secondary_text,
                                              const gchar *no_text,
                                              const gchar *yes_text)
{
    GtkWidget *dialog;
    gint result;

    dialog = gtk_message_dialog_new(parent
            , GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT
            , GTK_MESSAGE_WARNING
            , GTK_BUTTONS_NONE
            , "%s", primary_text);
    if (secondary_text) 
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog)
                , "%s", secondary_text);
    gtk_dialog_add_buttons(GTK_DIALOG(dialog)
            , no_text, GTK_RESPONSE_NO, yes_text, GTK_RESPONSE_YES, NULL);
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return(result);
}

void orage_error_dialog (GtkWindow *parent, const gchar *primary_text,
                                            const gchar *secondary_text)
{
    orage_gen_info_dialog (parent, GTK_MESSAGE_ERROR, primary_text,
                           secondary_text);
}

#ifdef HAVE_X11_TRAY_ICON
gboolean orage_status_icon_is_embedded (GtkStatusIcon *status_icon)
{
    gboolean is_embedded;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    is_embedded = gtk_status_icon_is_embedded (status_icon);
    G_GNUC_END_IGNORE_DEPRECATIONS

    return is_embedded;
}

void orage_status_icon_set_visible (GtkStatusIcon *status_icon,
                                    const gboolean visible)
{
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_status_icon_set_visible (status_icon, visible);
    G_GNUC_END_IGNORE_DEPRECATIONS
}

GtkStatusIcon *orage_status_icon_new_from_pixbuf (GdkPixbuf *pixbuf)
{
    GtkStatusIcon *status_icon;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    status_icon = gtk_status_icon_new_from_pixbuf (pixbuf);
    G_GNUC_END_IGNORE_DEPRECATIONS

    return status_icon;
}

void orage_status_icon_set_tooltip_markup (GtkStatusIcon *status_icon,
                                           const gchar *markup)
{
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_status_icon_set_tooltip_markup (status_icon, markup);
    G_GNUC_END_IGNORE_DEPRECATIONS
}
#endif

GtkWidget *orage_util_image_button (const gchar *icon_name, const gchar *label)
{
    GtkWidget *button;
    GtkWidget *image;

    image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
    gtk_widget_show (image);

    button = gtk_button_new_with_mnemonic (label);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_show (button);

    return button;
}

gboolean orage_str_to_rgba (const gchar *color_str,
                             GdkRGBA *rgba,
                             const gchar *def)
{
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    gboolean result;

    if (strstr (color_str, "rgb"))
    {
        if (gdk_rgba_parse (rgba, color_str) == FALSE)
        {
            g_warning ("unable to parse rgba colour string '%s', using default",
                       color_str);

            result = gdk_rgba_parse (rgba, def);
        }
        else
            result = TRUE;
    }
    else
    {
        if (sscanf (color_str, "%uR %uG %uB", &red, &green, &blue) == 3)
        {
            rgba->red = CLAMP ((double)red / 65535.0, 0.0, 1.0);
            rgba->green = CLAMP ((double)green / 65535.0, 0.0, 1.0);
            rgba->blue = CLAMP ((double)blue / 65535.0, 0.0, 1.0);
            rgba->alpha = 1.0;
            result = TRUE;
        }
        else
        {
            g_warning ("unable to parse legacy Orage colour string '%s', "
                       "using default '%s'", color_str, def);

            result = gdk_rgba_parse (rgba, def);
        }
    }

    return result;
}

gboolean orage_is_debug_logging_enabled (void)
{
    static gboolean enabled = FALSE;
    static gboolean visited = FALSE;
    const gchar *domains;
    const gchar *log_domain = G_LOG_DOMAIN;

    if (visited)
        return enabled;

    domains = g_getenv ("G_MESSAGES_DEBUG");
    if (domains == NULL)
        enabled = FALSE;
    else if (strcmp (domains, "all") == 0)
        enabled = TRUE;
    else if (log_domain != NULL && strstr (domains, log_domain))
        enabled = TRUE;
    else
        enabled = FALSE;

    visited = TRUE;

    return enabled;
}

void orage_open_help_page (void)
{
    const gchar *helpdoc;
    GError *error = NULL;

    helpdoc = "exo-open " ORAGE_DOC_ADDRESS;
    if (orage_exec (helpdoc, NULL, &error) == FALSE)
    {
        g_message ("%s failed: %s. Trying firefox", helpdoc, error->message);
        g_clear_error (&error);

        helpdoc = "firefox " ORAGE_DOC_ADDRESS;
        if (orage_exec (helpdoc, NULL, &error) == FALSE)
        {
            g_warning ("start of %s failed: %s", helpdoc, error->message);
            g_clear_error (&error);
        }
    }
}

GtkWidget *orage_image_menu_item_new_from_stock (const gchar *stock_id,
                                                 GtkAccelGroup *accel_group)
{
    GtkWidget *item;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    item = gtk_image_menu_item_new_from_stock (stock_id, accel_group);
    G_GNUC_END_IGNORE_DEPRECATIONS
    return item;
}

GtkWidget *orage_image_menu_item_for_parent_new_from_stock (
    const gchar *stock_id, GtkWidget *menu, GtkAccelGroup *accel_group)
{
    GtkWidget *item = orage_image_menu_item_new_from_stock (stock_id,
                                                            accel_group);

    gtk_container_add (GTK_CONTAINER (menu), item);

    return item;
}

GtkWidget *orage_image_menu_item_new (const gchar *label,
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
