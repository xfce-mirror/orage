/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2005-2011 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2004-2005 Mickael Graf (korbinus at xfce.org)
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

#ifndef __ICAL_CODE_H__
#define __ICAL_CODE_H__

#define XFICAL_UID_LEN 200

typedef enum
{
    XFICAL_FREQ_NONE = 0
   ,XFICAL_FREQ_DAILY
   ,XFICAL_FREQ_WEEKLY
   ,XFICAL_FREQ_MONTHLY
   ,XFICAL_FREQ_YEARLY
   ,XFICAL_FREQ_HOURLY
} xfical_freq;

typedef enum
{
    XFICAL_TYPE_EVENT = 0
   ,XFICAL_TYPE_TODO
   ,XFICAL_TYPE_JOURNAL
} xfical_type;

typedef enum
{
    EXDATE,
    RDATE
} xfical_exception_type;

typedef struct _xfical_exception
{
    GDateTime *time;
    xfical_exception_type type;
    gboolean all_day;
} xfical_exception;

typedef struct _xfical_appt
{
    xfical_type type;
    /* note that version 4.5.9 changed uid format.
     * new format starts with 3 char source id (plus separator '.'), 
     * which tells the file where the id is found:
     * "O00." = Orage file (normal file)
     * "A00." = Archive file
     * "F10." = Foreign file number 10
     * "H08." = Holiday file number 08
     */
    gchar *uid; 
    gchar *title;
    gchar *location;

    gboolean allDay;
    gboolean readonly;

    GDateTime *starttime;
    gchar *start_tz_loc;
    gboolean use_due_time;  /* VTODO has due date or not */

    GDateTime *endtime;
    gchar *end_tz_loc;
    gboolean use_duration;
    gint   duration;
    gboolean completed;

    GDateTime *completedtime;
    gchar *completed_tz_loc;

    gint availability;
    gint priority;
    gchar *categories;
    gchar *note;

        /* alarm */
    gint alarmtime;
    gboolean alarm_before; /* TRUE = before FALSE = after */
        /* TRUE = related to start FALSE= related to end */
    gboolean alarm_related_start; 
    gboolean alarm_persistent;  /* do this alarm even if orage has been down */

    gboolean sound_alarm;
    gchar *sound;
    gboolean soundrepeat;
    gint soundrepeat_cnt;
    gint soundrepeat_len;

    gboolean display_alarm_orage;
    gboolean display_alarm_notify;
        /* used only with libnotify. -1 = no timeout 0 = use default timeout */
    gint display_notify_timeout;  

#if 0
    gboolean email_alarm;
    gchar *email_attendees;
#endif

    gboolean procedure_alarm;
    gchar *procedure_cmd;
    gchar *procedure_params;

    /* For repeating events cur times show current repeating event. Normal times
     * are always the real (=first) start and end times.
     */
    GDateTime *starttimecur;
    GDateTime *endtimecur;
    xfical_freq freq;
    gint   recur_limit; /* 0 = no limit  1 = count  2 = until */
    gint   recur_count;

    GDateTime *recur_until;
    gboolean recur_byday[7]; /* 0=Mo, 1=Tu, 2=We, 3=Th, 4=Fr, 5=Sa, 6=Su */
    gint   recur_byday_cnt[7]; /* monthly/early: 1=first -1=last 2=second... */
    gint   interval;    /* 1=every day/week..., 2=every second day/week,... */
    gboolean recur_todo_base_start; /* TRUE=start time, FALSE=completed time */
    GList  *recur_exceptions; /* EXDATE and RDATE list xfical_exception */
} xfical_appt;

gboolean xfical_set_local_timezone(gboolean testing);

gboolean xfical_file_open(gboolean foreign);
void xfical_file_close(gboolean foreign);
void xfical_file_close_force(void);

xfical_appt *xfical_appt_alloc();
char *xfical_appt_add(char *ical_file_id, xfical_appt *appt);
xfical_appt *xfical_appt_get(const gchar *ical_id);
void xfical_appt_free(xfical_appt *appt);
gboolean xfical_appt_mod(char *ical_id, xfical_appt *appt);
gboolean xfical_appt_del(char *ical_id);

/** Read next EVENT/TODO/JOURNAL component on the specified date from ical
 *  datafile.
 *  @param gdt start date of ical component which is to be read
 *  @param first get first appointment is TRUE, if not get next
 *  @param days how many more days to check forward. 0 = only one day
 *  @param type EVENT/TODO/JOURNAL to be read
 *  @returns NULL if failed and xfical_appt pointer to xfical_appt struct filled
 *           with data if successfull. You need to deallocate it after used. It
 *           will be overdriven by next invocation of this function.
 *  @note starttimecur and endtimecur are converted to local timezone
 */
xfical_appt *xfical_appt_get_next_on_day (GDateTime *gdt, gboolean first,
                                          gint days, xfical_type type,
                                          gchar *file_type);

xfical_appt *xfical_appt_get_next_with_string(char *str, gboolean first
        , gchar *file_type);
void xfical_get_each_app_within_time (GDateTime *a_day, int days
        , xfical_type type, const gchar *file_type , GList **data);

void xfical_mark_calendar(GtkCalendar *gtkcal);
void xfical_mark_calendar_recur(GtkCalendar *gtkcal, xfical_appt *appt);

void xfical_alarm_build_list(gboolean first_list_today);

int xfical_compare_times(xfical_appt *appt);
#ifdef HAVE_ARCHIVE
gboolean xfical_archive_open(void);
void xfical_archive_close(void);
gboolean xfical_archive(void);
gboolean xfical_unarchive(void);
gboolean xfical_unarchive_uid(char *uid);
#endif

gboolean xfical_import_file(const gchar *file_name);
gboolean xfical_export_file(char *file_name, int type, char *uids);

gboolean xfical_file_check(gchar *file_name);

#endif /* !__ICAL_CODE_H__ */
