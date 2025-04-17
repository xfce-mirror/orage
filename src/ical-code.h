/*
 * Copyright (c) 2021-2025 Erkki Moorits
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
 *     Free Software Foundation
 *     51 Franklin Street, 5th Floor
 *     Boston, MA 02110-1301 USA
 */

#ifndef __ICAL_CODE_H__
#define __ICAL_CODE_H__

#include "ical-expimp.h"
#include <gio/gio.h>
#include <gtk/gtk.h>

#define LIBICAL_GLIB_UNSTABLE_API
#include <libical-glib/libical-glib.h>

#define MAX_MONTH_RECURRENCE_DAY 28

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
    XFICAL_RECUR_NO_LIMIT = 0,
    XFICAL_RECUR_COUNT,
    XFICAL_RECUR_UNTIL
} xfical_recur_limit;

typedef enum
{
    XFICAL_RECUR_MONTH_TYPE_BEGIN = 0,
    XFICAL_RECUR_MONTH_TYPE_END,
    XFICAL_RECUR_MONTH_TYPE_EVERY
} xfical_recur_month_type;

typedef enum
{
    XFICAL_RECUR_MONTH_DAY_MON = 0,
    XFICAL_RECUR_MONTH_DAY_TUE,
    XFICAL_RECUR_MONTH_DAY_WED,
    XFICAL_RECUR_MONTH_DAY_THU,
    XFICAL_RECUR_MONTH_DAY_FRI,
    XFICAL_RECUR_MONTH_DAY_SAT,
    XFICAL_RECUR_MONTH_DAY_SUN
} xfical_recur_day_sel;

typedef enum
{
    XFICAL_RECUR_MONTH_WEEK_FIRST = 0,
    XFICAL_RECUR_MONTH_WEEK_SECOND,
    XFICAL_RECUR_MONTH_WEEK_THIRD,
    XFICAL_RECUR_MONTH_WEEK_FOURTH,
    XFICAL_RECUR_MONTH_WEEK_LAST
} xfical_recur_week_sel;

typedef enum
{
    XFICAL_RECUR_MONTH_JAN = 0,
    XFICAL_RECUR_MONTH_FEB,
    XFICAL_RECUR_MONTH_MAR,
    XFICAL_RECUR_MONTH_APR,
    XFICAL_RECUR_MONTH_MAY,
    XFICAL_RECUR_MONTH_JUN,
    XFICAL_RECUR_MONTH_JUL,
    XFICAL_RECUR_MONTH_AUG,
    XFICAL_RECUR_MONTH_SEP,
    XFICAL_RECUR_MONTH_OCT,
    XFICAL_RECUR_MONTH_NOV,
    XFICAL_RECUR_MONTH_DEC
} xfical_recur_month_sel;

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
    xfical_recur_limit recur_limit;
    gint recur_count;
    xfical_recur_month_type recur_month_type;
    guint recur_month_days;
    xfical_recur_week_sel recur_week_sel;
    xfical_recur_day_sel recur_day_sel;
    xfical_recur_month_sel recur_month_sel;

    GDateTime *recur_until;
    gboolean recur_byday[7]; /* 0=Mo, 1=Tu, 2=We, 3=Th, 4=Fr, 5=Sa, 6=Su */
    gint   interval;    /* 1=every day/week..., 2=every second day/week,... */
    gboolean recur_todo_base_start; /* TRUE=start time, FALSE=completed time */
    GList  *recur_exceptions; /* EXDATE and RDATE list xfical_exception */
} xfical_appt;

#define ORAGE_CALENDAR_COMPONENT_TYPE (orage_calendar_component_get_type ())
G_DECLARE_FINAL_TYPE (OrageCalendarComponent, orage_calendar_component, ORAGE, CALENDAR_COMPONENT, GObject)

/** Constructs new Orage calendar component from ICAL component.
 *  @param icalcomp ICAL component
 *  @return OrageCalendarComponent or NULL for faliure
 */
OrageCalendarComponent *o_cal_component_new_from_icalcomponent (
        ICalComponent *icalcomp);

/** Return name of the event from caledar component.
 *  @param ocal_comp calendar component
 *  @return event name
 */
const gchar *o_cal_component_get_summary (OrageCalendarComponent *ocal_comp);

/** Return location of the event from calendar component.
 *  @param ocal_comp calendar component
 *  @return location
 */
const gchar *o_cal_component_get_location (OrageCalendarComponent *ocal_comp);

/** Return event type.
 *  @param ocal_comp calendar component
 *  @return event type, or -1 casted to xfical_type
 */
xfical_type o_cal_component_get_type (OrageCalendarComponent *ocal_comp);

/** Test if event is all day event.
 *  @param ocal_comp calendar component
 *  @return true for all day event
 */
gboolean o_cal_component_is_all_day_event (OrageCalendarComponent *ocal_comp);

/** Return event start time.
 *  @param ocal_comp calendar component
 *  @return event start time
 */
GDateTime *o_cal_component_get_dtstart (OrageCalendarComponent *ocal_comp);

/** Return event end time.
 *  @param ocal_comp calendar component
 *  @return event end time
 */
GDateTime *o_cal_component_get_dtend (OrageCalendarComponent *ocal_comp);

/** Tests if event is recurring.
 *  @param ocal_comp calendar component
 *  @return TRUE if event is recurring
 */
gboolean o_cal_component_is_recurring (OrageCalendarComponent *ocal_comp);

/** Return event URL.
 *  @param ocal_comp calendar component
 *  @return evnet url
 */
const gchar *o_cal_component_get_url (OrageCalendarComponent *ocal_comp);

gboolean xfical_set_local_timezone(gboolean testing);

gboolean xfical_file_open(gboolean foreign);
void xfical_file_close(gboolean foreign);
void xfical_file_close_force(void);

/** Read appointments from ICS file.
 *  @param file input file
 *  @return NULL if failed or no data read, non NULL if data successfully read
 *  from file
 */
GList *xfical_appt_new_from_file (GFile *file);

/** Allocates memory and initializes for given data it for new ical_type
 *  structure.
 *  @param gdt date for initialization
 *  @return NULL if failed and pointer to xfical_appt if successfull. You must
 *          free it after not being used anymore. (g_free())
 */
xfical_appt *xfical_appt_new_day (GDateTime *gdt);

/** Add EVENT/TODO/JOURNAL type ical appointment to ical file
 *  @param appt pointer to filled xfical_appt structure, which is to be stored.
 *              Caller is responsible for filling and allocating and freeing it.
 *  @returns NULL if failed and new ical id if successfully added. This ical id
 *           is owned by the routine. Do not deallocate it. It will be
 *           overwrittewritten by next invocation of this function.
 */
gchar *xfical_appt_add (gchar *ical_file_id, xfical_appt *appt);

/** Read EVENT from ical datafile.
 *  @param ical_uid key of ical EVENT appt-> is to be read
 *  @return if failed: NULL
 *          if successfull: xfical_appt pointer to xfical_appt struct filled
 *                          with data. You must free it after not being used
 *                          anymore using xfical_appt_free.
 *
 *  @note This routine does not fill starttimecur nor endtimecur, those are
 *  always initialized to null string
 */
xfical_appt *xfical_appt_get(const gchar *ical_id);
void xfical_appt_free(xfical_appt *appt);

 /** Modify EVENT type ical appointment in ical file.
  *  @param ical_uid char pointer to ical ical_uid to modify
  *  @param app pointer to filled xfical_appt structure, which is stored. Caller
  *         is responsible for filling and allocating and freeing it.
  *  @return TRUE is successfull, FALSE if failed
  */
gboolean xfical_appt_mod (gchar *ical_id, xfical_appt *appt);

/** Removes EVENT with ical_uid from ical file
 *  @param ical_uid pointer to ical_uid to be deleted
 *  @returns: TRUE is successfull, FALSE if failed
 */
gboolean xfical_appt_del (gchar *ical_id);

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

/** Read next EVENT/TODO/JOURNAL which contains the specified string from ical
 *  datafile. You must deallocate the appt after the call
 *  @param str string to search
  * @param first get first appointment is TRUE, if not get next
 *  @return NULL if failed, xfical_appt pointer to xfical_appt struct filled
 *          with data if successfull
  */
xfical_appt *xfical_appt_get_next_with_string (const gchar *str, gboolean first,
                                               const gchar *file_type);

void xfical_get_each_app_within_time (GDateTime *a_day, gint days
        , xfical_type type, const gchar *file_type , GList **data);

void xfical_mark_calendar(GtkCalendar *gtkcal);
void xfical_mark_calendar_recur(GtkCalendar *gtkcal, const xfical_appt *appt);

void xfical_alarm_build_list(gboolean first_list_today);

gint xfical_compare_times (xfical_appt *appt);
#ifdef HAVE_ARCHIVE
gboolean xfical_archive_open(void);
void xfical_archive_close(void);
gboolean xfical_archive(void);
gboolean xfical_unarchive(void);
gboolean xfical_unarchive_uid (const gchar *uid);
#endif

gboolean xfical_file_check (const gchar *file_name);

#endif /* !__ICAL_CODE_H__ */
