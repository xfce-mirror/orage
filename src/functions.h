/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2006-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2005-2006 Mickael Graf (korbinus at xfce.org)
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

#ifndef ORAGE_FUNCTIONS_H
#define ORAGE_FUNCTIONS_H

#include <glib.h>
#include <gtk/gtk.h>

#define CALENDAR_RUNNING "_XFCE_CALENDAR_RUNNING"
#define CALENDAR_TOGGLE_EVENT "_XFCE_CALENDAR_TOGGLE_HERE"

#define ORAGE_DIR "orage" G_DIR_SEPARATOR_S
#define ORAGE_PAR_FILE  "oragerc"
#define ORAGE_PAR_DIR_FILE ORAGE_DIR ORAGE_PAR_FILE
#define ORAGE_APP_FILE  "orage.ics"
#define ORAGE_APP_DIR_FILE ORAGE_DIR ORAGE_APP_FILE
#define ORAGE_ARC_FILE  "orage_archive.ics"
#define ORAGE_ARC_DIR_FILE ORAGE_DIR ORAGE_ARC_FILE
#define ORAGE_CATEGORIES_FILE "orage_categories.txt"
#define ORAGE_CATEGORIES_DIR_FILE ORAGE_DIR ORAGE_CATEGORIES_FILE
#define ORAGE_PERSISTENT_ALARMS_FILE "orage_persistent_alarms.txt"
#define ORAGE_PERSISTENT_ALARMS_DIR_FILE ORAGE_DIR ORAGE_PERSISTENT_ALARMS_FILE
#define ORAGE_DEFAULT_ALARM_FILE "orage_default_alarm.txt"
#define ORAGE_DEFAULT_ALARM_DIR_FILE ORAGE_DIR ORAGE_DEFAULT_ALARM_FILE

#define ORAGE_STR_EXISTS(str) ((str != NULL) && (str[0] != 0))

#define DATE_KEY "button-date"

typedef struct _OrageRc
{
    GKeyFile *rc;
    gboolean read_only;
    gchar *file_name;
    gchar *cur_group;
} OrageRc;

#if 0
void program_log (const char *format, ...);
#endif

GtkWidget *orage_create_combo_box_with_content(const gchar *text[], int size);
gboolean orage_date_button_clicked (GtkWidget *button, GtkWidget *win);
gboolean orage_exec(const gchar *cmd, gboolean *cmd_active, GError **error);

GtkWidget *orage_toolbar_append_button (GtkWidget *toolbar,
                                        const gchar *icon_name,
                                        const gchar *tooltip_text, gint pos);
GtkWidget *orage_toolbar_append_separator(GtkWidget *toolbar, gint pos);

GtkWidget *orage_table_new (guint border);
void orage_table_add_row (GtkWidget *table, GtkWidget *label,
                          GtkWidget *input, guint row,
                          GtkAttachOptions input_x_option,
                          GtkAttachOptions input_y_option);

GtkWidget *orage_menu_new(const gchar *menu_header_title, GtkWidget *menu_bar);
GtkWidget *orage_image_menu_item_new_from_stock(const gchar *stock_id
        , GtkWidget *menu, GtkAccelGroup *ag);
GtkWidget *orage_separator_menu_item_new(GtkWidget *menu);
GtkWidget *orage_menu_item_new_with_mnemonic(const gchar *label
        , GtkWidget *menu);

char *orage_replace_text(char *text, char *old, char *new);
char *orage_limit_text(char *text, int max_line_len, int max_lines);
gchar *orage_process_text_commands (const gchar *text);

GtkWidget *orage_period_hbox_new(gboolean head_space, gboolean tail_space
        , GtkWidget *spin_dd, GtkWidget *dd_label
        , GtkWidget *spin_hh, GtkWidget *hh_label
        , GtkWidget *spin_mm, GtkWidget *mm_label);

void orage_gdatetime_unref (GDateTime *gdt);
gchar *orage_gdatetime_to_i18_time (GDateTime *gdt, gboolean date_only);

/** Create ical time string. Unlike g_date_time_format this function add padding
 *  with '0' to year value. Padding is required by Ical format.
 *  @note This function should not used for Orage internal date/time, use
 *        GDateTime instead. It is intended only for exporting GDateTime.
 *  @param gdt GDateTime
 *  @param date_only, when true returns date string in format yyyymmdd, if false
 *         return date and time in format yyyymmddThhmmss.
 *  @return date or time sting. User must free returned string.
 */
gchar *orage_gdatetime_to_icaltime (GDateTime *gdt, gboolean date_only);

/** Comapare dates only.
 *  @param gdt1 GDateTime fist date and time
 *  @param gdt2 GDateTime second date and time
 *  @return 0 if dates are equal, -1 if gdt1 is less than gdt2, 1 if gdt1 is
 *  larger than gdt2.
 */
gint orage_gdatetime_compare_date (GDateTime *gdt1, GDateTime *gdt2);

/** Convert icaltime to GDateTime.
 *  @note This function should not used for Orage internal date/time, use
 *        GDateTime instead. It is intended only for importing Ical date/time.
 *  @param i18_date date and time in icaltime format.
 *  @return GDateTime or NULL if input string does not contain date/time
 *          information
 */
GDateTime *orage_icaltime_to_gdatetime (const gchar *i18_date);

/** Return GDateTime from GTK calendar. This function set hour and minute values
 *  as well.
 *  @param cal instance of GTK calendar
 *  @param hh hour value
 *  @param mm minute value
 *  @return GDateTime, user should release return by g_date_time_unref.
 */
GDateTime *orage_cal_to_gdatetime (GtkCalendar *cal, gint hh, gint mm);

/** Find number of days between two time values.
 *  @param gdt1 first time value
 *  @param gdt2 second time value
 *  @return Number of days between two time value. This value can be negative if
 *          gdt2 < gdt1
 */
gint orage_gdatetime_days_between (GDateTime *gdt1, GDateTime *gdt2);

/** Set (GTK) calendar to selected date.
 *  @param cal instance of GTK calendar
 *  @param gdt selected date.
 */
void orage_select_date (GtkCalendar *cal, GDateTime *gdt);
void orage_select_today(GtkCalendar *cal);

gboolean orage_copy_file (const gchar *source, const gchar *target);
gchar *orage_data_file_location(const gchar *dir_name);
gchar *orage_config_file_location (const gchar *dir_name);

OrageRc *orage_rc_file_open(const gchar *fpath, gboolean read_only);
void orage_rc_file_close(OrageRc *orc);
gchar **orage_rc_get_groups(OrageRc *orc);
void orage_rc_set_group(OrageRc *orc, const gchar *grp);
void orage_rc_del_group(OrageRc *orc, const gchar *grp);
gchar *orage_rc_get_group(OrageRc *orc);
gchar *orage_rc_get_str(OrageRc *orc, const gchar *key, const gchar *def);
gint   orage_rc_get_int(OrageRc *orc, const gchar *key, gint def);
gboolean orage_rc_get_bool(OrageRc *orc, const gchar *key, gboolean def);
GDateTime *orage_rc_get_gdatetime (OrageRc *orc, const gchar *key, GDateTime *def);
void orage_rc_put_str(OrageRc *orc, const gchar *key, const gchar *val);
void orage_rc_put_int(OrageRc *orc, const gchar *key, gint val);
void orage_rc_put_bool(OrageRc *orc, const gchar *key, gboolean val);
void orage_rc_put_gdatetime (OrageRc *orc, const gchar *key, GDateTime *gdt);
gboolean orage_rc_exists_item(OrageRc *orc, const gchar *key);
void orage_rc_del_item(OrageRc *orc, const gchar *key);

/** Read RGBA colour from configuration file.
 *  described in orage rc, then if colour is not
 *  @param orc Orage RC file refernce
 *  @param key key for colour
 *  @param rgba output colour.
 *  @param def default colour. This colour is used only when colour is not
 *         present or not pareseable in given rc file.
 *  @retrun true when output rgba is updated, false if output not updated.
 */
gboolean orage_rc_read_color (OrageRc *orc, const gchar *key,
                              GdkRGBA *rgba, const gchar *def);

gint orage_info_dialog (GtkWindow *parent, const gchar *primary_text,
                                           const gchar *secondary_text);

gint orage_warning_dialog (GtkWindow *parent, const gchar *primary_text,
                                              const gchar *secondary_text,
                                              const gchar *no_text,
                                              const gchar *yes_text);

gint orage_error_dialog (GtkWindow *parent, const gchar *primary_text,
                                            const gchar *secondary_text);

GtkWidget *orage_create_framebox_with_content (const gchar *title,
                                               GtkShadowType shadow_type,
                                               GtkWidget *content);

/* NOTE: the following is in main.c */
void orage_toggle_visible(void);

/* This is wrapper for deprecated 'gtk_status_icon_is_embedded', it is used only
 * for suppress deprecated warning message.
 */
gboolean orage_status_icon_is_embedded (GtkStatusIcon *status_icon);

/* This is wrapper for deprecated 'gtk_status_icon_set_visible', it is used only
 * for suppress deprecated warning message.
 */
void orage_status_icon_set_visible (GtkStatusIcon *status_icon,
                                    gboolean visible);

/* This is wrapper for deprecated 'gtk_status_icon_new_from_pixbuf', it is used
 * only for suppress deprecated warning message.
 */
GtkStatusIcon *orage_status_icon_new_from_pixbuf (GdkPixbuf *pixbuf);

/* This is wrapper for deprecated 'gtk_status_icon_set_tooltip_markup', it is
 * used only for suppress deprecated warning message.
 */
void orage_status_icon_set_tooltip_markup (GtkStatusIcon *status_icon,
                                           const gchar *markup);

/** Create button with image. This code is taken from Mousepad.
 *
 *  @note This function is direct replacement for gtk_button_new_from_stock.
 *        I.e. 'gtk_button_new_from_stock("gtk-open")' can be changed to
 *        'orage_util_image_button ("document-open", _("_Open"))'.
 */
GtkWidget *orage_util_image_button (const gchar *icon_name, const gchar *label);

/** This function take input string and try to convert to GdkRGBA, if conversion
 *  failed, then default value will be converted.
 *  @param color_str colour string to convert. This string may be in GDK RBGA
 *         format or in legacy Orage format.
 *  @param rgba converted output
 *  @param def default value when colour_str can not be parsed. Only GDK RBGA
 *         format is supported.
 *  @return true when rgba was successfully updated, false color_str or def was
 *         invalid
 */
gboolean orage_str_to_rgba (const gchar *color_str,
                             GdkRGBA *rgba,
                             const gchar *def);

/** Tests if debug messages are printed out.
 *  @return true when debug messages is printed out, false if not.
 */
gboolean orage_is_debug_logging_enabled (void);


#endif /* !__ORAGE_FUNCTIONS_H__ */
