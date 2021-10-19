/*      Orage - Calendar and alarm handler
 *
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
       Free Software Foundation
       51 Franklin Street, 5th Floor
       Boston, MA 02110-1301 USA

 */

#define _XOPEN_SOURCE /* glibc2 needs this */
#define _XOPEN_SOURCE_EXTENDED 1 /* strptime needs this in posix systems */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
/*
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
*/

#include "orage-i18n.h"
#include "functions.h"
#include "parameters.h"

#define ORAGE_DEBUG 0

/**************************************
 *  Debugging helping functions       *
 **************************************/
/* this is for testing only. it can be used to see where time is spent.
 * Add call program_log("dbus started") in the code and run orage like:
 * strace -ttt -f -o /tmp/logfile.strace ./orage
 * And then you can check results:
 * grep MARK /tmp/logfile.strace
 * grep MARK /tmp/logfile.strace|sed s/", F_OK) = -1 ENOENT (No such file or directory)"/\)/
 * */
#if 0
void program_log (const char *format, ...)
{
        va_list args;
        char *formatted, *str;

        va_start (args, format);
        formatted = g_strdup_vprintf (format, args);
        va_end (args);

        str = g_strdup_printf ("MARK: %s: %s", g_get_prgname(), formatted);
        g_free (formatted);

        access (str, F_OK);
        g_free (str);
}
#endif

/**************************************
 *  General purpose helper functions  *
 **************************************/

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

gboolean orage_date_button_clicked(GtkWidget *button, GtkWidget *selDate_dialog)
{
/*  GtkWidget *selDate_dialog; */
    GtkWidget *selDate_calendar;
    gint result;
    char *new_date=NULL;
    const gchar *cur_date;
    struct tm cur_t;
    gboolean changed, allocated=FALSE;

    /*
       For some unknown reason NLS does not work in this file, so this
       has to be done in the main code:
    selDate_dialog = gtk_dialog_new_with_buttons(
            _("Pick the date"), GTK_WINDOW(win),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("Today"),
            1,
            "_OK",
            GTK_RESPONSE_ACCEPT,
            NULL);
            */

    selDate_calendar = gtk_calendar_new();
    gtk_container_add(
        GTK_CONTAINER(gtk_dialog_get_content_area (GTK_DIALOG(selDate_dialog)))
            , selDate_calendar);

    cur_date = (char *)gtk_button_get_label(GTK_BUTTON(button));
    if (cur_date)
        cur_t = orage_i18_date_to_tm_date(cur_date);
    else /* something was wrong. let's return some valid value */
        cur_t = orage_i18_date_to_tm_date(orage_localdate_i18());

    orage_select_date(GTK_CALENDAR(selDate_calendar)
            , cur_t.tm_year+1900, cur_t.tm_mon, cur_t.tm_mday);
    gtk_widget_show_all(selDate_dialog);

    result = gtk_dialog_run(GTK_DIALOG(selDate_dialog));
    switch(result){
        case GTK_RESPONSE_ACCEPT:
            new_date = orage_cal_to_i18_date(GTK_CALENDAR(selDate_calendar));
            break;
        case 1:
            new_date = orage_localdate_i18();
            break;
        case GTK_RESPONSE_DELETE_EVENT:
        default:
            new_date = g_strdup(cur_date);
            allocated = TRUE;
            break;
    }
    if (g_ascii_strcasecmp(new_date, cur_date) != 0)
        changed = TRUE;
    else
        changed = FALSE;
    gtk_button_set_label(GTK_BUTTON(button), (const gchar *)new_date);
    if (allocated)
        g_free(new_date);
    gtk_widget_destroy(selDate_dialog);
    return(changed);
}

static void child_setup_async(gpointer user_data)
{
#if defined(HAVE_SETSID) && !defined(G_OS_WIN32)
    setsid();
#endif
    
    (void)user_data;
}

static void child_watch_cb(GPid pid, gint status, gpointer data)
{
    gboolean *cmd_active = (gboolean *)data;

    waitpid(pid, NULL, 0);
    g_spawn_close_pid(pid);
    *cmd_active = FALSE;
    
    (void)status;
}

gboolean orage_exec(const gchar *cmd, gboolean *cmd_active, GError **error)
{
    char **argv;
    gboolean success;
    int spawn_flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD;
    GPid pid;

    if (!g_shell_parse_argv(cmd, NULL, &argv, error))
        return(FALSE);

    if (!argv || !argv[0])
        return(FALSE);

    success = g_spawn_async(NULL, argv, NULL, spawn_flags
            , child_setup_async, NULL, &pid, error);
    if (cmd_active) {
        if (success)
            *cmd_active = TRUE;
        g_child_watch_add(pid, child_watch_cb, cmd_active);
    }
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
    gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button), tooltip_text);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (button), pos);
    
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

void orage_table_add_row (GtkWidget *table, GtkWidget *label, 
                          GtkWidget *input, const guint row,
                          const GtkAttachOptions input_x_option,
                          const GtkAttachOptions input_y_option)
{
    if (label)
    {
        gtk_grid_attach (GTK_GRID (table), label, 0, row, 1, 1);
        g_object_set (label, "xalign", 0.0, "yalign", 0.5,
                             "halign", GTK_ALIGN_FILL,
                             NULL);
    }
    
    if (input)
    {
        gtk_grid_attach (GTK_GRID (table), input, 1, row, 1, 1);
        
        if (input_x_option & GTK_FILL)
            g_object_set (input, "halign", GTK_ALIGN_FILL, NULL);
        if (input_x_option & GTK_EXPAND)
            g_object_set (input, "hexpand", TRUE, NULL);
        if (input_x_option & GTK_SHRINK)
        {
            g_object_set (input, "halign", GTK_ALIGN_CENTER,
                                 "hexpand", FALSE, NULL);
        }
        
        if (input_y_option & GTK_FILL)
            g_object_set (input, "valign", GTK_ALIGN_FILL, NULL);
        if (input_y_option & GTK_EXPAND)
            g_object_set (input, "vexpand", TRUE, NULL);
        if (input_y_option & GTK_SHRINK)
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

GtkWidget *orage_image_menu_item_new_from_stock(const gchar *stock_id
    , GtkWidget *menu, GtkAccelGroup *ag)
{
    GtkWidget *menu_item;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    menu_item = gtk_image_menu_item_new_from_stock (stock_id, ag);
G_GNUC_END_IGNORE_DEPRECATIONS
    gtk_container_add(GTK_CONTAINER(menu), menu_item);
    return menu_item;
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
#undef P_N
#define P_N "orage_replace_text: "
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
#undef P_N
#define P_N "orage_limit_text: "
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
char *orage_process_text_commands(char *text)
{
#undef P_N
#define P_N "process_text_commands: "
    /* these point to the original string and travel it until no more commands 
     * are found:
     * cur points to the current head
     * cmd points to the start of new command
     * end points to the end of new command */
    char *cur, *end, *cmd;
    /* these point to the new string, which has commands in processed form:
     * new is the new fragment to be added
     * beq is the total new string. */
    char *new=NULL, *beq=NULL;
    char *tmp; /* temporary pointer to handle freeing */
    int start_year = -1, year_diff, res;
    struct tm *cur_time;

    /**** RULE <&Ynnnn> difference of the nnnn year and current year *****/
    /* This is usefull in birthdays for example: I will be <&Y1980>
     * translates to "I will be 29" if the alarm is raised on 2009 */
    for (cur = text; cur && (cmd = strstr(cur, "<&Y")); cur = end) {
        if ((end = strstr(cmd, ">"))) {
            end[0] = '\0'; /* temporarily. */
            res = sscanf(cmd, "<&Y%d", &start_year);
            end[0] = '>'; /* put it back. */
            if (res == 1 && start_year > 0) { /* we assume success */
                cur_time = orage_localtime();
                year_diff = cur_time->tm_year + 1900 - start_year;
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
                    g_warning (P_N "start year is too big (%d).", start_year);
            }
            else
                g_warning (P_N "failed to understand parameter (%s).", cmd);
        }
        else
            g_warning (P_N "parameter (%s) misses ending >.", cmd);
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

    if (head_space)
    {
        filler = orage_hfiller_new (field_fill_width);
        gtk_grid_attach_next_to (GTK_GRID (hbox), filler, NULL,
                                 GTK_POS_RIGHT, 1, 1);
    }
    
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spin_dd), TRUE);
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

    if (tail_space)
    {
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
    GtkWidget *label;
    gchar *tmp;

    framebox = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (framebox), shadow_type);
    gtk_frame_set_label_align (GTK_FRAME (framebox), 0.0, 1.0);
    gtk_widget_show (framebox);

    if (title)
    {
        tmp = g_strdup_printf ("<b>%s</b>", title);
        label = gtk_label_new (tmp);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        g_object_set (label, "xalign", 0.0, "yalign", 0.5,
                             "margin-left", 5,
                             NULL);
        gtk_widget_show (label);
        gtk_frame_set_label_widget (GTK_FRAME (framebox), label);
        g_free (tmp);
    }

    g_object_set (content, "margin-top", 5,
                           "margin-bottom", 5,
                           "margin-left", 21,
                           "margin-right", 5,
                           NULL);
    gtk_container_add (GTK_CONTAINER (framebox), content);

    return framebox;
}

/*******************************************************
 * time convert and manipulation functions
 *******************************************************/

struct tm orage_i18_time_to_tm_time (const gchar *i18_time)
{
    char *ret;
    struct tm tm_time = {0};

    ret = (char *)strptime(i18_time, "%x %R", &tm_time);
    if (ret == NULL)
    {
        g_error ("Orage: orage_i18_time_to_tm_time wrong format (%s)",
                 i18_time);
    }
    else if (ret[0] != '\0')
    {
        g_warning ("Orage: orage_i18_time_to_tm_time too long format (%s). Ignoring:%s)"
                , i18_time, ret);
    }
    
    return (tm_time);
}

struct tm orage_i18_date_to_tm_date (const gchar *i18_date)
{
    char *ret;
    struct tm tm_date = {0};
    
    ret = strptime ((const char *)i18_date, "%x", &tm_date);
    if (ret == NULL)
        g_error("Orage: orage_i18_date_to_tm_date wrong format (%s)", i18_date);
    else if (ret[0] != '\0')
    {
        g_warning("Orage: orage_i18_date_to_tm_date too long format (%s). Ignoring:%s)"
                , i18_date, ret);
    }
    
    return(tm_date);
}

gchar *orage_tm_time_to_i18_time(struct tm *tm_time)
{
    static gchar i18_time[128];

    if (strftime (i18_time, sizeof (i18_time), "%x %R", tm_time) == 0)
        g_error("Orage: orage_tm_time_to_i18_time too long string in strftime");
    return(i18_time);
}

gchar *orage_tm_date_to_i18_date (const struct tm *tm_date)
{
    static gchar i18_date[128];

    if (strftime ((char *)i18_date, sizeof (i18_date), "%x", tm_date) == 0)
        g_error ("orage_tm_date_to_i18_date too long string in strftime");
    
    return(i18_date);
}

struct tm orage_cal_to_tm_time(GtkCalendar *cal, gint hh, gint mm)
{
    struct tm tm_date = {0};
 
    /* dst needs to -1 or mktime adjusts time if we are in another
     * dst setting. */
    tm_date.tm_isdst = -1;

    gtk_calendar_get_date(cal
            , (unsigned int *)&tm_date.tm_year
            , (unsigned int *)&tm_date.tm_mon
            , (unsigned int *)&tm_date.tm_mday);
    tm_date.tm_year -= 1900;
    tm_date.tm_hour = hh;
    tm_date.tm_min = mm;
    /* need to fill missing tm_wday and tm_yday, which are in use 
     * in some locale's default date. For example in en_IN. mktime does it */
    if (mktime(&tm_date) == (time_t) -1) {
        g_warning("orage: orage_cal_to_tm_time mktime failed %d %d %d"
                , tm_date.tm_year, tm_date.tm_mon, tm_date.tm_mday);
    }
    return(tm_date);
}

char *orage_cal_to_i18_time(GtkCalendar *cal, gint hh, gint mm)
{
    struct tm tm_date = {0};
    
    tm_date.tm_isdst = -1;

    tm_date = orage_cal_to_tm_time(cal, hh, mm);
    return(orage_tm_time_to_i18_time(&tm_date));
}

char *orage_cal_to_i18_date(GtkCalendar *cal)
{
    struct tm tm_date = {0};
    
    tm_date.tm_isdst = -1;

    tm_date = orage_cal_to_tm_time(cal, 1, 1);
    return(orage_tm_date_to_i18_date(&tm_date));
}

char *orage_cal_to_icaldate(GtkCalendar *cal)
{
    struct tm tm_date = {0};
    char *icalt;
    
    tm_date.tm_isdst = -1;

    tm_date = orage_cal_to_tm_time(cal, 1, 1);
    icalt = orage_tm_time_to_icaltime(&tm_date);
    icalt[8] = '\0'; /* we know it is date */
    return(icalt);
}

struct tm orage_icaltime_to_tm_time (const gchar *icaltime,
                                     const gboolean real_tm)
{
    struct tm t = {0};
    char *ret;

    ret = strptime(icaltime, "%Y%m%dT%H%M%S", &t);
    if (ret == NULL) {
        /* not all format string matched, so it must be DATE */
        /* and tm_wday is not calculated ! */
    /* need to fill missing tm_wday and tm_yday, which are in use 
     * in some locale's default date. For example in en_IN. mktime does it */
        if (mktime(&t) == (time_t) -1) {
            g_warning("orage: orage_icaltime_to_tm_time mktime failed %d %d %d"
                    , t.tm_year, t.tm_mon, t.tm_mday);
        }
        t.tm_hour = -1;
        t.tm_min = -1;
        t.tm_sec = -1;
    }
    else if (ret[0] != 0) { /* icaltime was not processed completely */
        /* UTC times end to Z, which is ok */
        if (ret[0] != 'Z' || ret[1] != 0) /* real error */
            g_error("orage: orage_icaltime_to_tm_time error %s %s", icaltime
                    , ret);
    }

    if (!real_tm) { /* convert from standard tm format to "normal" format */
        t.tm_year += 1900;
        t.tm_mon += 1;
    }
    return(t);
}

gchar *orage_tm_time_to_icaltime(struct tm *t)
{
    static gchar icaltime[XFICAL_APPT_TIME_FORMAT_LEN];

    g_snprintf (icaltime, sizeof (icaltime), XFICAL_APPT_TIME_FORMAT,
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);

    return (icaltime);
}

char *orage_icaltime_to_i18_time(const char *icaltime)
{ /* timezone is not converted */
    struct tm t;
    char      *ct;

    t = orage_icaltime_to_tm_time(icaltime, TRUE);
    if (t.tm_hour == -1)
        ct = orage_tm_date_to_i18_date(&t);
    else
        ct = orage_tm_time_to_i18_time(&t);
    return(ct);
}

char *orage_icaltime_to_i18_time_only(const char *icaltime)
{
    struct tm t;
    static char i18_time[10];

    t = orage_icaltime_to_tm_time(icaltime, TRUE);
    if (strftime(i18_time, 10, "%R", &t) == 0)
        g_error("Orage: orage_icaltime_to_i18_time_short too long string in strftime");
    return(i18_time);
}

gchar *orage_i18_time_to_icaltime(const gchar *i18_time)
{
    struct tm t;
    char      *ct;

    t = orage_i18_time_to_tm_time(i18_time);
    ct = orage_tm_time_to_icaltime(&t);
    return(ct);
}

gchar *orage_i18_date_to_icaldate(const gchar *i18_date)
{
    struct tm t;
    gchar *icalt;

    t = orage_i18_date_to_tm_date(i18_date);
    icalt = orage_tm_time_to_icaltime(&t);
    icalt[8] = '\0'; /* we know it is date */
    return(icalt);
}

struct tm *orage_localtime(void)
{
    time_t tt;

    tt = time(NULL);
    return(localtime(&tt));
}

char *orage_localdate_i18(void)
{
    struct tm *t;

    t = orage_localtime();
    return(orage_tm_date_to_i18_date(t));
}

/* move one day forward or backward */
void orage_move_day(struct tm *t, const gint day)
{
    t->tm_mday += day; /* may be negative */
    /* mktime adjusts t correctly. It also fills missing tm_wday and tm_yday, 
     * which are in use in some locale's default date. For example in en_IN */
    if (mktime(t) == (time_t) -1) {
        g_warning("orage: orage_move_day mktime failed %d %d %d"
                , t->tm_year, t->tm_mon, t->tm_mday);
    }
}

gint orage_days_between (const struct tm *t1, const struct tm *t2)
{
    GDate *g_t1, *g_t2;
    gint dd;

    g_t1 = g_date_new_dmy(t1->tm_mday, t1->tm_mon, t1->tm_year);
    g_t2 = g_date_new_dmy(t2->tm_mday, t2->tm_mon, t2->tm_year);
    dd = g_date_days_between(g_t1, g_t2);
    g_date_free(g_t1);
    g_date_free(g_t2);
    return(dd);
}

void orage_select_date(GtkCalendar *cal
    , guint year, guint month, guint day)
{
    guint cur_year, cur_month, cur_mday;

    gtk_calendar_get_date(cal, &cur_year, &cur_month, &cur_mday);

    if (cur_year == year && cur_month == month)
        gtk_calendar_select_day(cal, day);
    else {
        gtk_calendar_select_day(cal, 0); /* need to avoid illegal day/month */
        gtk_calendar_select_month(cal, month, year);
        gtk_calendar_select_day(cal, day);
    }
}

void orage_select_today(GtkCalendar *cal)
{
    struct tm *t;

    t = orage_localtime();
    orage_select_date(cal, t->tm_year+1900, t->tm_mon, t->tm_mday);
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
        g_warning ("orage_copy_file: Could not open file (%s) error:%s"
                , source, error->message);
        g_error_free(error);
        ok = FALSE;
    }
    /* write file */
    if (ok && !g_file_set_contents(target, text, -1, &error)) {
        g_warning ("orage_copy_file: Could not write file (%s) error:%s"
                , target, error->message);
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
            g_warning ("orage_data_file_location: (%s) (%s) directory creation failed.\n", base_dir, file_name);
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
            g_warning ("orage_config_file_location: (%s) (%s) directory creation failed.\n", base_dir, file_name);
        }
        g_free(dir_name);
        /* now we have the directories ready, let's check for system default */
        sys_name = orage_xdg_system_config_file_location (name);
        if (sys_name) { /* we found it, we need to copy it */
            orage_copy_file(sys_name, file_name);
        }
    }

    return(file_name);
}

/*******************************************************
 * rc file interface
 *******************************************************/

OrageRc *orage_rc_file_open (const gchar *fpath, gboolean read_only)
{
#undef P_N
#define P_N "orage_rc_file_open: "
    /* XfceRc *rc; */
    OrageRc *orc = NULL;
    GKeyFile *grc;
    GError *error = NULL;

    grc = g_key_file_new();
    if (g_key_file_load_from_file(grc, fpath, G_KEY_FILE_KEEP_COMMENTS, &error))
    { /* success */
        orc = g_new(OrageRc, 1);
        orc->rc = grc;
        orc->read_only = read_only;
        orc->file_name = g_strdup(fpath);
        orc->cur_group = NULL;
    }
    else {
#if ORAGE_DEBUG
        g_debug (P_N "Unable to open RC file (%s). Creating it. (%s)",
                 fpath, error->message);
#endif
        
        g_clear_error(&error);
        if (g_file_set_contents(fpath, "#Created by Orage", -1
                    , &error)) { /* successfully created new file */
            orc = g_new(OrageRc, 1);
            orc->rc = grc;
            orc->read_only = read_only;
            orc->file_name = g_strdup(fpath);
            orc->cur_group = NULL;
        }
        else {
#if ORAGE_DEBUG
            g_debug (P_N "Unable to open (create) RC file (%s). (%s)",
                     fpath, error->message);
#endif
            g_key_file_free(grc);
        }
    }

    return(orc);
}

void orage_rc_file_close(OrageRc *orc)
    /* FIXME: check if file contents have been changed and only write when
       needed or build separate save function */
{
#undef P_N
#define P_N "orage_rc_file_close: "
    GError *error = NULL;
    gchar *file_content = NULL;
    gsize length;

    if (orc) {
        /* xfce_rc_close((XfceRc *)orc->rc); */
        if (!orc->read_only) { /* need to write it */
            file_content = g_key_file_to_data(orc->rc, &length, NULL);
            if (file_content 
            && !g_file_set_contents(orc->file_name, file_content, -1
                , &error)) { /* write needed and failed */
                g_warning (P_N "File save failed. RC file (%s). (%s)",
                           orc->file_name, error->message);
            }
            g_free(file_content);
        }
        g_key_file_free(orc->rc);
        g_free(orc->file_name);
        g_free(orc->cur_group);
        g_free(orc);
    }
    else {
        g_debug (P_N "closing empty file.");
    }
}

gchar **orage_rc_get_groups(OrageRc *orc)
{
    return(g_key_file_get_groups(orc->rc, NULL));
}

void orage_rc_set_group(OrageRc *orc, const gchar *grp)
{
    g_free(orc->cur_group);
    orc->cur_group = g_strdup(grp);
}

void orage_rc_del_group(OrageRc *orc, const gchar *grp)
{
#undef P_N
#define P_N "orage_rc_del_group: "

    GError *error = NULL;

    if (!g_key_file_remove_group(orc->rc, grp, &error))
    {
#if ORAGE_DEBUG
        g_debug (P_N "Group remove failed. RC file (%s). group (%s) (%s)",
                 orc->file_name, grp, error->message);
#endif
    }
}

gchar *orage_rc_get_group(OrageRc *orc)
{
    return(g_strdup(orc->cur_group));
}

gchar *orage_rc_get_str(OrageRc *orc, const gchar *key, const gchar *def)
{
    GError *error = NULL;
    gchar *ret;

    ret = g_key_file_get_string (orc->rc, orc->cur_group, key, &error);
    if (!ret && error)
    {
        ret = g_strdup (def);
#if ORAGE_DEBUG
        g_debug ("orage_rc_get_str: "
                 "str (%s) group (%s) in RC file (%s) not found, "
                 "using default (%s)",
                 key, orc->cur_group, orc->file_name, ret);
#endif
    }
    
    return(ret);
}

gint orage_rc_get_int (OrageRc *orc, const gchar *key, const gint def)
{
    GError *error = NULL;
    gint ret;

    ret = g_key_file_get_integer (orc->rc, orc->cur_group, key, &error);
    if (!ret && error)
    {
        ret = def;
#if ORAGE_DEBUG
        g_debug ("orage_rc_get_int: "
                 "str (%s) group (%s) in RC file (%s) not found, "
                 "using default (%d)",
                 key, orc->cur_group, orc->file_name, ret);
#endif
    }
    
    return(ret);
}

gboolean orage_rc_get_bool (OrageRc *orc, const gchar *key, const gboolean def)
{
    GError *error = NULL;
    gboolean ret;

    ret = g_key_file_get_boolean (orc->rc, orc->cur_group, key, &error);
    if (!ret && error)
    {
        ret = def;
#if ORAGE_DEBUG
        g_debug ("orage_rc_get_bool: "
                 "str (%s) group (%s) in RC file (%s) not found, "
                 "using default (%d)",
                 key, orc->cur_group, orc->file_name, ret);
#endif
    }
    
    return(ret);
}

void orage_rc_put_str (OrageRc *orc, const gchar *key, const gchar *val)
{
    if (ORAGE_STR_EXISTS(val))
        g_key_file_set_string (orc->rc, orc->cur_group, key, val);
}

void orage_rc_put_int (OrageRc *orc, const gchar *key, const gint val)
{
    g_key_file_set_integer (orc->rc, orc->cur_group ,key, val);
}

void orage_rc_put_bool (OrageRc *orc, const gchar *key, const gboolean val)
{
    g_key_file_set_boolean (orc->rc, orc->cur_group, key, val);
}

gboolean orage_rc_exists_item (OrageRc *orc, const gchar *key)
{
    return g_key_file_has_key (orc->rc, orc->cur_group, key, NULL);
}

void orage_rc_del_item (OrageRc *orc, const gchar *key)
{
    g_key_file_remove_key (orc->rc, orc->cur_group, key, NULL);
}

/*******************************************************
 * dialog functions
 *******************************************************/

gint orage_info_dialog (GtkWindow *parent
        , const gchar *primary_text, const gchar *secondary_text)
{
    GtkWidget *dialog;
    gint result;

    dialog = gtk_message_dialog_new(parent
            , GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT
            , GTK_MESSAGE_INFO
            , GTK_BUTTONS_OK
            , "%s", primary_text);
    if (secondary_text) 
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog)
                , "%s", secondary_text);
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return(result);
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

gint orage_error_dialog (GtkWindow *parent, const gchar *primary_text,
                                            const gchar *secondary_text)
{
    GtkWidget *dialog;
    gint result;

    dialog = gtk_message_dialog_new(parent
            , GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT
            , GTK_MESSAGE_ERROR
            , GTK_BUTTONS_OK
            , "%s", primary_text);
    if (secondary_text) 
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog)
                , "%s", secondary_text);
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return(result);
}

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