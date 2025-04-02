/*
 * Copyright (c) 2021-2025 Erkki Moorits
 * Copyright (c) 2005-2011 Juha Kautto  (juha at xfce.org)
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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <unistd.h>
#include <time.h>
#include <math.h>

#include <glib.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include <libical/ical.h>
#include <libical/icalss.h>

#define LIBICAL_GLIB_UNSTABLE_API
#include <libical-glib/libical-glib.h>

#include "orage-alarm-structure.h"
#include "orage-i18n.h"
#include "orage-window.h"
#include "functions.h"
#include "reminder.h"
#include "ical-code.h"
#include "ical-internal.h"
#include "event-list.h"
#include "orage-appointment-window.h"
#include "parameters.h"
#include "interface.h"
#include "xfical_exception.h"

#define XFICAL_UID_LEN 200

struct _OrageCalendarComponent
{
    GObject __parent__;

    /* The icalcomponent we wrap */
    ICalComponent *icalcomp;

    /* Whether we should increment the sequence number when piping the
     * object over the wire.
     */
    guint need_sequence_inc : 1;
};

G_DEFINE_TYPE (OrageCalendarComponent, orage_calendar_component, G_TYPE_OBJECT)

icalset *ic_fical = NULL;
icalcomponent *ic_ical = NULL;
#ifdef HAVE_ARCHIVE
icalset *ic_afical = NULL;
icalcomponent *ic_aical = NULL;
#endif
gboolean ic_file_modified = FALSE; /* has any ical file been changed */
ic_foreign_ical_files ic_f_ical[10];

static void xfical_alarm_build_list_internal(gboolean first_list_today);

typedef struct _xfical_timezone_array
{
    int    count;     /* how many timezones we have */
    char **city;      /* pointer to timezone location name strings */
    int  *utc_offset; /* pointer to int array holding utc offsets */
    int  *dst;        /* pointer to int array holding dst settings */
} xfical_timezone_array;

typedef struct _app_data
{
    GList **list;
    const gchar *file_type;
    /* Need to check that returned value is withing limits. Check more from
     * BUG 5764 and 7886.
     */
    GDateTime *asdate;
    GDateTime *aedate;
    gint orig_start_hour, orig_end_hour;
} app_data;

static guint    file_close_timer = 0;  /* delayed file close timer */

typedef struct _excluded_time
{
    struct icaltimetype e_time;
} excluded_time;

typedef struct _mark_calendar_data
{
    GtkCalendar *cal;
    guint year;
    guint month;
    gint orig_start_hour, orig_end_hour;
    xfical_appt appt;
} mark_calendar_data;

/* timezone handling */
static icaltimezone *utc_icaltimezone = NULL;
static icaltimezone *local_icaltimezone = NULL;

static guint orage_recurrence_type_to_day_of_week (short by_day)
{
    const icalrecurrencetype_weekday wd =
        icalrecurrencetype_day_day_of_week (by_day);

    switch (wd)
    {
        case ICAL_SUNDAY_WEEKDAY:
            return 6;

        case ICAL_MONDAY_WEEKDAY:
        case ICAL_TUESDAY_WEEKDAY:
        case ICAL_WEDNESDAY_WEEKDAY:
        case ICAL_THURSDAY_WEEKDAY:
        case ICAL_FRIDAY_WEEKDAY:
        case ICAL_SATURDAY_WEEKDAY:
            return wd - ICAL_MONDAY_WEEKDAY;

        case ICAL_NO_WEEKDAY:
        default:
            g_warning ("%s: invalid by_day value from RRULE", G_STRFUNC);
            return 0;
    }
}

static struct icaltimetype icaltimetype_from_gdatetime (GDateTime *gdt,
                                                        const gboolean date_only)
{
    gchar *str;
    struct icaltimetype icalt;

    /* TODO: icaltimetype should be created directly from GDateTime, not using
     * intermediate string.
     */

    str = orage_gdatetime_to_icaltime (gdt, date_only);
    icalt = icaltime_from_string (str);
    g_free (str);

    return icalt;
}

static GDateTime *icaltimetype_to_gdatetime (struct icaltimetype t)
{
    const gchar *i18_date = icaltime_as_ical_string (t);

    return orage_icaltime_to_gdatetime (i18_date);
}

static gchar *icaltime_to_i18_time (const char *icaltime)
{
    /* timezone is not converted */
    GDateTime *gdt;
    gchar *ct;
    gboolean date_only;

    gdt = orage_icaltime_to_gdatetime (icaltime);
    date_only = (strchr (icaltime, 'T') == NULL);
    ct = orage_gdatetime_to_i18_time (gdt, date_only);
    g_date_time_unref (gdt);

    return(ct);
}

gboolean xfical_set_local_timezone(gboolean testing)
{
    local_icaltimezone = NULL;
    g_par.local_timezone_utc = FALSE;
    if (!utc_icaltimezone)
            utc_icaltimezone = icaltimezone_get_utc_timezone();

    if (!ORAGE_STR_EXISTS(g_par.local_timezone)) {
        g_warning ("%s: empty timezone", G_STRFUNC);
        g_par.local_timezone = g_strdup("floating");
    }

    if (strcmp(g_par.local_timezone,"UTC") == 0) {
        g_par.local_timezone_utc = TRUE;
        local_icaltimezone = utc_icaltimezone;
    }
    else if (strcmp(g_par.local_timezone,"floating") == 0) {
        g_warning ("Default timezone set to floating. Do not use timezones when setting appointments, it does not make sense without proper local timezone.");
        return(TRUE); /* g_par.local_timezone = NULL */
    }
    else
        local_icaltimezone = 
                icaltimezone_get_builtin_timezone(g_par.local_timezone);

    if (!local_icaltimezone) {
        if (!testing)
            g_warning ("%s: builtin timezone %s not found", G_STRFUNC
                    , g_par.local_timezone);
        return (FALSE);
    }
    return (TRUE); 
}

gboolean ic_internal_file_open(icalcomponent **p_ical
        , icalset **p_fical, const gchar *file_icalpath, gboolean read_only
        , gboolean test)
{
    icalcomponent *iter;
    gint cnt=0;

    if (file_close_timer) { 
        /* We are opening main ical file and delayed close is in progress. 
         * Closing must be cancelled since we are now opening the file. */
        g_source_remove(file_close_timer);
        file_close_timer = 0;
        g_debug ("%s: canceling delayed close", G_STRFUNC);
    }
    if (*p_fical != NULL) {
        return(TRUE);
    }
    if (!ORAGE_STR_EXISTS(file_icalpath)) {
        if (test)
            g_warning ("%s: file empty", G_STRFUNC);
        else
            g_error ("%s: file empty", G_STRFUNC);

        return(FALSE);
    }
    if (read_only)
        *p_fical = icalset_new_file_reader(file_icalpath);
    else 
        *p_fical = icalset_new_file(file_icalpath);
    if (*p_fical == NULL) {
        if (test)
            g_warning ("%s: Could not open ical file (%s) %s", G_STRFUNC
                    , file_icalpath, icalerror_strerror(icalerrno));
        else
            g_critical ("%s: Could not open ical file (%s) %s", G_STRFUNC
                    , file_icalpath, icalerror_strerror(icalerrno));
        return(FALSE);
    }
    else { /* file open, let's find last VCALENDAR entry */
        for (iter = icalset_get_first_component(*p_fical); 
             iter != 0;
             iter = icalset_get_next_component(*p_fical)) {
            cnt++;
            *p_ical = iter; /* last valid component */
        }
        if (cnt == 0) {
            if (test) { /* failed */
                g_warning ("%s: no top level (VCALENDAR) component in "
                           "calendar file %s", G_STRFUNC, file_icalpath);
                return(FALSE);
            }
        /* calendar missing, need to add one.  
         * Note: According to standard rfc2445 calendar always needs to
         *       contain at least one other component. So strictly speaking
         *       this is not valid entry before adding an event or timezone
         */
            *p_ical = icalcomponent_vanew(ICAL_VCALENDAR_COMPONENT
                   , icalproperty_new_version("2.0")
                   , icalproperty_new_prodid("-//Xfce//Orage//EN")
                   , NULL);

            icalset_add_component(*p_fical, *p_ical);
#if 0
            icalset_add_component(*p_fical
                   , icalcomponent_new_clone(*p_ical));

            *p_ical = icalset_get_first_component(*p_fical);
#endif
            icalset_commit(*p_fical);
        }
        else { /* VCALENDAR found */
            if (cnt > 1) {
                g_warning ("%s: too many top level components in calendar "
                           "file %s", G_STRFUNC , file_icalpath);
                if (test) { /* failed */
                    return(FALSE);
                }
            }
            if (test 
            && icalcomponent_isa(*p_ical) != ICAL_VCALENDAR_COMPONENT) {
                g_critical ("%s: top level component is not VCALENDAR %s",
                            G_STRFUNC, file_icalpath);
                return(FALSE);
            }
        }
    }

    ic_file_modified = FALSE;
    return(TRUE);
}

gboolean xfical_file_open(gboolean foreign)
{ 
    gboolean ok;
    gint i;
    struct stat s;

    /* make sure there are no external updates or they will be overwritten */
    if (g_par.latest_file_change)
        orage_external_update_check(NULL);
    ok = ic_internal_file_open(&ic_ical, &ic_fical, g_par.orage_file, FALSE
            , FALSE);
    /* store last access time */
    if (ok)
    {
        if (g_stat(g_par.orage_file, &s) < 0) {
            g_warning ("%s: stat of %s failed: %s", G_STRFUNC,
                       g_par.orage_file, g_strerror (errno));
            g_par.latest_file_change = (time_t)0;
        }
        else {
            g_par.latest_file_change = s.st_mtime;
        }
    }

    if (ok && foreign) /* let's open foreign files */
        for (i = 0; i < g_par.foreign_count; i++) {
            ok = ic_internal_file_open(&(ic_f_ical[i].ical)
                    , &(ic_f_ical[i].fical), g_par.foreign_data[i].file
                    , g_par.foreign_data[i].read_only , FALSE);
            if (!ok) {
                ic_f_ical[i].ical = NULL;
                ic_f_ical[i].fical = NULL;
                g_par.foreign_data[i].latest_file_change = (time_t)0;
            }
            else {
                /* store last access time */
                if (g_stat(g_par.foreign_data[i].file, &s) < 0) {
                    g_warning ("%s, stat of %s failed: %s", G_STRFUNC,
                               g_par.foreign_data[i].file, g_strerror (errno));
                    g_par.foreign_data[i].latest_file_change = (time_t)0;
                }
                else {
                    g_par.foreign_data[i].latest_file_change = s.st_mtime;
                }
            }
        }

    return(ok);
}

gboolean xfical_file_check (const gchar *file_name)
{
    icalcomponent *x_ical = NULL;
    icalset *x_fical = NULL;

    return(ic_internal_file_open(&x_ical, &x_fical, file_name, FALSE, TRUE));
}

static gboolean delayed_file_close (G_GNUC_UNUSED gpointer user_data)
{
    if (ic_fical == NULL)
        return(FALSE); /* closed already, nothing to do */
    icalset_free(ic_fical);
    ic_fical = NULL;
    g_debug ("%s: closing ical file", G_STRFUNC);
    /* we only close file once, so end here */
    file_close_timer = 0;

    return(FALSE); 
}

void xfical_file_close(gboolean foreign)
{
    gint i;
    struct stat s;

    if (ic_fical == NULL)
        g_critical ("%s: ic_fical is NULL", G_STRFUNC);
    else {
        if (file_close_timer) { 
            /* We are closing main ical file and delayed close is in progress. 
             * Closing must be cancelled since we are now closing the file. */
            g_source_remove(file_close_timer);
            file_close_timer = 0;
            g_debug ("%s: canceling delayed close", G_STRFUNC);
        }
        if (ic_file_modified || g_par.file_close_delay == 0) { /* close it now */
            g_debug ("%s: closing file now", G_STRFUNC);
            delayed_file_close(NULL);
            /* store last access time */
            if (g_stat(g_par.orage_file, &s) < 0) {
                g_warning ("%s: stat of %s failed: %s", G_STRFUNC,
                        g_par.orage_file, g_strerror (errno));
                g_par.latest_file_change = (time_t)0;
            }
            else {
                g_par.latest_file_change = s.st_mtime;
            }
        }
        else { /* close it later (to save time) */
            g_debug ("%s: closing file after %d seconds", G_STRFUNC,
                     g_par.file_close_delay);
            file_close_timer = g_timeout_add_seconds(g_par.file_close_delay
                    , (GSourceFunc)delayed_file_close, NULL);
        }
    }
    
    if (foreign) 
        for (i = 0; i < g_par.foreign_count; i++) {
            if (ic_f_ical[i].fical == NULL)
                g_warning ("%s: foreign fical is NULL", G_STRFUNC);
            else {
                icalset_free(ic_f_ical[i].fical);
                ic_f_ical[i].fical = NULL;
                /* store last access time */
                if (g_stat(g_par.foreign_data[i].file, &s) < 0) {
                    g_warning ("%s: stat of %s failed: %s", G_STRFUNC,
                            g_par.foreign_data[i].file, g_strerror (errno));
                    g_par.foreign_data[i].latest_file_change = (time_t)0;
                }
                else {
                    g_par.foreign_data[i].latest_file_change = s.st_mtime;
                }
            }
        }
}

void xfical_file_close_force(void)
{
    ic_file_modified = TRUE;
    xfical_file_close(TRUE);
}

char *ic_get_char_timezone(icalproperty *p)
{
    icalparameter *itime_tz;
    gchar *tz_loc = NULL;

    if ((itime_tz = icalproperty_get_first_parameter(p, ICAL_TZID_PARAMETER)))
        tz_loc = (char *)icalparameter_get_tzid(itime_tz);
    return(tz_loc);
}

static icaltimezone *get_builtin_timezone(gchar *tz_loc)
{
     /* This probably is evolution format, 
      * which has /xxx/xxx/timezone and we should remove the 
      * extra /xxx/xxx/ from it */
    char **new_str;
    icaltimezone *l_icaltimezone = NULL;

    if (tz_loc[0] == '/') {
        g_debug ("%s: evolution timezone fix %s", G_STRFUNC, tz_loc);
        new_str = g_strsplit(tz_loc, "/", 4);
        if (new_str[0] != NULL && new_str[1] != NULL && new_str[2] != NULL
        &&  new_str[3] != NULL)
            l_icaltimezone = icaltimezone_get_builtin_timezone(new_str[3]);
        g_strfreev(new_str);
    }
    else
        l_icaltimezone = icaltimezone_get_builtin_timezone(tz_loc);
    return(l_icaltimezone);
}

struct icaltimetype ic_convert_to_timezone(struct icaltimetype t
        , icalproperty *p)
{
    gchar *tz_loc;
    icaltimezone *l_icaltimezone;
    struct icaltimetype tz;

    if ((tz_loc = ic_get_char_timezone(p))) {
        /* FIXME: could we now call convert_to_zone or is it a problem
         * if we always move to zone format ? */
        if (!(l_icaltimezone = get_builtin_timezone(tz_loc))) {
            g_critical ("%s: builtin timezone %s not found, conversion failed.",
                        G_STRFUNC, tz_loc);
        }
        tz = icaltime_convert_to_zone(t, l_icaltimezone);
    }
    else
        tz = t;

    return(tz);
}

static struct icaltimetype convert_to_local_timezone(struct icaltimetype t
            , icalproperty *p)
{
    struct icaltimetype tl;

    tl = ic_convert_to_timezone(t, p);
    tl = icaltime_convert_to_zone(tl, local_icaltimezone);

    return (tl);
}

xfical_period ic_get_period(icalcomponent *c, gboolean local) 
{
    icalproperty *p = NULL, *p2 = NULL;
    xfical_period per;
    struct icaldurationtype duration_tmp;
    gint dur_int;

    /* Exactly one start time must be there */
    p = icalcomponent_get_first_property(c, ICAL_DTSTART_PROPERTY);
    if (p != NULL) {
        per.stime = icalproperty_get_dtstart(p);
        if (local)
            per.stime = convert_to_local_timezone(per.stime, p);
        else
            per.stime = ic_convert_to_timezone(per.stime, p);
    }
    else {
        per.stime = icaltime_null_time();
    } 

    /* Either endtime/duetime or duration may be there. 
     * But neither is required.
     * VTODO may also have completed time but it does not have dtstart always
     */
    per.ikind = icalcomponent_isa(c);
    if (per.ikind == ICAL_VEVENT_COMPONENT)
        p = icalcomponent_get_first_property(c, ICAL_DTEND_PROPERTY);
    else if (per.ikind == ICAL_VTODO_COMPONENT) {
        p = icalcomponent_get_first_property(c, ICAL_DUE_PROPERTY);
        p2 = icalcomponent_get_first_property(c, ICAL_COMPLETED_PROPERTY);
    }
    else if (per.ikind == ICAL_VJOURNAL_COMPONENT 
         ||  per.ikind == ICAL_VTIMEZONE_COMPONENT)
        p = NULL; /* does not exist for journal and timezone */
    else {
        g_warning ("%s: unknown component type (%s)", G_STRFUNC,
                   icalcomponent_get_uid(c));
        p = NULL;
    }
    if (p != NULL) {
        if (per.ikind == ICAL_VEVENT_COMPONENT)
            per.etime = icalproperty_get_dtend(p);
        else if (per.ikind == ICAL_VTODO_COMPONENT)
            per.etime = icalproperty_get_due(p);
        if (local)
            per.etime = convert_to_local_timezone(per.etime, p);
        else
            per.etime = ic_convert_to_timezone(per.etime, p);
        per.duration = icaltime_subtract(per.etime, per.stime);
        if (icaltime_is_date(per.stime) 
            && icaldurationtype_as_int(per.duration) != 0) {
        /* need to subtract 1 day.
         * read explanation from appt_add_internal: appt->endtime processing */
            duration_tmp = icaldurationtype_from_int(60*60*24);
            dur_int = icaldurationtype_as_int(per.duration);
            dur_int -= icaldurationtype_as_int(duration_tmp);
            per.duration = icaldurationtype_from_int(dur_int);
            per.etime = icaltime_add(per.stime, per.duration);
        }

    }
    else {
        p = icalcomponent_get_first_property(c, ICAL_DURATION_PROPERTY);
        if (p != NULL) {
            per.duration = icalproperty_get_duration(p);
            per.etime = icaltime_add(per.stime, per.duration);
        }
        else {
            per.etime = per.stime;
            per.duration = icaldurationtype_null_duration();
        } 
    } 

    if (p2 != NULL) {
        per.ctime = icalproperty_get_completed(p2);
        if (local)
            per.ctime = convert_to_local_timezone(per.ctime, p2);
        else
            per.ctime = ic_convert_to_timezone(per.ctime, p);
    }
    else
        per.ctime = icaltime_null_time();

    return(per);
}

/* basically copied from icaltime_compare, which can't be used
 * because it uses utc
 */
static int local_compare(struct icaltimetype a, struct icaltimetype b)
{
    int retval = 0;

    if (a.year > b.year)
        retval = 1;
    else if (a.year < b.year)
        retval = -1;

    else if (a.month > b.month)
        retval = 1;
    else if (a.month < b.month)
        retval = -1;

    else if (a.day > b.day)
        retval = 1;
    else if (a.day < b.day)
        retval = -1;

    /* if both are dates, we are done */
    if (a.is_date && b.is_date)
        return retval;

    /* else, if we already found a difference, we are done */
    else if (retval != 0)
        return retval;

    /* else, if only one is a date (and we already know the date part is equal),       then the other is greater */
    else if (b.is_date)
        retval = 1;
    else if (a.is_date)
        retval = -1;

    else if (a.hour > b.hour)
        retval = 1;
    else if (a.hour < b.hour)
        retval = -1;

    else if (a.minute > b.minute)
        retval = 1;
    else if (a.minute < b.minute)
        retval = -1;

    else if (a.second > b.second)
        retval = 1;
    else if (a.second < b.second)
        retval = -1;

    return retval;
}

/* basically copied from icaltime_compare_date_only, which can't be used
 * because it uses utc 
 */
static int local_compare_date_only(struct icaltimetype a
        , struct icaltimetype b)
{
    int retval;

    if (a.year > b.year)
        retval = 1;
    else if (a.year < b.year)
        retval = -1;
                                                                                
    else if (a.month > b.month)
        retval = 1;
    else if (a.month < b.month)
        retval = -1;
                                                                                
    else if (a.day > b.day)
        retval = 1;
    else if (a.day < b.day)
        retval = -1;
                                                                                
    else
        retval = 0;
                                                                                
    return retval;
}

static struct icaltimetype convert_to_zone(struct icaltimetype t, gchar *tz)
{
    struct icaltimetype wtime = t;
    icaltimezone *l_icaltimezone = NULL;

    if ORAGE_STR_EXISTS(tz) {
        if (strcmp(tz, "UTC") == 0) {
            wtime = icaltime_convert_to_zone(t, utc_icaltimezone);
        }
        else if (strcmp(tz, "floating") == 0) {
            if (local_icaltimezone)
                wtime = icaltime_convert_to_zone(t, local_icaltimezone);
        }
        else {
            l_icaltimezone = get_builtin_timezone(tz);
            if (!l_icaltimezone)
            {
                g_critical ("%s: builtin timezone %s not found, "
                            "conversion failed", G_STRFUNC, tz);
            }
            else
                wtime = icaltime_convert_to_zone(t, l_icaltimezone);
        }
    }
    else  /* floating time */
        if (local_icaltimezone)
            wtime = icaltime_convert_to_zone(t, local_icaltimezone);

    return(wtime);
}

gint xfical_compare_times (xfical_appt *appt)
{
    struct icaltimetype stime, etime;
    const char *text;
    struct icaldurationtype duration;

    if (appt->starttime == NULL)
        g_error ("%s: null start time", G_STRFUNC);

    if (appt->use_duration) {
        stime = icaltimetype_from_gdatetime (appt->starttime, appt->allDay);
        duration = icaldurationtype_from_int(appt->duration);
        etime = icaltime_add(stime, duration);
        text  = icaltime_as_ical_string(etime);
        orage_gdatetime_unref (appt->endtime);
        appt->endtime = orage_icaltime_to_gdatetime (text);
        g_free(appt->end_tz_loc);
        appt->end_tz_loc = g_strdup(appt->start_tz_loc);
        return(0); /* ok */

    }
    else {
        if (appt->endtime == NULL)
            g_error ("%s: null end time", G_STRFUNC);

        stime = icaltimetype_from_gdatetime (appt->starttime, appt->allDay);
        etime = icaltimetype_from_gdatetime (appt->endtime, appt->allDay);

        stime = convert_to_zone (stime, appt->start_tz_loc);
        stime = icaltime_convert_to_zone (stime, local_icaltimezone);
        etime = convert_to_zone (etime, appt->end_tz_loc);
        etime = icaltime_convert_to_zone (etime, local_icaltimezone);

        duration = icaltime_subtract (etime, stime);
        appt->duration = icaldurationtype_as_int (duration);
        return icaltime_compare (stime, etime);
    }
}

static void xfical_appt_init0_gdt (xfical_appt *appt, GDateTime *gdt)
{
    guint i;
    guint week;
    guint weekday;
    guint month;
    gint month_day;

    memset (appt, 0, sizeof (xfical_appt));

    week = 0; /* TODO: find week of month, locale should be taken acount also. */
    weekday = g_date_time_get_day_of_week (gdt) - 1;
    month = g_date_time_get_month (gdt) - 1;
    month_day = CLAMP (g_date_time_get_day_of_month (gdt),
                       1, MAX_MONTH_RECURRENCE_DAY);

    appt->availability = 1;
    appt->interval = 1;
    appt->starttimecur = gdt;
    appt->recur_week_sel = week;
    appt->recur_day_sel = weekday;
    appt->recur_month_sel = month;
    appt->recur_month_days = month_day;

    for (i = 0; i <= 6; i++)
        appt->recur_byday[i] = (weekday == i);
}

static void xfical_appt_init0 (xfical_appt *appt)
{
    xfical_appt_init0_gdt (appt, g_date_time_new_now_local ());
}

/** Allocates memory and initializes it for new ical_type structure.
 *  @return NULL if failed and pointer to xfical_appt if successfull. You must
 *          free it after not being used anymore. (g_free())
 */
static xfical_appt *xfical_appt_alloc (void)
{
    xfical_appt *appt;

    appt = g_new (xfical_appt, 1);
    xfical_appt_init0 (appt);

    return appt;
}

xfical_appt *xfical_appt_new_day (GDateTime *gdt)
{
    xfical_appt *appt;

    appt = g_new (xfical_appt, 1);

    xfical_appt_init0_gdt (appt, g_date_time_ref (gdt));

    return appt;
}

char *ic_generate_uid(void)
{
    gchar xf_host[XFICAL_UID_LEN / 2 + 1];
    gchar *xf_uid;
    static guint seq = 0;
    struct icaltimetype dtstamp;

    xf_uid = g_new (gchar, XFICAL_UID_LEN);
    dtstamp = icaltime_current_time_with_zone (utc_icaltimezone);
    gethostname (xf_host, XFICAL_UID_LEN / 2);
    xf_host[XFICAL_UID_LEN / 2] = '\0';
    g_snprintf (xf_uid, XFICAL_UID_LEN, "Orage-%s%u-%lu@%s",
                icaltime_as_ical_string (dtstamp), seq, (long)getuid (),
                xf_host);

    if (++seq > 999)
        seq = 0;

    return(xf_uid);
}

/* dispprop   = 3*(
                ; the following are all REQUIRED,
                ; but MUST NOT occur more than once
                action / description / trigger /
***** Not Implemented in Orage *****
                ; 'duration' and 'repeat' are both optional,
                ; and MUST NOT occur more than once each,
                ; but if one occurs, so MUST the other
                duration / repeat /
***** Not Implemented in Orage *****
                ; the following is optional,
                ; and MAY occur more than once
                x-prop
***** Orage implements: *****
X-ORAGE-DISPLAY-ALARM:ORAGE
X-ORAGE-DISPLAY-ALARM:NOTIFY
X-ORAGE-NOTIFY-ALARM-TIMEOUT:%d
                )
*/
static void appt_add_alarm_internal_display(xfical_appt *appt
        , icalcomponent *ialarm)
{
    char text[50];

    icalcomponent_add_property(ialarm
            , icalproperty_new_action(ICAL_ACTION_DISPLAY));
    /* FIXME: add real alarm description and use the below as default only */
    if ORAGE_STR_EXISTS(appt->note)
        icalcomponent_add_property(ialarm
                , icalproperty_new_description(appt->note));
    else if ORAGE_STR_EXISTS(appt->title)
        icalcomponent_add_property(ialarm
                , icalproperty_new_description(appt->title));
    else
        icalcomponent_add_property(ialarm
                , icalproperty_new_description(_("Orage default alarm")));
    if (appt->display_alarm_orage) {
        icalcomponent_add_property(ialarm
                , icalproperty_new_from_string("X-ORAGE-DISPLAY-ALARM:ORAGE"));
    }
    if (appt->display_alarm_notify) {
        icalcomponent_add_property(ialarm
                , icalproperty_new_from_string("X-ORAGE-DISPLAY-ALARM:NOTIFY"));
        g_snprintf(text, sizeof (text), "X-ORAGE-NOTIFY-ALARM-TIMEOUT:%d"
                , appt->display_notify_timeout);
        icalcomponent_add_property(ialarm
                , icalproperty_new_from_string(text));
    }
}

/*
     audioprop  = 2*(
                ; 'action' and 'trigger' are both REQUIRED,
                ; but MUST NOT occur more than once
                action / trigger /
                ; 'duration' and 'repeat' are both optional,
                ; and MUST NOT occur more than once each,
                ; but if one occurs, so MUST the other
                duration / repeat /
                ; the following is optional,
                ; but MUST NOT occur more than once
                attach /
                ; the following is optional,
                ; and MAY occur more than once
                x-prop
                )
*/
static void appt_add_alarm_internal_audio(xfical_appt *appt
        , icalcomponent *ialarm)
{
    icalattach *attach;

    icalcomponent_add_property(ialarm
            , icalproperty_new_action(ICAL_ACTION_AUDIO));
    attach = icalattach_new_from_url(appt->sound);
    icalcomponent_add_property(ialarm, icalproperty_new_attach(attach));
    if (appt->soundrepeat) {
        icalcomponent_add_property(ialarm
                , icalproperty_new_repeat(appt->soundrepeat_cnt));
        icalcomponent_add_property(ialarm
                , icalproperty_new_duration(
                        icaldurationtype_from_int(appt->soundrepeat_len)));
    }
    icalattach_unref(attach);
}

/* emailprop  = 5*(
                ; the following are all REQUIRED,
                ; but MUST NOT occur more than once
                action / description / trigger / summary
                ; the following is REQUIRED,
                ; and MAY occur more than once
                attendee /
                ; 'duration' and 'repeat' are both optional,
                ; and MUST NOT occur more than once each,
                ; but if one occurs, so MUST the other
                duration / repeat /
                ; the following are optional,
                ; and MAY occur more than once
                attach / x-prop
                )
*/
#if 0
static void appt_add_alarm_internal_email(xfical_appt *appt
        , icalcomponent *ialarm)
{
    icalcomponent_add_property(ialarm
            , icalproperty_new_action(ICAL_ACTION_EMAIL));
    g_warning ("EMAIL ACTION not implemented yet");
}
#endif

/* procprop   = 3*(
                ; the following are all REQUIRED,
                ; but MUST NOT occur more than once
                action / attach / trigger /
***** Not Implemented in Orage *****
                ; 'duration' and 'repeat' are both optional,
                ; and MUST NOT occur more than once each,
                ; but if one occurs, so MUST the other
                duration / repeat /
***** Not Implemented in Orage *****
                ; 'description' is optional,
                ; and MUST NOT occur more than once
                description /
                ; the following is optional,
                ; and MAY occur more than once
                x-prop
                )
*/
static void appt_add_alarm_internal_procedure(xfical_appt *appt
        , icalcomponent *ialarm)
{
    icalattach *attach;

    icalcomponent_add_property(ialarm
            , icalproperty_new_action(ICAL_ACTION_PROCEDURE));
    attach = icalattach_new_from_url(appt->procedure_cmd);
    icalcomponent_add_property(ialarm, icalproperty_new_attach(attach));
    if ORAGE_STR_EXISTS(appt->procedure_params)
        icalcomponent_add_property(ialarm
                , icalproperty_new_description(appt->procedure_params));
    icalattach_unref(attach);
}

/* Create new alarm and add trigger to it.
 * Note that Orage uses same trigger for all alarms.
 */
static icalcomponent *appt_add_alarm_internal_base(xfical_appt *appt
        , struct icaltriggertype trg)
{
    icalcomponent *ialarm;

    ialarm = icalcomponent_new(ICAL_VALARM_COMPONENT);
    if (appt->alarm_related_start)
        icalcomponent_add_property(ialarm, icalproperty_new_trigger(trg));
    else
        icalcomponent_add_property(ialarm
                , icalproperty_vanew_trigger(trg
                        , icalparameter_new_related(ICAL_RELATED_END)
                        , 0));
    if (appt->alarm_persistent)
        icalcomponent_add_property(ialarm
                , icalproperty_new_from_string("X-ORAGE-PERSISTENT-ALARM:YES"));
    return(ialarm);
}

/* Add VALARM.
 * FIXME: We assume all alarms start and stop same time = have same trigger
 */
static void appt_add_alarm_internal(xfical_appt *appt, icalcomponent *ievent)
{
    icalcomponent *ialarm;
    gint duration=0;
    struct icaltriggertype trg;

    /* these lines may not be needed if there are no alarms defined, but
       they are fast so it does not make sense to test */
    duration = appt->alarmtime;
    trg.time = icaltime_null_time();
    if (appt->alarm_before)
        trg.duration = icaldurationtype_from_int(-duration);
    else /* alarm happens after */
        trg.duration = icaldurationtype_from_int(duration);

    /********** DISPLAY **********/
    if (appt->display_alarm_orage || appt->display_alarm_notify) {
        ialarm = appt_add_alarm_internal_base(appt, trg);
        appt_add_alarm_internal_display(appt, ialarm);
        icalcomponent_add_component(ievent, ialarm);
    }

    /********** AUDIO **********/
    if (appt->sound_alarm && ORAGE_STR_EXISTS(appt->sound)) {
        ialarm = appt_add_alarm_internal_base(appt, trg);
        appt_add_alarm_internal_audio(appt, ialarm);
        icalcomponent_add_component(ievent, ialarm);
    }

#if 0
    /********** EMAIL **********/
    if ORAGE_STR_EXISTS(appt->sound) {
        ialarm = appt_add_alarm_internal_base(appt, trg);
        appt_add_alarm_internal_email(appt, ialarm);
        icalcomponent_add_component(ievent, ialarm);
    }
#endif

    /********** PROCEDURE **********/
    if (appt->procedure_alarm && ORAGE_STR_EXISTS(appt->procedure_cmd)) {
        ialarm = appt_add_alarm_internal_base(appt, trg);
        appt_add_alarm_internal_procedure(appt, ialarm);
        icalcomponent_add_component(ievent, ialarm);
    }
}

static void appt_add_exception_internal(const xfical_appt *appt
        , icalcomponent *icmp)
{
    struct icaltimetype wtime;
    const GList *gl_tmp;
    xfical_exception *excp;
    struct icaldatetimeperiodtype rdate;
    xfical_exception_type type;

    for (gl_tmp = g_list_first(appt->recur_exceptions);
         gl_tmp != NULL;
         gl_tmp = g_list_next(gl_tmp)) {
        excp = (xfical_exception *)gl_tmp->data;
        wtime = icaltimetype_from_gdatetime (xfical_exception_get_time (excp),
                                             FALSE);
        type = xfical_exception_get_type (excp);
        if (type == EXDATE)
            icalcomponent_add_property(icmp, icalproperty_new_exdate(wtime));
        else if (type == RDATE)
        {
            /* Orage does not support rdate periods */
            rdate.period = icalperiodtype_null_period();
            rdate.time = wtime;
            icalcomponent_add_property(icmp
                    , icalproperty_new_rdate(rdate));
        }
        else
            g_warning ("%s: unknown exception type %d, ignoring", G_STRFUNC,
                       type);
    }
}

/** Fill recurrence rule.
 *  NOTE: This function create RRULE string and then tranfer RRULE data to
 *  libical structure.
 */
static void appt_add_recur_internal (const xfical_appt *appt,
                                     icalcomponent *icmp)
{
    gchar recur_str[1001], *recur_p, *recur_p2;
    struct icalrecurrencetype rrule;
    struct icaltimetype wtime;
    gchar *byday_a[] = {"MO", "TU", "WE", "TH", "FR", "SA", "SU"};
    const char *text;
    int i, cnt;
    gint week;
    gint week_sel;
    icaltimezone *l_icaltimezone = NULL;
    gboolean add_interval = FALSE;

    if (appt->freq == XFICAL_FREQ_NONE) {
        return;
    }
    recur_p = g_stpcpy(recur_str, "FREQ=");
    switch(appt->freq) {
        case XFICAL_FREQ_DAILY:
            recur_p = g_stpcpy(recur_p, "DAILY");
            add_interval = (appt->interval > 1);
            break;
        case XFICAL_FREQ_WEEKLY:
            recur_p = g_stpcpy(recur_p, "WEEKLY");
            add_interval = (appt->interval > 1);
            break;
        case XFICAL_FREQ_MONTHLY:
            recur_p = g_stpcpy(recur_p, "MONTHLY");
            break;
        case XFICAL_FREQ_YEARLY:
            recur_p = g_stpcpy(recur_p, "YEARLY");
            break;
        case XFICAL_FREQ_HOURLY:
            recur_p = g_stpcpy(recur_p, "HOURLY");
            add_interval = (appt->interval > 1);
            break;
        default:
            g_warning ("%s: Unsupported freq", G_STRFUNC);
            icalrecurrencetype_clear(&rrule);
    }
    if (add_interval) { /* not default, need to insert it */
        recur_p += g_sprintf(recur_p, ";INTERVAL=%d", appt->interval);
    }
    if (appt->recur_limit == XFICAL_RECUR_COUNT) {
        recur_p += g_sprintf(recur_p, ";COUNT=%d", appt->recur_count);
    }
    else if (appt->recur_limit == XFICAL_RECUR_UNTIL) { /* needs to be in UTC */
/* BUG 2937: convert recur_until to utc from start time timezone */
        wtime = icaltimetype_from_gdatetime (appt->recur_until, FALSE);
        if ORAGE_STR_EXISTS(appt->start_tz_loc) {
        /* Null == floating => no special action needed */
            if (strcmp(appt->start_tz_loc, "floating") == 0) {
                wtime = icaltime_convert_to_zone(wtime, local_icaltimezone);
            }
            else if (strcmp(appt->start_tz_loc, "UTC") != 0) {
            /* FIXME: add this vtimezone to vcalendar if it is not there */
                l_icaltimezone = icaltimezone_get_builtin_timezone(
                        appt->start_tz_loc);
                wtime = icaltime_convert_to_zone(wtime, l_icaltimezone);
            }
        }
        else
            wtime = icaltime_convert_to_zone(wtime, local_icaltimezone);
        wtime = icaltime_convert_to_zone(wtime, utc_icaltimezone);
        text  = icaltime_as_ical_string(wtime);
        recur_p += g_sprintf(recur_p, ";UNTIL=%s", text);
    }

    switch (appt->freq)
    {
        case XFICAL_FREQ_WEEKLY:
            recur_p2 = recur_p; /* store current pointer */
            for (i = 0, cnt = 0; i <= 6; i++)
            {
                if (appt->recur_byday[i])
                {
                    if (cnt == 0) /* first day found */
                        recur_p = g_stpcpy (recur_p, ";BYDAY=");
                    else /* continue the list */
                        recur_p = g_stpcpy (recur_p, ",");
                    recur_p = g_stpcpy (recur_p, byday_a[i]);
                    cnt++;
                }
            }

            if (cnt == 7) { /* all days defined... */
                *recur_p2 = *recur_p; /* ...reset to null... */
            }
            else if (appt->interval > 1)
            {
                /* we have BYDAY rule, let's check week starting date:
                 * WKST has meaning only in two cases:
                 * 1) WEEKLY rule && interval > 1 && BYDAY rule is in use
                 * 2) YEARLY rule && BYWEEKNO rule is in use
                 * BUT Orage is not using BYWEEKNO rule, so we only check 1)
                 * Monday is default, so we can skip that, too
                 * */
                if (g_par.ical_weekstartday)
                    g_sprintf(recur_p, ";WKST=%s", byday_a[g_par.ical_weekstartday]);
            }
            break;

        case XFICAL_FREQ_MONTHLY:
            switch (appt->recur_month_type)
            {
                case XFICAL_RECUR_MONTH_TYPE_BEGIN:
                    g_sprintf (recur_p, ";BYMONTHDAY=%d",
                               appt->recur_month_days);
                    break;

                case XFICAL_RECUR_MONTH_TYPE_END:
                    g_sprintf (recur_p, ";BYMONTHDAY=-%d",
                               appt->recur_month_days);
                    break;

                case XFICAL_RECUR_MONTH_TYPE_EVERY:
                    g_assert ((int)appt->recur_day_sel >= 0);

                    week_sel = appt->recur_week_sel;
                    week = (week_sel == XFICAL_RECUR_MONTH_WEEK_LAST) ? -1 : week_sel + 1;

                    g_sprintf (recur_p, ";BYDAY=%d%s",
                               week, byday_a[appt->recur_day_sel]);
                    break;
            }
            break;

        case XFICAL_FREQ_YEARLY:
            g_assert ((int)appt->recur_day_sel >= 0);
            g_assert ((int)appt->recur_month_sel >= 0);

            week = (appt->recur_week_sel == XFICAL_RECUR_MONTH_WEEK_LAST)
                 ? -1 : (int)appt->recur_week_sel + 1;

            g_sprintf (recur_p, ";BYDAY=%d%s;BYMONTH=%d",
                       week, byday_a[appt->recur_day_sel],
                       appt->recur_month_sel + 1);
            break;

        default:
            break;
    }

    rrule = icalrecurrencetype_from_string(recur_str);
    icalcomponent_add_property(icmp, icalproperty_new_rrule(rrule));
    if (appt->type == XFICAL_TYPE_TODO) {
        if (appt->recur_todo_base_start)
            icalcomponent_add_property(icmp
                    , icalproperty_new_from_string(
                            "X-ORAGE-TODO-BASE:START"));
        else
            icalcomponent_add_property(icmp
                    , icalproperty_new_from_string(
                            "X-ORAGE-TODO-BASE:COMPLETED"));
    }
}

static void appt_add_starttime_internal (const xfical_appt *appt,
                                         icalcomponent *icmp)
{
    struct icaltimetype wtime;

    if (appt->starttime) {
        wtime = icaltimetype_from_gdatetime (appt->starttime, appt->allDay);
        if (appt->allDay) { /* date */
            icalcomponent_add_property(icmp
                    , icalproperty_new_dtstart(wtime));
        }
        else if ORAGE_STR_EXISTS(appt->start_tz_loc) {
            if (strcmp(appt->start_tz_loc, "UTC") == 0) {
                wtime=icaltime_convert_to_zone(wtime, utc_icaltimezone);
                icalcomponent_add_property(icmp
                        , icalproperty_new_dtstart(wtime));
            }
            else if (strcmp(appt->start_tz_loc, "floating") == 0) {
                icalcomponent_add_property(icmp
                        , icalproperty_new_dtstart(wtime));
            }
            else {
            /* FIXME: add this vtimezone to vcalendar if it is not there */
                icalcomponent_add_property(icmp
                        , icalproperty_vanew_dtstart(wtime
                                , icalparameter_new_tzid(appt->start_tz_loc)
                                , 0));
            }
        }
        else { /* floating time */
            icalcomponent_add_property(icmp
                    , icalproperty_new_dtstart(wtime));
        }
    }
}

static void appt_add_endtime_internal (const xfical_appt *appt,
                                       icalcomponent *icmp)
{
    struct icaltimetype wtime;
    gboolean end_time_done;
    struct icaldurationtype duration;

    if (!appt->use_due_time && (appt->type == XFICAL_TYPE_TODO)) {
        ; /* done with due time */
    }
    else if (appt->use_duration) {
        /* both event and todo can have duration */
        duration = icaldurationtype_from_int(appt->duration);
        icalcomponent_add_property(icmp
                , icalproperty_new_duration(duration));
    }
    else if (appt->endtime) {
        end_time_done = FALSE;
        wtime = icaltimetype_from_gdatetime (appt->endtime, FALSE);
        if (appt->allDay) { 
            /* need to add 1 day. For example:
             * DTSTART=20070221 & DTEND=20070223
             * means only 21 and 22 February */
            duration = icaldurationtype_from_int(60*60*24);
            wtime = icaltime_add(wtime, duration);
        }
        else if ORAGE_STR_EXISTS(appt->end_tz_loc) {
        /* Null == floating => no special action needed */
            if (strcmp(appt->end_tz_loc, "UTC") == 0)
                wtime = icaltime_convert_to_zone(wtime, utc_icaltimezone);
            else if (strcmp(appt->end_tz_loc, "floating") != 0) {
            /* FIXME: add this vtimezone to vcalendar if it is not there */
                if (appt->type == XFICAL_TYPE_EVENT)
                    icalcomponent_add_property(icmp
                        , icalproperty_vanew_dtend(wtime
                                , icalparameter_new_tzid(appt->end_tz_loc)
                                , 0));
                else if (appt->type == XFICAL_TYPE_TODO)
                    icalcomponent_add_property(icmp
                        , icalproperty_vanew_due(wtime
                                , icalparameter_new_tzid(appt->end_tz_loc)
                                , 0));
                end_time_done = TRUE;
            }
        }
        if (!end_time_done) {
            if (appt->type == XFICAL_TYPE_EVENT)
                icalcomponent_add_property(icmp
                        , icalproperty_new_dtend(wtime));
            else if (appt->type == XFICAL_TYPE_TODO)
                icalcomponent_add_property(icmp
                        , icalproperty_new_due(wtime));
        }
    }
}

static void appt_add_completedtime_internal (const xfical_appt *appt,
                                             icalcomponent *icmp)
{
    struct icaltimetype wtime;
    icaltimezone *l_icaltimezone = NULL;

    if (appt->type != XFICAL_TYPE_TODO) {
        return; /* only VTODO can have completed time */
    }
    if ((appt->completed) && (appt->completedtime)) {
        wtime = icaltimetype_from_gdatetime (appt->completedtime, FALSE);
        if ORAGE_STR_EXISTS(appt->completed_tz_loc) {
        /* Null == floating => no special action needed */
            if (strcmp(appt->completed_tz_loc, "floating") == 0) {
                wtime = icaltime_convert_to_zone(wtime
                        , local_icaltimezone);
            }
            else if (strcmp(appt->completed_tz_loc, "UTC") != 0) {
        /* FIXME: add this vtimezone to vcalendar if it is not there */
                l_icaltimezone = icaltimezone_get_builtin_timezone(
                        appt->completed_tz_loc);
                wtime = icaltime_convert_to_zone(wtime, l_icaltimezone);
            }
        }
        else
            wtime = icaltime_convert_to_zone(wtime, local_icaltimezone);
        wtime = icaltime_convert_to_zone(wtime, utc_icaltimezone);
        icalcomponent_add_property(icmp
                , icalproperty_new_completed(wtime));
    }
}

static gchar *appt_add_internal (xfical_appt *appt, gboolean add, gchar *uid,
                                 struct icaltimetype cre_time)
{
    icalcomponent *icmp;
    struct icaltimetype dtstamp, create_time;
    gchar *int_uid, *ext_uid;
    gchar **tmp_cat;
    int i;
    icalcomponent_kind ikind = ICAL_VEVENT_COMPONENT;

    dtstamp = icaltime_current_time_with_zone(utc_icaltimezone);
    if (add) {
        int_uid = ic_generate_uid();
        ext_uid = g_strconcat(uid, int_uid, NULL);
        appt->uid = ext_uid;
        create_time = dtstamp;
    }
    else { /* mod */
        int_uid = g_strdup(uid+4);
        ext_uid = uid;
        if (icaltime_is_null_time(cre_time))
            create_time = dtstamp;
        else
            create_time = cre_time;
    }

    if (appt->type == XFICAL_TYPE_EVENT)
        ikind = ICAL_VEVENT_COMPONENT;
    else if (appt->type == XFICAL_TYPE_TODO)
        ikind = ICAL_VTODO_COMPONENT;
    else if (appt->type == XFICAL_TYPE_JOURNAL)
        ikind = ICAL_VJOURNAL_COMPONENT;
    else
        g_critical ("%s: Unsupported Type", G_STRFUNC);

    icmp = icalcomponent_vanew(ikind
           , icalproperty_new_uid(int_uid)
           /* , icalproperty_new_categories("ORAGENOTE") */
           , icalproperty_new_class(ICAL_CLASS_PUBLIC)
           , icalproperty_new_dtstamp(dtstamp)
           , icalproperty_new_created(create_time)
           , icalproperty_new_lastmodified(dtstamp)
           , NULL);
    g_free(int_uid);

    if ORAGE_STR_EXISTS(appt->title)
        icalcomponent_add_property(icmp
                , icalproperty_new_summary(appt->title));
    if ORAGE_STR_EXISTS(appt->note)
        icalcomponent_add_property(icmp
                , icalproperty_new_description(appt->note));
    if (appt->type != XFICAL_TYPE_JOURNAL) {
        if ORAGE_STR_EXISTS(appt->location)
            icalcomponent_add_property(icmp
                    , icalproperty_new_location(appt->location));
    }
    if (appt->categories) { /* need to split it if more than one */
        tmp_cat = g_strsplit(appt->categories, ",", 0);
        for (i=0; tmp_cat[i] != NULL; i++) {
            icalcomponent_add_property(icmp
                   , icalproperty_new_categories(tmp_cat[i]));
        }
        g_strfreev(tmp_cat);
    }
    appt_add_starttime_internal(appt, icmp);

    if (appt->type != XFICAL_TYPE_JOURNAL) { 
        /* journal has no duration nor enddate or due 
         * journal also has no priority or transparent setting
         * journal also has not alarms or repeat settings */
        appt_add_endtime_internal(appt, icmp);
        appt_add_completedtime_internal(appt, icmp);

        if (appt->priority != 0)
            icalcomponent_add_property(icmp
                   , icalproperty_new_priority(appt->priority));
        if (appt->type == XFICAL_TYPE_EVENT) {
            if (appt->availability == 0)
                icalcomponent_add_property(icmp
                       , icalproperty_new_transp(ICAL_TRANSP_TRANSPARENT));
            else if (appt->availability == 1)
                icalcomponent_add_property(icmp
                       , icalproperty_new_transp(ICAL_TRANSP_OPAQUE));
        }
        /* NOTE: according to standard VJOURNAL _has_ recurrency, 
         * but I can't understand how it could be usefull, 
         * so Orage takes it away */
        appt_add_recur_internal(appt, icmp);
        appt_add_exception_internal(appt, icmp);
        appt_add_alarm_internal(appt, icmp);
    }

    if (ext_uid[0] == 'O') {
        icalcomponent_add_component(ic_ical, icmp);
        icalset_mark(ic_fical);
    }
    else if (ext_uid[0] == 'F') {
        sscanf(ext_uid, "F%02d", &i);
        if (i < g_par.foreign_count && ic_f_ical[i].ical != NULL) {
            icalcomponent_add_component(ic_f_ical[i].ical, icmp);
            icalset_mark(ic_f_ical[i].fical);
        }
        else {
            g_critical ("%s: unknown foreign file number %s", G_STRFUNC, uid);
            return(NULL);
        }
    }
    else { /* note: we never update/add Archive file */ 
        g_critical ("%s: unknown file type %s", G_STRFUNC, ext_uid);
        return(NULL);
    }
    xfical_alarm_build_list_internal(FALSE);
    ic_file_modified = TRUE;
    return(ext_uid);
}

gchar *xfical_appt_add (gchar *ical_file_id, xfical_appt *appt)
{
    return(appt_add_internal(appt, TRUE, ical_file_id, icaltime_null_time()));
}

static gboolean get_alarm_trigger(icalcomponent *ca,  xfical_appt *appt)
{
    icalproperty *p = NULL;
    struct icaltriggertype trg;
    icalparameter *trg_related_par;
    icalparameter_related rel;

    p = icalcomponent_get_first_property(ca, ICAL_TRIGGER_PROPERTY);
    if (p) {
        trg = icalproperty_get_trigger(p);
        if (icaltime_is_null_time(trg.time)) {
            appt->alarmtime = icaldurationtype_as_int(trg.duration);
            if (appt->alarmtime < 0) { /* before */
                appt->alarmtime *= -1;
                appt->alarm_before = TRUE;
            }
            else
                appt->alarm_before = FALSE;
            trg_related_par = icalproperty_get_first_parameter(p
                    , ICAL_RELATED_PARAMETER);
            if (trg_related_par) {
                rel = icalparameter_get_related(trg_related_par);
                if (rel == ICAL_RELATED_END)
                    appt->alarm_related_start = FALSE;
                else
                    appt->alarm_related_start = TRUE;
            }
            else
                appt->alarm_related_start = TRUE;
        }
        else
            g_warning ("%s: Can not process time triggers", G_STRFUNC);
    }
    else {
        g_warning ("%s: Trigger missing. Ignoring alarm", G_STRFUNC);
        return(FALSE);
    }
    return(TRUE);
}

static void get_alarm_data_x(icalcomponent *ca,  xfical_appt *appt)
{
    icalproperty *p = NULL;
    char *text;
    int i;

    for (p = icalcomponent_get_first_property(ca, ICAL_X_PROPERTY);
         p != 0;
         p = icalcomponent_get_next_property(ca, ICAL_X_PROPERTY)) {
        text = (char *)icalproperty_get_x_name(p);
        if (!strcmp(text, "X-ORAGE-PERSISTENT-ALARM")) {
            text = (char *)icalproperty_get_value_as_string(p);
            if (!strcmp(text, "YES"))
                appt->alarm_persistent = TRUE;
        }
        else if (!strcmp(text, "X-ORAGE-DISPLAY-ALARM")) {
            text = (char *)icalproperty_get_value_as_string(p);
            if (!strcmp(text, "ORAGE")) {
                appt->display_alarm_orage = TRUE;
            }
            else if (!strcmp(text, "NOTIFY")) {
                appt->display_alarm_notify = TRUE;
            }
        }
        else if (!strcmp(text, "X-ORAGE-NOTIFY-ALARM-TIMEOUT")) {
            text = (char *)icalproperty_get_value_as_string(p);
            sscanf(text, "%d", &i);
            appt->display_notify_timeout = i;
        }
        else {
            g_warning ("%s: unknown X property %s", G_STRFUNC, text);
        }
    }
}

static void get_alarm_data(icalcomponent *ca,  xfical_appt *appt)
{
    icalproperty *p = NULL;
    enum icalproperty_action act;
    icalattach *attach;
    struct icaldurationtype duration;
    char *text;

    p = icalcomponent_get_first_property(ca, ICAL_ACTION_PROPERTY);
    if (!p) {
        g_warning ("%s: No ACTION in alarm. Ignoring this ALARM.", G_STRFUNC);
        return;
    }
    act = icalproperty_get_action(p);
    if (act == ICAL_ACTION_DISPLAY) {
        get_alarm_data_x(ca, appt);
        /* fixing bug 6643 by commenting next 3 lines */
        /* FIXME properly by adding real alarm description
        p = icalcomponent_get_first_property(ca, ICAL_DESCRIPTION_PROPERTY);
        if (p)
            appt->note = (char *)icalproperty_get_description(p);
            */
        /* default display alarm is orage if none is set */
        if (!appt->display_alarm_orage && !appt->display_alarm_notify) {
            if (g_par.use_foreign_display_alarm_notify)
                appt->display_alarm_notify = TRUE;
            else
                appt->display_alarm_orage = TRUE;
        }
    }
    else if (act == ICAL_ACTION_AUDIO) {
        get_alarm_data_x(ca, appt);
        p = icalcomponent_get_first_property(ca, ICAL_ATTACH_PROPERTY);
        if (p) {
            appt->sound_alarm = TRUE;
            attach = icalproperty_get_attach(p);
            appt->sound = (char *)icalattach_get_url(attach);
            p = icalcomponent_get_first_property(ca, ICAL_REPEAT_PROPERTY);
            if (p) {
                appt->soundrepeat = TRUE;
                appt->soundrepeat_cnt = icalproperty_get_repeat(p);
            }
            p = icalcomponent_get_first_property(ca, ICAL_DURATION_PROPERTY);
            if (p) {
                duration = icalproperty_get_duration(p);
                appt->soundrepeat_len = icaldurationtype_as_int(duration);
            }
        }
    }
    else if (act == ICAL_ACTION_PROCEDURE) {
        get_alarm_data_x(ca, appt);
        p = icalcomponent_get_first_property(ca, ICAL_ATTACH_PROPERTY);
        if (p) {
            appt->procedure_alarm = TRUE;
            attach = icalproperty_get_attach(p);
            text = (char *)icalattach_get_url(attach);
            if (text) {
                appt->procedure_cmd = text;
                p = icalcomponent_get_first_property(ca
                        , ICAL_DESCRIPTION_PROPERTY);
                if (p)
                    appt->procedure_params = 
                            (char *)icalproperty_get_description(p);
            }
        }
    }
    else { /* unknown alarm action */
        g_warning ("%s: Unknown ACTION (%d) in alarm. Ignoring ALARM.",
                   G_STRFUNC, act);
        return;
    }
}

static void get_appt_alarm_from_icalcomponent(icalcomponent *c
        , xfical_appt *appt)
{
    icalcomponent *ca = NULL;
    icalcompiter ci;
    gboolean trg_processed = FALSE;

    for (ci = icalcomponent_begin_component(c, ICAL_VALARM_COMPONENT);
         icalcompiter_deref(&ci) != 0;
         icalcompiter_next(&ci)) {
        ca = icalcompiter_deref(&ci);
        /* FIXME: Orage assumes all alarms have similar trigger, which may
         * not be the case with other than Orage originated alarms.
         * We process trigger only once. */
        if (!trg_processed) {
            trg_processed = TRUE;
            if (!get_alarm_trigger(ca, appt)) {
                g_warning ("%s: Trigger missing. Ignoring alarm", G_STRFUNC);
                return;
            }
        }
        if (trg_processed) {
            get_alarm_data(ca, appt);
        }
    }
}

static void process_start_date(xfical_appt *appt, icalproperty *p
        , struct icaltimetype *itime, struct icaltimetype *stime
        , struct icaltimetype *sltime)
{
    const char *text;

    text = icalproperty_get_value_as_string(p);
    *itime = icaltime_from_string(text);
    *stime = ic_convert_to_timezone(*itime, p);
    *sltime = convert_to_local_timezone(*itime, p);
    orage_gdatetime_unref (appt->starttime);
    appt->starttime = orage_icaltime_to_gdatetime (text);
    if (icaltime_is_date(*itime)) {
        appt->allDay = TRUE;
        appt->start_tz_loc = "floating";
    }
    else if (icaltime_is_utc(*itime)) {
        appt->start_tz_loc = "UTC";
    }
    else { /* let's check timezone */
        char *t;

        appt->start_tz_loc = ((t = ic_get_char_timezone(p)) ? t : "floating");
    }

    if (appt->endtime == NULL) {
        appt->endtime = g_date_time_ref (appt->starttime);
        appt->end_tz_loc = appt->start_tz_loc;
    }
}

static void process_end_date(xfical_appt *appt, icalproperty *p
        , struct icaltimetype *itime, struct icaltimetype *eltime)
{
    const char *text;

    text = icalproperty_get_value_as_string(p);
    *itime = icaltime_from_string(text);
    *eltime = convert_to_local_timezone(*itime, p);
#if 0
    text  = icaltime_as_ical_string(*itime);
#endif
    orage_gdatetime_unref (appt->endtime);
    appt->endtime = orage_icaltime_to_gdatetime (text);
    if (icaltime_is_date(*itime)) {
        appt->allDay = TRUE;
        appt->end_tz_loc = "floating";
    }
    else if (icaltime_is_utc(*itime)) {
        appt->end_tz_loc = "UTC";
    }
    else { /* let's check timezone */
        char *t;

        appt->end_tz_loc = ((t = ic_get_char_timezone(p)) ? t : "floating");
    }
    appt->use_due_time = TRUE;
}

static void process_completed_date(xfical_appt *appt, icalproperty *p)
{
    const char *text;
    struct icaltimetype itime;
    struct icaltimetype eltime;

    text = icalproperty_get_value_as_string(p);
    itime = icaltime_from_string(text);
    eltime = convert_to_local_timezone(itime, p);
    text  = icaltime_as_ical_string(eltime);
    orage_gdatetime_unref (appt->completedtime);
    appt->completedtime = orage_icaltime_to_gdatetime (text);
    appt->completed_tz_loc = g_par.local_timezone;
    appt->completed = TRUE;
}

static void ical_appt_get_rrule_internal (G_GNUC_UNUSED icalcomponent *c,
                                          xfical_appt *appt,
                                          icalproperty *p)
{
    struct icalrecurrencetype rrule;
    int i, cnt, day;
    GDateTime *gdt;
    gint month_day;

    rrule = icalproperty_get_rrule(p);
    switch (rrule.freq) {
        case ICAL_DAILY_RECURRENCE:
            appt->freq = XFICAL_FREQ_DAILY;
            break;
        case ICAL_WEEKLY_RECURRENCE:
            appt->freq = XFICAL_FREQ_WEEKLY;
            break;
        case ICAL_MONTHLY_RECURRENCE:
            appt->freq = XFICAL_FREQ_MONTHLY;
            month_day = rrule.by_month_day[0];
            if (month_day != ICAL_RECURRENCE_ARRAY_MAX)
            {
                if (month_day > 0)
                {
                    appt->recur_month_type = XFICAL_RECUR_MONTH_TYPE_BEGIN;
                    appt->recur_month_days = month_day;
                }
                else
                {
                    appt->recur_month_type = XFICAL_RECUR_MONTH_TYPE_END;
                    appt->recur_month_days = -1 * month_day;
                }
            }
            else if (rrule.by_day[0] != ICAL_RECURRENCE_ARRAY_MAX)
            {
                appt->recur_month_type = XFICAL_RECUR_MONTH_TYPE_EVERY;

                cnt = icalrecurrencetype_day_position (rrule.by_day[0]);

                appt->recur_week_sel =
                        cnt < 0 ? XFICAL_RECUR_MONTH_WEEK_LAST : cnt - 1;
                appt->recur_day_sel =
                        orage_recurrence_type_to_day_of_week (rrule.by_day[0]);
            }
            else
                g_critical ("%s: unknown weekday for %s", G_STRFUNC, appt->uid);

            break;
        case ICAL_YEARLY_RECURRENCE:
            appt->freq = XFICAL_FREQ_YEARLY;

            cnt = icalrecurrencetype_day_position (rrule.by_day[0]);

            appt->recur_week_sel =
                    cnt < 0 ? XFICAL_RECUR_MONTH_WEEK_LAST : cnt - 1;
            appt->recur_day_sel =
                    orage_recurrence_type_to_day_of_week (rrule.by_day[0]);
            appt->recur_month_sel =
                    icalrecurrencetype_month_month (rrule.by_month[0]) - 1;
            break;
        case ICAL_HOURLY_RECURRENCE:
            appt->freq = XFICAL_FREQ_HOURLY;
            break;
        default:
            appt->freq = XFICAL_FREQ_NONE;
            break;
    }
    if ((appt->recur_count = rrule.count))
        appt->recur_limit = XFICAL_RECUR_COUNT;
    else if(! icaltime_is_null_time(rrule.until)) {
        const char *text;

        text = icaltime_as_ical_string (rrule.until);
        gdt = orage_icaltime_to_gdatetime (text);
        if (gdt)
        {
            orage_gdatetime_unref (appt->recur_until);
            appt->recur_until = gdt;
            appt->recur_limit = XFICAL_RECUR_UNTIL;
        }
        else
            appt->recur_limit = XFICAL_RECUR_NO_LIMIT;
    }
    if (rrule.by_day[0] != ICAL_RECURRENCE_ARRAY_MAX) {
        for (i=0; i <= 6; i++)
            appt->recur_byday[i] = FALSE;
        for (i=0; i <= 6 && rrule.by_day[i] != ICAL_RECURRENCE_ARRAY_MAX; i++) {
            cnt = (int)floor((double)(rrule.by_day[i]/8));
            day = abs(rrule.by_day[i]-8*cnt);
            switch (day) {
                case ICAL_MONDAY_WEEKDAY:
                    appt->recur_byday[0] = TRUE;
                    break;
                case ICAL_TUESDAY_WEEKDAY:
                    appt->recur_byday[1] = TRUE;
                    break;
                case ICAL_WEDNESDAY_WEEKDAY:
                    appt->recur_byday[2] = TRUE;
                    break;
                case ICAL_THURSDAY_WEEKDAY:
                    appt->recur_byday[3] = TRUE;
                    break;
                case ICAL_FRIDAY_WEEKDAY:
                    appt->recur_byday[4] = TRUE;
                    break;
                case ICAL_SATURDAY_WEEKDAY:
                    appt->recur_byday[5] = TRUE;
                    break;
                case ICAL_SUNDAY_WEEKDAY:
                    appt->recur_byday[6] = TRUE;
                    break;
                case ICAL_NO_WEEKDAY:
                    break;
                default:
                    g_critical ("%s: unknown weekday %s: %d/%d (%x)", G_STRFUNC,
                                appt->uid, rrule.by_day[i], i, rrule.by_day[i]);
                    break;
            }
        }
    }
    appt->interval = rrule.interval;
}

static void appt_init(xfical_appt *appt)
{
    xfical_appt_init0 (appt);

    appt->uid = NULL;
    appt->title = NULL;
    appt->location = NULL;
    appt->allDay = FALSE;
    appt->readonly = FALSE;
    appt->starttime = NULL;
    appt->start_tz_loc = NULL;
    appt->use_due_time = FALSE;
    appt->endtime = NULL;
    appt->end_tz_loc = NULL;
    appt->use_duration = FALSE;
    appt->duration = 0;
    appt->completed = FALSE;
    appt->completedtime = NULL;
    appt->completed_tz_loc = NULL;
    appt->availability = -1;
    appt->priority = 0;
    appt->categories = NULL;
    appt->note = NULL;
    appt->alarmtime = 0;
    appt->alarm_before = TRUE;
    appt->alarm_related_start = TRUE;
    appt->alarm_persistent = FALSE;
    appt->sound_alarm = FALSE;
    appt->sound = NULL;
    appt->soundrepeat = FALSE;
    appt->soundrepeat_cnt = 500;
    appt->soundrepeat_len = 2;
    appt->display_alarm_orage = FALSE;
    appt->display_alarm_notify = FALSE;
    appt->display_notify_timeout = 0;
    appt->procedure_alarm = FALSE;
    appt->procedure_cmd = NULL;
    appt->procedure_params = NULL;
    appt->endtimecur = g_date_time_ref (appt->starttimecur);
    appt->recur_limit = XFICAL_RECUR_NO_LIMIT;
    appt->recur_count = 0;
    appt->recur_until = NULL;
#if 0
    appt->email_alarm = FALSE;
    appt->email_attendees = NULL;
#endif

    appt->recur_todo_base_start = TRUE;
    appt->recur_exceptions = NULL;
}

static gboolean get_appt_from_icalcomponent(icalcomponent *c, xfical_appt *appt)
{
    const char *text;
    icalproperty *p = NULL;
    struct icaltimetype itime, stime, etime, sltime, eltime, wtime;
    icaltimezone *l_icaltimezone = NULL;
    icalproperty_transp xf_transp;
    struct icaldurationtype duration, duration_tmp;
    gboolean stime_found = FALSE, etime_found = FALSE;
    xfical_exception *excp;
    struct icaldatetimeperiodtype rdate;
    GDateTime *gdt;

    appt_init (appt);

    /********** Component type ********/
    /* we want isolate all libical calls and features into this file,
     * so need to remap component type to our own defines */
    if (icalcomponent_isa(c) == ICAL_VEVENT_COMPONENT)
        appt->type = XFICAL_TYPE_EVENT;
    else if (icalcomponent_isa(c) == ICAL_VTODO_COMPONENT)
        appt->type = XFICAL_TYPE_TODO;
    else if (icalcomponent_isa(c) == ICAL_VJOURNAL_COMPONENT)
        appt->type = XFICAL_TYPE_JOURNAL;
    else {
        g_warning ("%s: Unknown component", G_STRFUNC);
        return(FALSE);
    }
        /*********** Defaults ***********/
    stime = icaltime_null_time();
    sltime = icaltime_null_time();
    eltime = icaltime_null_time();
    duration = icaldurationtype_null_duration ();

    /*********** Properties ***********/
    for (p = icalcomponent_get_first_property(c, ICAL_ANY_PROPERTY);
         p != 0;
         p = icalcomponent_get_next_property(c, ICAL_ANY_PROPERTY)) {
        /* these are in icalderivedproperty.h */
        switch (icalproperty_isa(p)) {
            case ICAL_SUMMARY_PROPERTY:
                appt->title = (char *)icalproperty_get_summary(p);
                break;
            case ICAL_LOCATION_PROPERTY:
                appt->location = (char *)icalproperty_get_location(p);
                break;
            case ICAL_DESCRIPTION_PROPERTY:
                appt->note = (char *)icalproperty_get_description(p);
                break;
            case ICAL_UID_PROPERTY:
                appt->uid = (char *)icalproperty_get_uid(p);
                break;
            case ICAL_TRANSP_PROPERTY:
                xf_transp = icalproperty_get_transp(p);
                if (xf_transp == ICAL_TRANSP_TRANSPARENT)
                    appt->availability = 0;
                else if (xf_transp == ICAL_TRANSP_OPAQUE)
                    appt->availability = 1;
                else
                    appt->availability = -1;
                break;
            case ICAL_DTSTART_PROPERTY:
                if (!stime_found)
                    process_start_date(appt, p, &itime, &stime, &sltime);
                break;
            case ICAL_DTEND_PROPERTY:
            case ICAL_DUE_PROPERTY:
                if (!etime_found)
                    process_end_date(appt, p, &itime, &eltime);
                break;
            case ICAL_COMPLETED_PROPERTY:
                process_completed_date(appt, p);
                break;
            case ICAL_DURATION_PROPERTY:
                appt->use_duration = TRUE;
                appt->use_due_time = TRUE;
                duration = icalproperty_get_duration(p);
                appt->duration = icaldurationtype_as_int(duration);
                break;
            case ICAL_RRULE_PROPERTY:
                ical_appt_get_rrule_internal(c, appt, p);
                break;
            case ICAL_X_PROPERTY:
                text = icalproperty_get_x_name(p);
                if (g_str_has_prefix(text, "X-ORAGE-ORIG-DTSTART")) {
                    process_start_date(appt, p, &itime, &stime, &sltime);
                    stime_found = TRUE;
                    break;
                }
                if (g_str_has_prefix(text, "X-ORAGE-ORIG-DTEND")) {
                    process_end_date(appt, p, &itime, &eltime);
                    etime_found = TRUE;
                    break;
                }
                if (g_str_has_prefix(text, "X-ORAGE-TODO-BASE")) {
                    text = icalproperty_get_value_as_string(p);
                    if (strcmp(text, "START") == 0)
                        appt->recur_todo_base_start = TRUE;
                    else
                        appt->recur_todo_base_start = FALSE;

                    break;
                }
                if (g_str_has_prefix (text, "X-GOOGLE-CONFERENCE"))
                {
                    /* Ignore, Orage does not support this now. */
                    break;
                }
                g_warning ("%s: unknown X property %s", G_STRFUNC, text);
                break; /* ICAL_X_PROPERTY: */
            case ICAL_PRIORITY_PROPERTY:
                appt->priority = icalproperty_get_priority(p);
                break;
            case ICAL_CATEGORIES_PROPERTY:
                if (appt->categories == NULL)
                    appt->categories = g_strdup(icalproperty_get_categories(p));
                else {
                    text = appt->categories;
                    appt->categories = g_strjoin(","
                            , appt->categories
                            , icalproperty_get_categories(p), NULL);
                    g_free((char *)text);
                }
                break;
            case ICAL_EXDATE_PROPERTY:
                text = icalproperty_get_value_as_string(p);
                if (strlen(text) > 16)
                {
                    g_message ("%s: invalid EXDATE %s. Ignoring",
                               G_STRFUNC, text);
                }
                else {
                    gdt = orage_icaltime_to_gdatetime (text);
                    excp = xfical_exception_new (gdt, FALSE, EXDATE);
                    g_date_time_unref (gdt);
                    appt->recur_exceptions =
                            g_list_prepend(appt->recur_exceptions, excp);
                }
                break;
            case ICAL_RDATE_PROPERTY:
                rdate = icalproperty_get_rdate(p);
                /* Orage does not support rdate periods */
                if (!icalperiodtype_is_null_period(rdate.period)) {
                    g_message ("%s: Orage does not support rdate periods, but "
                               "only simple rdate. Ignoring %s", G_STRFUNC,
                               (char *)icalproperty_get_value_as_string(p));
                }
                else {
                    text = icalproperty_get_value_as_string(p);
                    if (strlen(text) > 16)
                    {
                        g_message ("%s: invalid RDATE %s. Ignoring", G_STRFUNC,
                                   text);
                    }
                    else {
                        gdt = orage_icaltime_to_gdatetime (text);
                        excp = xfical_exception_new (gdt, FALSE, RDATE);
                        g_date_time_unref (gdt);
                        appt->recur_exceptions =
                                g_list_prepend(appt->recur_exceptions, excp);
                    }
                }
                break;
            case ICAL_CLASS_PROPERTY:
            case ICAL_DTSTAMP_PROPERTY:
            case ICAL_CREATED_PROPERTY:
            case ICAL_LASTMODIFIED_PROPERTY:
            case ICAL_SEQUENCE_PROPERTY:
            case ICAL_ORGANIZER_PROPERTY:
            case ICAL_STATUS_PROPERTY:
            case ICAL_ATTENDEE_PROPERTY:
            case ICAL_RECURRENCEID_PROPERTY:
                break;
            default:
                g_message ("%s: unknown property %s", G_STRFUNC,
                           icalproperty_get_property_name(p));
                break;
        }
    } /* Property for loop */
    get_appt_alarm_from_icalcomponent(c, appt);

    /* need to set missing endtime or duration */
    if (appt->use_duration) {
        etime = icaltime_add(stime, duration);
        text  = icaltime_as_ical_string(etime);
        orage_gdatetime_unref (appt->endtime);
        appt->endtime = orage_icaltime_to_gdatetime (text);
        appt->end_tz_loc = appt->start_tz_loc;
    }
    else {
        duration = icaltime_subtract(eltime, sltime);
        appt->duration = icaldurationtype_as_int(duration);
        if (appt->allDay && appt->duration) {
    /* need to subtract 1 day.
     * read explanation from appt_add_internal: appt.endtime processing */
            duration_tmp = icaldurationtype_from_int(60*60*24);
            appt->duration -= icaldurationtype_as_int(duration_tmp);
            duration = icaldurationtype_from_int(appt->duration);
            etime = icaltime_add(stime, duration);
            text  = icaltime_as_ical_string(etime);
            orage_gdatetime_unref (appt->endtime);
            appt->endtime = orage_icaltime_to_gdatetime (text);
        }
    }
    if (appt->recur_limit == XFICAL_RECUR_UNTIL) { /* BUG 2937: convert back from UTC */
        wtime = icaltimetype_from_gdatetime (appt->recur_until, FALSE);
        if (! ORAGE_STR_EXISTS(appt->start_tz_loc) )
            wtime = icaltime_convert_to_zone(wtime, local_icaltimezone);
        else if (strcmp(appt->start_tz_loc, "floating") == 0)
            wtime = icaltime_convert_to_zone(wtime, local_icaltimezone);
        else if (strcmp(appt->start_tz_loc, "UTC") != 0) {
            /* done if in UTC, but real conversion needed if not */
            l_icaltimezone = icaltimezone_get_builtin_timezone(
                    appt->start_tz_loc);
            wtime = icaltime_convert_to_zone(wtime, l_icaltimezone);
        }
        text  = icaltime_as_ical_string(wtime);
        orage_gdatetime_unref (appt->recur_until);
        appt->recur_until = orage_icaltime_to_gdatetime (text);
    }
    return(TRUE);
}

 /* Read EVENT/TODO/JOURNAL component from ical datafile.
  * ical_uid:  key of ical comp appt-> is to be read
  * returns: if failed: NULL
  *          if successfull: xfical_appt pointer to xfical_appt struct 
  *              filled with data.
  *              You need to deallocate the struct after it is not used.
  *          NOTE: This routine does not fill starttimecur nor endtimecur,
  *                Those are always initialized to null string
  */
static xfical_appt *xfical_appt_get_internal(const char *ical_uid
        , icalcomponent *base)
{
    xfical_appt appt;
    icalcomponent *c = NULL;
    gboolean key_found = FALSE;
    const char *text;

    xfical_appt_init0 (&appt);

    for (c = icalcomponent_get_first_component(base, ICAL_ANY_COMPONENT); 
         c != 0 && (key_found == FALSE);
         c = icalcomponent_get_next_component(base, ICAL_ANY_COMPONENT)) {
        text = icalcomponent_get_uid(c);

        if (ORAGE_STR_EXISTS(text) && strcmp(text, ical_uid) == 0) { 
            /* we found our uid (=component) */
            key_found = get_appt_from_icalcomponent(c, &appt);
        }
    }
    if (key_found) {
        return(g_memdup(&appt, sizeof(xfical_appt)));
    }
    else {
        return(NULL);
    }
}

static void xfical_appt_get_fill_internal (xfical_appt *appt,
                                           const gchar *file_type)
{
    if (appt) {
        appt->uid = g_strconcat(file_type, appt->uid, NULL);
        appt->title = g_strdup(appt->title);
        appt->location = g_strdup(appt->location);
        if (appt->start_tz_loc)
            appt->start_tz_loc = g_strdup(appt->start_tz_loc);
        else
            appt->start_tz_loc = g_strdup("floating");
        if (appt->end_tz_loc)
            appt->end_tz_loc = g_strdup(appt->end_tz_loc);
        else
            appt->end_tz_loc = g_strdup("floating");
        if (appt->completed_tz_loc)
            appt->completed_tz_loc = g_strdup(appt->completed_tz_loc);
        else
            appt->completed_tz_loc = g_strdup("floating");
        appt->note = g_strdup(appt->note);
        appt->sound = g_strdup(appt->sound);
        appt->procedure_cmd = g_strdup(appt->procedure_cmd);
        appt->procedure_params = g_strdup(appt->procedure_params);
    }
}

static xfical_appt *appt_get_any(const char *ical_uid, icalcomponent *base
        , const gchar *file_type)
{
    xfical_appt *appt;

    if ((appt = xfical_appt_get_internal(ical_uid, base))) {
        xfical_appt_get_fill_internal(appt, file_type);
    }
    return(appt);
}

xfical_appt *xfical_appt_get(const gchar *uid)
{
    xfical_appt *appt;
    const char *ical_uid;
    char file_type[8];
    gint i;

    strncpy(file_type, uid, 4); /* file id */
    file_type[4] = '\0';
    ical_uid = uid+4; /* skip file id */
    if (uid[0] == 'O') {
        appt = appt_get_any(ical_uid, ic_ical, file_type);
    }
#ifdef HAVE_ARCHIVE
    else if (uid[0] == 'A') {
        appt = appt_get_any(ical_uid, ic_aical, file_type);
    }
#endif
    else if (uid[0] == 'F') {
        sscanf(uid, "F%02d", &i);
        if (i < g_par.foreign_count && ic_f_ical[i].ical != NULL) {
            appt = appt_get_any(ical_uid, ic_f_ical[i].ical, file_type);
            if (appt)
                appt->readonly = g_par.foreign_data[i].read_only;
        }
        else {
            g_critical ("%s: unknown foreign file number %s", G_STRFUNC, uid);
            return(NULL);
        }
    }
    else {
        g_critical ("%s: unknown file type %s", G_STRFUNC, uid);
        return(NULL);
    }
    return(appt);
}

void xfical_appt_free(xfical_appt *appt)
{
    GList *tmp;

    if (!appt)
        return;
    g_free(appt->uid);
    g_free(appt->title);
    g_free(appt->location);
    g_free(appt->start_tz_loc);
    g_free(appt->end_tz_loc);
    g_free(appt->completed_tz_loc);
    g_free(appt->note);
    g_free(appt->sound);
    g_free(appt->procedure_cmd);
    g_free(appt->procedure_params);
    g_free(appt->categories);
    orage_gdatetime_unref (appt->starttime);
    orage_gdatetime_unref (appt->endtime);
    orage_gdatetime_unref (appt->completedtime);
#if 0
    g_free(appt->email_attendees);
#endif
    for (tmp = g_list_first(appt->recur_exceptions);
         tmp != NULL;
         tmp = g_list_next(tmp)) {
        xfical_exception_unref (tmp->data);
    }
    g_list_free(appt->recur_exceptions);
    g_free(appt);
}

gboolean xfical_appt_mod (gchar *ical_uid, xfical_appt *appt)
{
    icalcomponent *c, *base;
    gchar *uid;
    const gchar *int_uid;
    icalproperty *p = NULL;
    gboolean key_found = FALSE;
    struct icaltimetype create_time = icaltime_null_time();
    int i;

    if (ical_uid == NULL) {
        g_message ("%s: Got NULL uid. doing nothing", G_STRFUNC);
        return(FALSE);
    }
    int_uid = ical_uid+4;
    if (ical_uid[0] == 'O')
        base = ic_ical;
    else if (ical_uid[0] == 'F') {
        sscanf(ical_uid, "F%02d", &i);
        if (i < g_par.foreign_count && ic_f_ical[i].ical != NULL)
            base = ic_f_ical[i].ical;
        else {
            g_critical ("%s: unknown foreign file number %s", G_STRFUNC,
                        ical_uid);
            return(FALSE);
        }
        if (g_par.foreign_data[i].read_only) {
            g_message ("%s: foreign file %s is READ only. Not modified.",
                       G_STRFUNC, g_par.foreign_data[i].file);
            return(FALSE);
        }
    }
    else { /* note: we never update/add Archive file */ 
        g_critical ("%s: unknown file type %s", G_STRFUNC, ical_uid);
        return(FALSE);
    }
    for (c = icalcomponent_get_first_component(base, ICAL_ANY_COMPONENT); 
         c != 0 && !key_found;
         c = icalcomponent_get_next_component(base, ICAL_ANY_COMPONENT)) {
        uid = (char *)icalcomponent_get_uid(c);
        if (ORAGE_STR_EXISTS(uid) && (strcmp(uid, int_uid) == 0)) {
            if ((p = icalcomponent_get_first_property(c,
                            ICAL_CREATED_PROPERTY)))
                create_time = icalproperty_get_created(p);
            icalcomponent_remove_component(base, c);
            key_found = TRUE;
        }
    } 
    if (!key_found) {
        g_warning ("%s: uid %s not found. Doing nothing", G_STRFUNC, ical_uid);
        return(FALSE);
    }

    appt_add_internal(appt, FALSE, ical_uid, create_time);
    return(TRUE);
}

gboolean xfical_appt_del (gchar *ical_uid)
{
    icalcomponent *c, *base;
    icalset *fbase;
    char *uid, *int_uid;
    int i;

    if (ical_uid == NULL) {
        g_warning ("%s: Got NULL uid. doing nothing", G_STRFUNC);
        return(FALSE);
    }
    int_uid = ical_uid+4;
    if (ical_uid[0] == 'O') {
        base = ic_ical;
        fbase = ic_fical;
    }
    else if (ical_uid[0] == 'F') {
        sscanf(ical_uid, "F%02d", &i);
        if (i < g_par.foreign_count && ic_f_ical[i].ical != NULL) {
            base = ic_f_ical[i].ical;
            fbase = ic_f_ical[i].fical;
        }
        else {
            g_critical ("%s, unknown foreign file number %s",
                        G_STRFUNC, ical_uid);
            return(FALSE);
        }
        if (g_par.foreign_data[i].read_only) {
            g_warning ("%s: foreign file %s is READ only. Not modified.",
                       G_STRFUNC, g_par.foreign_data[i].file);
            return(FALSE);
        }
    }
    else { /* note: we never update/add Archive file */ 
        g_critical ("%s: unknown file type %s", G_STRFUNC, ical_uid);
        return(FALSE);
    }

    for (c = icalcomponent_get_first_component(base, ICAL_ANY_COMPONENT); 
         c != 0;
         c = icalcomponent_get_next_component(base, ICAL_ANY_COMPONENT)) {
        uid = (char *)icalcomponent_get_uid(c);
        if (ORAGE_STR_EXISTS(uid) && strcmp(uid, int_uid) == 0) {
            icalcomponent_remove_component(base, c);
            icalset_mark(fbase);
            xfical_alarm_build_list_internal(FALSE);
            ic_file_modified = TRUE;
            return(TRUE);
        }
    } 
    g_warning ("%s: uid %s not found. Doing nothing", G_STRFUNC, ical_uid);
    return(FALSE);
}

static void set_todo_times(icalcomponent *c, xfical_period *per)
{
    char *text;
    icalproperty *p = NULL;

    if (per->ikind == ICAL_VTODO_COMPONENT 
    && !icaltime_is_null_time(per->ctime)) {
        /* we need to check if repeat is based on 
         * start date or complete date */
        for (p = icalcomponent_get_first_property(c, ICAL_X_PROPERTY);
             p != 0;
             p = icalcomponent_get_next_property(c, ICAL_X_PROPERTY)) {
            text = (char *)icalproperty_get_x_name(p);
            if (!strcmp(text, "X-ORAGE-TODO-BASE")) {
                text = (char *)icalproperty_get_value_as_string(p);
                if (!strcmp(text, "COMPLETED")) {/* use completed time */
                     per->stime = per->ctime;
                     per->etime = icaltime_add(per->stime, per->duration);
                }
                break; /* for loop done */
            }
        }
    }
}

/* this works in UTC times */
static struct icaltimetype count_first_alarm_time(xfical_period per
        , struct icaldurationtype dur
        , icalparameter_related rel) 
{
    struct icaltimetype alarm_time;

    if (icaltime_is_date(per.stime)) { 
/* HACK: convert to local time so that we can use time arithmetic
 * when counting alarm time. */
        if (rel == ICAL_RELATED_START) {
            per.stime.is_date       = 0;
            per.stime.is_daylight   = 0;
            per.stime.zone          = utc_icaltimezone;
            per.stime.hour          = 0;
            per.stime.minute        = 0;
            per.stime.second        = 0;
        }
        else {
            per.etime.is_date       = 0;
            per.etime.is_daylight   = 0;
            per.etime.zone          = utc_icaltimezone;
            per.etime.hour          = 0;
            per.etime.minute        = 0;
            per.etime.second        = 0;
        }
    }
    if (rel == ICAL_RELATED_END)
        alarm_time = icaltime_add(per.etime, dur);
    else /* default is ICAL_RELATED_START */
        alarm_time = icaltime_add(per.stime, dur);
    return(alarm_time);
}

/* this works in UTC times */
static struct icaltimetype count_next_alarm_time(struct icaltimetype start_time
        , struct icaldurationtype dur)
{
    struct icaltimetype alarm_time;

    if (icaltime_is_date(start_time)) { 
/* HACK: convert to UTC time so that we can use time arithmetic
 * when counting alarm time. */
        start_time.is_date       = 0;
        start_time.is_daylight   = 0;
        start_time.zone          = utc_icaltimezone;
        start_time.hour          = 0;
        start_time.minute        = 0;
        start_time.second        = 0;
    }
    alarm_time = icaltime_add(start_time, dur);
    return(alarm_time);
}

static gint exclude_order(gconstpointer a, gconstpointer b)
{
    return(icaltime_compare(((excluded_time *)a)->e_time
                          , ((excluded_time *)b)->e_time));
}

static icaltimezone *build_excluded_list_dtstart(icalcomponent *c) 
{
    const icaltimezone *zone=NULL;
    icalproperty *p = NULL;
    const char *text;
    struct icaltimetype itime;

    p = icalcomponent_get_first_property(c, ICAL_DTSTART_PROPERTY);
    if (p) {
        text = icalproperty_get_value_as_string(p);
        itime = icaltime_from_string(text);
        itime = ic_convert_to_timezone(itime, p);
        zone = itime.zone;
    }
    if (!zone) /* we have floating time, so let's use local timezone */
        zone = local_icaltimezone;

    return((icaltimezone *)zone);
}

static void build_excluded_list(GList **exclude_l, icalcomponent *comp
        , const icaltimezone *zone)
{
    icalproperty *exdate;
    struct icaltimetype exdatetime;
    excluded_time *l_etime;

    for (exdate = icalcomponent_get_first_property(comp,ICAL_EXDATE_PROPERTY);
         exdate != NULL;
         exdate = icalcomponent_get_next_property(comp,ICAL_EXDATE_PROPERTY)) {
        exdatetime = icalproperty_get_exdate(exdate);
        exdatetime = ic_convert_to_timezone(exdatetime, exdate);
        if (!exdatetime.zone) 
            icaltime_set_timezone(&exdatetime, zone);

        l_etime = g_new(excluded_time, 1);
        l_etime->e_time = exdatetime;
        *exclude_l = g_list_prepend(*exclude_l, l_etime);
    }
}

static void free_exclude_time(gpointer e_t, G_GNUC_UNUSED gpointer dummy)
{
    g_free(e_t);
}

static void free_excluded_list(GList *exclude_l)
{

    g_list_foreach(exclude_l, free_exclude_time, NULL);
    g_list_free(exclude_l);
    exclude_l = NULL;
}

static gboolean time_is_excluded(GList *exclude_l, struct icaltimetype *time)
{
    GList *e_l=NULL;
    excluded_time *excl_time;
    struct icaltimetype e_time;

    if (exclude_l == NULL || time == NULL || icaltime_is_null_time(*time))
        return 0;

    for (e_l = g_list_first(exclude_l); e_l != NULL; e_l = g_list_next(e_l)) {
        excl_time = (excluded_time *)e_l->data;
        e_time = excl_time->e_time;
        if ((icaltime_is_date(e_time)
             && icaltime_compare_date_only(e_time, *time) == 0)
            || (icaltime_compare(e_time, *time) == 0))
            return(1);
        else if ((icaltime_is_date(e_time)
                  && icaltime_compare_date_only(e_time, *time) > 0)
                 || (icaltime_compare(e_time, *time) > 0))
        /* remember that it is sorted list, so when e_time is bigger than
           searched time, we know that we did not find a match */
            return(0);
    }
    return(0); /* end of list reached */
}

/* let's find the trigger and check that it is active.
 * return new alarm struct if alarm is active and NULL if it is not
 * FIXME: We assume all alarms have similar trigger, which 
 * may not be true for other than Orage appointments
 */
static alarm_struct *process_alarm_trigger(icalcomponent *c
        , icalcomponent *ca, struct icaltimetype cur_time, int *cnt_repeat)
{ /* c == main component; ca == alarm component */
    icalproperty *p, *rdate;
    struct icaltriggertype trg;
    icalparameter *trg_related_par;
    icalparameter_related rel;
    struct icalrecurrencetype rrule;
    icalrecur_iterator* ri;
    xfical_period per;
    struct icaltimetype next_alarm_time, next_start_time, next_end_time, 
                        rdate_alarm_time;
    gboolean trg_active = FALSE;
    alarm_struct *new_alarm;
    struct icaldurationtype alarm_start_diff;
    struct icaldatetimeperiodtype rdate_period;
    gchar *tmp1, *tmp2;
    GList *excluded_list = NULL;
    icaltimezone *dtstart_zone;
    /* pvl_elem property_iterator;   */ /* for saving the iterator */

    p = icalcomponent_get_first_property(ca, ICAL_TRIGGER_PROPERTY);
    if (!p) {
        g_warning ("%s: Trigger not found", G_STRFUNC);
        return(NULL);
    }
    trg = icalproperty_get_trigger(p);
    trg_related_par = icalproperty_get_first_parameter(p
            , ICAL_RELATED_PARAMETER);
    if (trg_related_par)
        rel = icalparameter_get_related(trg_related_par);
    else
        rel = ICAL_RELATED_START;
    per = ic_get_period(c, TRUE);
    next_alarm_time = count_first_alarm_time(per, trg.duration, rel);
    alarm_start_diff = icaltime_subtract(next_alarm_time, per.stime);
    /* Due to the hack in date time calculation in count_first_alarm_time,
       we need to set next_alarm_time to local timezone so that 
       icaltime_compare works. Fix for Bug 8525
     */
    if (icaltime_is_date(per.stime)) {
        if (local_icaltimezone != utc_icaltimezone) {
            next_alarm_time.is_daylight   = 0;
            next_alarm_time.zone          = local_icaltimezone;
        }
    }

    /* we only have ctime for TODOs and only if todo has been completed.
     * So if we have ctime, we need to compare against it instead of 
     * current date */
    if (per.ikind == ICAL_VTODO_COMPONENT) {
        if (icaltime_is_null_time(per.ctime)
        || icaltime_compare(per.ctime, per.stime) < 0) {
        /* VTODO is never completed  */
        /* or it has completed before start, so
         * this one is not done and needs to be counted */
            if (icaltime_compare(cur_time, next_alarm_time) <= 0) { /* active */
                trg_active = TRUE;
            }
        }
    }
    else if (icaltime_compare(cur_time, next_alarm_time) <= 0) { /* active */
        trg_active = TRUE;
    }

    if (!trg_active 
    && (p = icalcomponent_get_first_property(c, ICAL_RRULE_PROPERTY)) != 0) { 
        /*
icalproperty *exdate;
struct icaltimetype exdatetime;
         */
        /* check recurring EVENTs */
        dtstart_zone = build_excluded_list_dtstart(c);
        build_excluded_list(&excluded_list, c, dtstart_zone);
        excluded_list = g_list_sort(excluded_list, exclude_order);
        rrule = icalproperty_get_rrule(p);
        set_todo_times(c, &per); /* may change per.stime to be per.ctime */
        next_alarm_time = count_first_alarm_time(per, trg.duration, rel);
        alarm_start_diff = icaltime_subtract(next_alarm_time, per.stime);
        ri = icalrecur_iterator_new(rrule, per.stime);
        for (next_start_time = icalrecur_iterator_next(ri),
             next_alarm_time = count_next_alarm_time(next_start_time
                    , alarm_start_diff);
             !icaltime_is_null_time(next_start_time)
             && ((per.ikind == ICAL_VTODO_COMPONENT
                  && icaltime_compare(next_start_time, per.ctime) <= 0)
                 || (per.ikind != ICAL_VTODO_COMPONENT 
                     && icaltime_compare(next_alarm_time, cur_time) <= 0)
                 || time_is_excluded(excluded_list, &next_start_time)
                 );
            next_start_time = icalrecur_iterator_next(ri),
            next_alarm_time = count_next_alarm_time(next_start_time
                    , alarm_start_diff)) {
            /* we loop = search next active alarm time as long as 
             * next_start_time is not null = real start time still found
             * and if TODO and next_start_time before complete time
             * or if not TODO (= EVENT) and next_alarm_time before current time
             */
            (*cnt_repeat)++;
        }
        icalrecur_iterator_free(ri);
        /* Due to the hack in date time calculation in count_first_alarm_time,
           we need to set next_alarm_time to local timezone so that 
           icaltime_compare works. Fix for Bug 8525
         */
        if (icaltime_is_date(per.stime)) {
            if (local_icaltimezone != utc_icaltimezone) {
                next_alarm_time.is_daylight   = 0;
                next_alarm_time.zone          = local_icaltimezone;
            }
        }
        if (icaltime_compare(cur_time, next_alarm_time) <= 0) {
            trg_active = TRUE;
        }
        free_excluded_list(excluded_list);
    }

    if (!trg_active)
        next_alarm_time = icaltime_null_time();

    /* this is used to convert rdate to local time. We need to read it here
     * so that it does not break the loop handling since it resets the
     * iterator */
    p = icalcomponent_get_first_property(c, ICAL_DTSTART_PROPERTY);
    next_start_time = icalproperty_get_dtstart(p);
    for (rdate = icalcomponent_get_first_property(c, ICAL_RDATE_PROPERTY);
         rdate != NULL;
         rdate = icalcomponent_get_next_property(c, ICAL_RDATE_PROPERTY)) {
        rdate_period = icalproperty_get_rdate(rdate);
        /* we only support normal times (not periods etc) */
        if (!icalperiodtype_is_null_period(rdate_period.period)) {
            g_message ("%s: Orage does not support rdate periods, but only "
                       "simple rdate. Ignoring %s", G_STRFUNC,
                       (char *)icalproperty_get_value_as_string(rdate));
            continue;
        }

        g_debug ("%s: found RDATE exception in alarm handling (%s)",
                 G_STRFUNC, icalproperty_get_value_as_string(rdate));

        if (icaltime_is_null_time(rdate_period.time))
            continue;

        /* it has same timezone than startdate, convert to local time */
        /* FIXME: this should not convert, but just set the timezone */
        if (icaltime_is_utc(next_start_time)) {
            /* FIXME: tarkista että convert_to_local_timezone toimii oikein
             * UTC:llä. se ei toimi kuten tässä */
            rdate_period.time = icaltime_convert_to_zone(rdate_period.time, utc_icaltimezone);
            rdate_period.time = icaltime_convert_to_zone(rdate_period.time, local_icaltimezone);
        }
        else {
            rdate_period.time = convert_to_local_timezone(rdate_period.time, p);
        }

        /* we still need to convert it from start time to alarm time */
        rdate_alarm_time = icaltime_add(rdate_period.time, alarm_start_diff);
        if (icaltime_compare(cur_time, rdate_alarm_time) <= 0) {
            /* this alarm is still active */
            /* save the iterator since excluded check changes it */
            /* FIXME: how to store the iterator??
            property_iterator = c->property_iterator;
            if (!icalproperty_recurrence_is_excluded(c
                     , &per.stime, &rdate_period.time)) {
                     */
                /* not excluded, so it really is active */
                trg_active = TRUE;
                if (icaltime_compare(rdate_alarm_time, next_alarm_time) < 0
                || icaltime_is_null_time(next_alarm_time)) {
                /* it is earlier than our previous alarm, so we use this */
                    next_alarm_time = rdate_alarm_time;
                }
            /*
            }
            c->property_iterator = property_iterator;
            */
        }
    }

    if (trg_active) {
        new_alarm = orage_alarm_new ();
        /* If we had a date, we now have the time already in local time and
           no conversion is needed. This is due to the hack in date time
           calculation in count_first_alarm_time. We just need to set it to
           local timezone.
           If it is normal time we need to convert it to local.
         */
        if (icaltime_is_date(per.stime)) {
            if (local_icaltimezone != utc_icaltimezone) {
                next_alarm_time.is_daylight   = 0;
                next_alarm_time.zone          = local_icaltimezone;
            }
        }
        else
            next_alarm_time = icaltime_convert_to_zone(next_alarm_time, local_icaltimezone);

        new_alarm->alarm_time = icaltimetype_to_gdatetime (next_alarm_time);
    /* alarm_start_diff goes from start to alarm, so we need to revert it
     * here since now we need to get the start time from alarm. */
        if (alarm_start_diff.is_neg)
            alarm_start_diff.is_neg = 0;
        else 
            alarm_start_diff.is_neg = 1;

        next_start_time = icaltime_add(next_alarm_time, alarm_start_diff);
        next_end_time = icaltime_add(next_start_time, per.duration);
        tmp1 = icaltime_to_i18_time (
                        icaltime_as_ical_string(next_start_time));
        tmp2 = icaltime_to_i18_time (
                        icaltime_as_ical_string(next_end_time));
        new_alarm->action_time = g_strconcat(tmp1, " - ", tmp2, NULL);
        g_free(tmp1);
        g_free(tmp2);

        return(new_alarm);
    }
    else {
        return(NULL);
    }
}

static void process_alarm_data(icalcomponent *ca, alarm_struct *new_alarm)
{
    xfical_appt *appt;
    /*
    icalproperty *p;:
    enum icalproperty_action act;
    icalattach *attach = NULL;
    struct icaldurationtype duration;
    char *text;
    int i;
    */

    appt = xfical_appt_alloc();
    get_alarm_data(ca, appt);

    new_alarm->persistent = appt->alarm_persistent;
    if (appt->display_alarm_orage || appt->display_alarm_notify) {
        new_alarm->display_orage = appt->display_alarm_orage;
        new_alarm->display_notify = appt->display_alarm_notify;
        new_alarm->notify_timeout = appt->display_notify_timeout;
        /* FIXME: use real alarm data */
        /* 
        if (ORAGE_STR_EXISTS(appt->note))
            new_alarm->description = orage_process_text_commands(appt->note);
            */
    }
    else if (appt->sound_alarm) {
        new_alarm->audio = appt->sound_alarm;
        if (ORAGE_STR_EXISTS(appt->sound))
            new_alarm->sound = g_strdup(appt->sound);
        if (appt->soundrepeat) {
            new_alarm->repeat_cnt = appt->soundrepeat_cnt;
            new_alarm->repeat_delay = appt->soundrepeat_len;
        }
    }
    else if(appt->procedure_alarm) {
        new_alarm->procedure = appt->procedure_alarm;
        if (ORAGE_STR_EXISTS(appt->procedure_cmd)) {
            new_alarm->cmd = g_strconcat(appt->procedure_cmd, " "
                    , appt->procedure_params, NULL);
        }
    }

    /* this really is g_free only instead of xfical_appt_free 
     * since get_alarm_data does not allocate more memory.
     * It is hack to do it faster. */
    g_free(appt); 
#if 0
    p = icalcomponent_get_first_property(ca, ICAL_ACTION_PROPERTY);
    if (!p) {
        g_warning ("%s: No ACTION in alarm. Ignoring this ALARM.", G_STRFUNC);
        return;
    }
    act = icalproperty_get_action(p);
    if (act == ICAL_ACTION_DISPLAY) {
        new_alarm->display = TRUE;
        p = icalcomponent_get_first_property(ca, ICAL_DESCRIPTION_PROPERTY);
        if (p)
            new_alarm->description = g_string_new(
                    (char *)icalproperty_get_description(p));
        for (p = icalcomponent_get_first_property(ca, ICAL_X_PROPERTY);
             p != 0;
             p = icalcomponent_get_next_property(ca, ICAL_X_PROPERTY)) {
            text = (char *)icalproperty_get_x_name(p);
            if (!strcmp(text, "X-ORAGE-DISPLAY-ALARM")) {
                text = (char *)icalproperty_get_value_as_string(p);
                if (!strcmp(text, "ORAGE")) {
                    new_alarm->display_orage = TRUE;
                }
                else if (!strcmp(text, "NOTIFY")) {
                    new_alarm->display_notify = TRUE;
                }
            }
            else if (!strcmp(text, "X-ORAGE-NOTIFY-ALARM-TIMEOUT")) {
                text = (char *)icalproperty_get_value_as_string(p);
                sscanf(text, "%d", &i);
                new_alarm->notify_timeout = i;
            }
            else {
                g_warning ("%s: unknown X property %s", G_STRFUNC, text);
            }
        }
    }
    else if (act == ICAL_ACTION_AUDIO) {
        new_alarm->audio = TRUE;
        p = icalcomponent_get_first_property(ca, ICAL_ATTACH_PROPERTY);
        if (p) {
            attach = icalproperty_get_attach(p);
            text = (char *)icalattach_get_url(attach);
            if (text)
                new_alarm->sound = g_string_new(text);
        }
        p = icalcomponent_get_first_property(ca, ICAL_REPEAT_PROPERTY);
        if (p)
            new_alarm->repeat_cnt = icalproperty_get_repeat(p);
        p = icalcomponent_get_first_property(ca, ICAL_DURATION_PROPERTY);
        if (p) {
            duration = icalproperty_get_duration(p);
            new_alarm->repeat_delay = icaldurationtype_as_int(duration);
        }
    }
    else if (act == ICAL_ACTION_PROCEDURE) {
        new_alarm->procedure = TRUE;
        p = icalcomponent_get_first_property(ca, ICAL_ATTACH_PROPERTY);
        if (p) {
            attach = icalproperty_get_attach(p);
            text = (char *)icalattach_get_url(attach);
            if (text) {
                new_alarm->cmd = g_string_new(text);
                p = icalcomponent_get_first_property(ca
                        , ICAL_DESCRIPTION_PROPERTY);
                if (p)
                    new_alarm->cmd = g_string_append(new_alarm->cmd
                            , (char *)icalproperty_get_description(p));
            }
        }
    }
    else {
        g_warning ("%s: Unknown ACTION (%d) in alarm. Ignoring ALARM.",
                   G_STRFUNC, act);
        return;
    }
#endif
}

static void xfical_alarm_build_list_internal_real(gboolean first_list_today
        , icalcomponent *base, char *file_type, char *file_name)
{
    icalcomponent *c, *ca;
    struct icaltimetype cur_time;
    char *suid;
    gboolean trg_processed = FALSE, trg_active = FALSE;
    gint cnt_alarm=0, cnt_repeat=0, cnt_event=0, cnt_act_alarm=0
        , cnt_alarm_add=0;
    icalcompiter ci;
    alarm_struct *new_alarm = NULL;

    cur_time = icaltime_current_time_with_zone(utc_icaltimezone);

    for (c = icalcomponent_get_first_component(base, ICAL_ANY_COMPONENT);
            c != 0;
            c = icalcomponent_get_next_component(base, ICAL_ANY_COMPONENT)) {
        cnt_event++;
        trg_processed = FALSE;
        trg_active = FALSE;
        for (ci = icalcomponent_begin_component(c, ICAL_VALARM_COMPONENT);
                icalcompiter_deref(&ci) != 0;
                icalcompiter_next(&ci)) {
            ca = icalcompiter_deref(&ci);
            cnt_alarm++;
            if (!trg_processed) {
                trg_processed = TRUE;
                new_alarm = process_alarm_trigger(c, ca, cur_time, &cnt_repeat);
                if (new_alarm) {
                    trg_active = TRUE;
                    suid = (char *)icalcomponent_get_uid(c);
                    new_alarm->uid = g_strconcat(file_type, suid, NULL);
                    new_alarm->title = orage_process_text_commands(
                            (char *)icalcomponent_get_summary(c));
                    new_alarm->description = orage_process_text_commands(
                            (char *)icalcomponent_get_description(c));
                }
            }
            if (trg_active) {
                cnt_act_alarm++;
                process_alarm_data(ca, new_alarm);
            }
#if 0
            if (trg_processed) {
                if (trg_active) {
                    cnt_act_alarm++;
                    process_alarm_data(ca, new_alarm);
                }
            }
            else {
                g_warning ("%s: Found alarm without trigger %s. Skipping it",
                           G_STRFUNC, icalcomponent_get_uid(c));
            }
#endif
        }  /* ALARM */
        if (trg_active) {
            alarm_add(new_alarm);
            /*
            g_message ("new alarm: alarm:%s action:%s title:%s\n"
            , new_alarm->alarm_time, new_alarm->action_time, new_alarm->title);
            */
            cnt_alarm_add++;
        }
    }  /* COMPONENT */
    if (first_list_today) {
        if (strcmp(file_type, "O00.") == 0)
            g_message ("Created alarm list for main Orage file");
        else 
            g_message ("Created alarm list for foreign file: %s (%s)",
                       file_name, file_type);

        g_message ("Added %d alarms. Processed %d events.", cnt_alarm_add,
                   cnt_event);

        g_message ("Found %d alarms of which %d are active. "
                   "(Searched %d recurring alarms.)", cnt_alarm, cnt_act_alarm,
                   cnt_repeat);
    }
}

static void xfical_alarm_build_list_internal(gboolean first_list_today)
{
    OrageApplication *app;
    gchar file_type[8];
    gint i;

    /* first remove all old alarms by cleaning the whole structure */
    alarm_list_free();

    /* first search base orage file */
    g_strlcpy (file_type, "O00.", sizeof (file_type));
    xfical_alarm_build_list_internal_real(first_list_today, ic_ical, file_type
            , NULL);
    /* then process all foreign files */
    for (i = 0; i < g_par.foreign_count; i++) {
        g_snprintf(file_type, sizeof (file_type), "F%02d.", i);
        xfical_alarm_build_list_internal_real(first_list_today
                , ic_f_ical[i].ical, file_type, g_par.foreign_data[i].name);
    }
    setup_orage_alarm_clock(); /* keep reminders upto date */

    /* Refresh main calendar window lists. */
    app = ORAGE_APPLICATION (g_application_get_default ());
    orage_window_build_info (ORAGE_WINDOW (orage_application_get_window (app)));
}

void xfical_alarm_build_list(gboolean first_list_today)
{
    if (xfical_file_open(TRUE)) {
        xfical_alarm_build_list_internal(first_list_today);
        xfical_file_close(TRUE);
    }
}

 /* Read next EVENT/TODO/JOURNAL component on the specified date from 
  * ical datafile.
  * a_day:  start date of ical component which is to be read
  * first:  get first appointment if TRUE, if not get next.
  * days:   how many more days to check forward. 0 = only one day
  * type:   EVENT/TODO/JOURNAL to be read
  * returns: NULL if failed and xfical_appt pointer to xfical_appt struct 
  *          filled with data if successfull. 
  *          You need to deallocate it after used.
  * Note:   starttimecur and endtimecur are converted to local timezone
  */
static xfical_appt *xfical_appt_get_next_on_day_internal (const gchar *a_day
        , gboolean first, gint days, xfical_type type, icalcomponent *base
        , gchar *file_type)
{
    struct icaltimetype asdate, aedate    /* period to check */
            , nsdate, nedate;   /* repeating event occurrency start and end */
    xfical_period per; /* event start and end times with duration */
    icalcomponent *c=NULL;
    icalproperty *p=NULL;
    static icalcompiter ci;
    gboolean date_found=FALSE;
    gboolean recurrent_date_found=FALSE;
    char *uid;
    xfical_appt *appt;
    struct icalrecurrencetype rrule;
    icalrecur_iterator* ri;
    icalcomponent_kind ikind = ICAL_VEVENT_COMPONENT;
    const gchar *start_str;
    const gchar *end_str;

    /* setup period to test */
    asdate = icaltime_from_string(a_day);
    aedate = asdate;
    if (days)
        icaltime_adjust(&aedate, days, 0, 0, 0);

    if (first) {
        if (type == XFICAL_TYPE_EVENT)
            ikind = ICAL_VEVENT_COMPONENT;
        else if (type == XFICAL_TYPE_TODO)
            ikind = ICAL_VTODO_COMPONENT;
        else if (type == XFICAL_TYPE_JOURNAL)
            ikind = ICAL_VJOURNAL_COMPONENT;
        else
            g_critical ("%s: Unsupported Type", G_STRFUNC);

        ci = icalcomponent_begin_component(base, ikind);
    }
    for ( ; 
         icalcompiter_deref(&ci) != 0 && !date_found;
         icalcompiter_next(&ci)) {
        /* next appointment loop. check if it is ok */
        c = icalcompiter_deref(&ci);
        per = ic_get_period(c, TRUE);
        if (type == XFICAL_TYPE_TODO) {
            set_todo_times(c, &per); /* may change per.stime to be per.ctime */
            if (icaltime_is_null_time(per.ctime)
            || local_compare(per.ctime, per.stime) < 0)
            /* VTODO is never completed  */
            /* or it has completed before start, so
             * this one is not done and needs to be counted 
             * unless it is excluded */
                if (!icalproperty_recurrence_is_excluded(c
                            , &per.stime, &per.stime))
                    date_found = TRUE;
        }
        else { /* VEVENT or VJOURNAL */
            if (local_compare_date_only(per.stime, aedate) <= 0
            &&  local_compare_date_only(asdate, per.etime) <= 0
            && !icalproperty_recurrence_is_excluded(c, &per.stime, &asdate)) {
                    date_found = TRUE;
            }
        }
        if (!date_found && (p = icalcomponent_get_first_property(c
                , ICAL_RRULE_PROPERTY)) != 0) { /* check recurring */
            nsdate = icaltime_null_time();
            rrule = icalproperty_get_rrule(p);
            ri = icalrecur_iterator_new(rrule, per.stime);
            for (nsdate = icalrecur_iterator_next(ri),
                    nedate = icaltime_add(nsdate, per.duration);
                 !icaltime_is_null_time(nsdate)
                 && ((type == XFICAL_TYPE_TODO
                        && local_compare(nsdate, per.ctime) <= 0)
                     || (type != XFICAL_TYPE_TODO
                         && local_compare_date_only(nedate, asdate) < 0)
                     || icalproperty_recurrence_is_excluded(c
                            , &per.stime, &nsdate));
                 nsdate = icalrecur_iterator_next(ri),
                    nedate = icaltime_add(nsdate, per.duration)) {}
            icalrecur_iterator_free(ri);
            if (type == XFICAL_TYPE_TODO) {
                if (!icaltime_is_null_time(nsdate)) {
                    date_found = TRUE;
                    recurrent_date_found = TRUE;
                }
            }
            else if (local_compare_date_only(nsdate, aedate) <= 0
                && local_compare_date_only(asdate, nedate) <= 0) {
                date_found = TRUE;
                recurrent_date_found = TRUE;
            }
        }
    }
    if (date_found) {
        if ((uid = (char *)icalcomponent_get_uid(c)) == NULL) {
            g_critical ("%s: File %s has component without UID. File is "
                        "either corrupted or not compatible with Orage, "
                        "skipping this file", G_STRFUNC, file_type);
            return(0);
        }
        appt = appt_get_any(uid, base, file_type);
        if (recurrent_date_found) {
            start_str = icaltime_as_ical_string (nsdate);
            end_str = icaltime_as_ical_string(nedate);
        }
        else {
            start_str = icaltime_as_ical_string (per.stime);
            end_str = icaltime_as_ical_string (per.etime);
        }

        orage_gdatetime_unref (appt->starttimecur);
        appt->starttimecur = orage_icaltime_to_gdatetime (start_str);
        orage_gdatetime_unref (appt->endtimecur);
        appt->endtimecur = orage_icaltime_to_gdatetime (end_str);

        return(appt);
    }
    else
        return(0);
}

xfical_appt *xfical_appt_get_next_on_day (GDateTime *gdt, gboolean first,
                                          gint days, xfical_type type,
                                          gchar *file_type)
{
    gint i;
    gchar *a_day;
    xfical_appt *appt;

    a_day = orage_gdatetime_to_icaltime (gdt, TRUE);

    /* FIXME: old code called, replace with xfical_get_each_app_within_time. */
    if (file_type[0] == 'O') {
        appt = xfical_appt_get_next_on_day_internal (a_day, first, days, type,
                                                     ic_ical, file_type);
    }
#ifdef HAVE_ARCHIVE
    else if (file_type[0] == 'A') {
        appt = xfical_appt_get_next_on_day_internal (a_day, first, days, type,
                                                     ic_aical, file_type);
    }
#endif
    else if (file_type[0] == 'F') {
        sscanf(file_type, "F%02d", &i);
        if (i < g_par.foreign_count && ic_f_ical[i].ical != NULL)
        {
            appt = xfical_appt_get_next_on_day_internal (a_day, first, days,
                    type, ic_f_ical[i].ical, file_type);
        }
        else
        {
            g_critical ("%s: unknown foreign file number %s", G_STRFUNC,
                        file_type);
            appt = NULL;
        }
    }
    else {
        g_critical ("%s: unknown file type", G_STRFUNC);
        appt = NULL;
    }

    g_free (a_day);

    return appt;
}

static gboolean xfical_mark_calendar_days(GtkCalendar *gtkcal
        , int cur_year, int cur_month
        , int s_year, int s_month, int s_day
        , int e_year, int e_month, int e_day)
{
    gint start_day, day_cnt, end_day;
    gboolean marked = FALSE;

    if ((s_year*12+s_month) <= (cur_year*12+cur_month)
    &&  (cur_year*12+cur_month) <= (e_year*12+e_month)) {
        /* event is in our year+month = visible in calendar */
        if (s_year == cur_year && s_month == cur_month)
            start_day = s_day;
        else
            start_day = 1;
        if (e_year == cur_year && e_month == cur_month)
            end_day = e_day;
        else { /* to the end of this month */
            guint monthdays[12] = 
                    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

            if (((cur_year%4) == 0) && (cur_month == 2)
            && (((cur_year%100) != 0) || ((cur_year%400) == 0)))
                ++monthdays[1]; /* we are on February leap year so 29 days */

            end_day = monthdays[cur_month-1]; /* monthdays is 0...11 */
        }
        for (day_cnt = start_day; day_cnt <= end_day; day_cnt++) {
            gtk_calendar_mark_day(gtkcal, day_cnt);
            marked = TRUE;
        }
    }
    return(marked);
}

/* note that this not understand timezones, but gets always raw time,
 * which we need to convert to correct timezone */
static void mark_calendar (G_GNUC_UNUSED icalcomponent *c,
                           icaltime_span *span,
                           void *data)
{
    struct icaltimetype sdate, edate;
    mark_calendar_data *cal_data;
    gchar *str;
    GDateTime *gdt_start;
    GDateTime *gdt_end;
    GDateTime *gdt_tmp;

    /*FIXME: find times from the span */
    cal_data = (mark_calendar_data *)data;

    gdt_start = g_date_time_new_from_unix_utc (span->start);
    gdt_end = g_date_time_new_from_unix_utc (span->end);

    /* check bug 7886 explanation in function add_appt_to_list */
    if (cal_data->appt.allDay && !cal_data->appt.use_duration) {
        gdt_tmp = gdt_end;
        gdt_end = g_date_time_add_days (gdt_tmp, -1);
        g_date_time_unref (gdt_tmp);
    }

    str = orage_gdatetime_to_icaltime (gdt_start, FALSE);
    sdate = icaltime_from_string (str);
    g_free (str);

    str = orage_gdatetime_to_icaltime (gdt_end, FALSE);
    edate = icaltime_from_string (str);
    g_free (str);

    if (cal_data->appt.freq != XFICAL_FREQ_HOURLY
    &&  g_date_time_get_hour (gdt_start) != cal_data->orig_start_hour) {
        /* WHEN we arrive here, libical has done an extra UTC conversion,
          which we need to undo */
        sdate = convert_to_zone(sdate, "UTC");
        edate = convert_to_zone(edate, "UTC");
    }
    else {
        sdate = convert_to_zone(sdate, cal_data->appt.start_tz_loc);
        edate = convert_to_zone(edate, cal_data->appt.end_tz_loc);
    }
    g_date_time_unref (gdt_start);
    g_date_time_unref (gdt_end);
    sdate = icaltime_convert_to_zone(sdate, local_icaltimezone);
    edate = icaltime_convert_to_zone(edate, local_icaltimezone);

    /* fix for bug 8508 prevent showing extra day in calendar.
       Only has effect when end date is midnight */
    icaltime_adjust(&edate, 0, 0, 0, -1);

    (void)xfical_mark_calendar_days (cal_data->cal, cal_data->year,
                                     cal_data->month, sdate.year, sdate.month,
                                     sdate.day, edate.year, edate.month,
                                     edate.day);
}

 /* Mark days from appointment c into calendar
  * year: Year to be searched
  * month: Month to be searched
  */
static void xfical_mark_calendar_from_component (GtkCalendar *gtkcal,
                                                 icalcomponent *c,
                                                 guint year,
                                                 guint month)
{
    xfical_period per;
    struct icaltimetype nsdate, nedate;
    struct icalrecurrencetype rrule;
    icalrecur_iterator* ri;
    icalproperty *p = NULL;
    gboolean marked;
    icalcomponent_kind kind;
    gchar *tmp;
    struct icaltimetype start;
    mark_calendar_data cal_data;

    /* Note that all VEVENTS are marked, but only the first VTODO
     * end date is marked */
    kind = icalcomponent_isa(c);
    if (kind == ICAL_VEVENT_COMPONENT) {
        tmp = g_strdup_printf("%04d%02d01T000000", year, month);
        nsdate = icaltime_from_string(tmp);
        g_free(tmp);
        nedate = nsdate;
        nedate.month++; 
        if ((unsigned int)nedate.month > 12) { /* next year */
            nedate.month = 1;
            nedate.year++;
        }
#if 0
        icaltime_adjust(&nedate, 0, 0, 0, 0);
#endif
        /* FIXME: we read the whole appointent just to get start and end 
         * timezones for mark_calendar. too heavy? */
        /* 1970 check due to bug 9507 */
        p = icalcomponent_get_first_property(c, ICAL_DTSTART_PROPERTY);
        start = icalproperty_get_dtstart(p);
        if (start.year >= 1970) {
            cal_data.cal = gtkcal;
            cal_data.year = year;
            cal_data.month = month;
            (void)get_appt_from_icalcomponent(c, &cal_data.appt);
        /* BUG 7929. If calendar file contains same timezone definition than
           what the time is in, libical returns wrong time in span.
           But as the hour only changes with HOURLY repeating appointments,
           we can replace received hour with the hour from start time */
#if 0
            p = icalcomponent_get_first_property(c, ICAL_DTEND_PROPERTY);
            start = icalproperty_get_dtend(p);
            cal_data.orig_end_hour = start.hour;
#endif
            p = icalcomponent_get_first_property(c, ICAL_DTSTART_PROPERTY);
            start = icalproperty_get_dtstart(p);
            cal_data.orig_start_hour = start.hour;
            icalcomponent_foreach_recurrence(c, nsdate, nedate, mark_calendar
                    , (void *)&cal_data);
            g_free(cal_data.appt.categories);
            orage_gdatetime_unref (cal_data.appt.starttime);
            orage_gdatetime_unref (cal_data.appt.endtime);
        }
        else {
            per = ic_get_period(c, TRUE);
            (void)xfical_mark_calendar_days (gtkcal, year, month,
                                             per.stime.year, per.stime.month,
                                             per.stime.day, per.etime.year,
                                             per.etime.month, per.etime.day);
            if ((p = icalcomponent_get_first_property(c
                    , ICAL_RRULE_PROPERTY)) != 0) {
                nsdate = icaltime_null_time();
                rrule = icalproperty_get_rrule(p);
                ri = icalrecur_iterator_new(rrule, per.stime);
                for (nsdate = icalrecur_iterator_next(ri),
                        nedate = icaltime_add(nsdate, per.duration);
                     !icaltime_is_null_time(nsdate)
                        && (nsdate.year*12+nsdate.month) <= (int)(year*12+month);
                     nsdate = icalrecur_iterator_next(ri),
                        nedate = icaltime_add(nsdate, per.duration)) {
                    if (!icalproperty_recurrence_is_excluded(c, &per.stime
                                , &nsdate))
                        (void)xfical_mark_calendar_days (gtkcal, year, month,
                                                         nsdate.year,
                                                         nsdate.month,
                                                         nsdate.day,
                                                         nedate.year,
                                                         nedate.month,
                                                         nedate.day);
                }
                icalrecur_iterator_free(ri);
            } 
        }
    } /* ICAL_VEVENT_COMPONENT */
    else if (kind == ICAL_VTODO_COMPONENT) {
        per = ic_get_period(c, TRUE);
        marked = FALSE;
        if (icaltime_is_null_time(per.ctime)
        || (local_compare(per.ctime, per.stime) < 0)) {
            /* VTODO needs to be checked either if it never completed 
             * or it has completed before start */
            marked = xfical_mark_calendar_days(gtkcal, year, month
                    , per.etime.year, per.etime.month, per.etime.day
                    , per.etime.year, per.etime.month, per.etime.day);
        }
        if (!marked && (p = icalcomponent_get_first_property(c
                , ICAL_RRULE_PROPERTY)) != 0) { 
            /* check recurring TODOs */
            nsdate = icaltime_null_time();
            rrule = icalproperty_get_rrule(p);
            set_todo_times(c, &per);/* may change per.stime to per.ctime */
            ri = icalrecur_iterator_new(rrule, per.stime);
            for (nsdate = icalrecur_iterator_next(ri);
                 !icaltime_is_null_time(nsdate)
                    && (((nsdate.year*12+nsdate.month) <= (int)(year*12+month)
                        && (local_compare(nsdate, per.ctime) <= 0))
                        || icalproperty_recurrence_is_excluded(c, &per.stime
                        , &nsdate));
                 nsdate = icalrecur_iterator_next(ri)) {
                /* find the active one like in 
                 * xfical_appt_get_next_on_day_internal */
            }
            icalrecur_iterator_free(ri);
            if (!icaltime_is_null_time(nsdate)) {
                nedate = icaltime_add(nsdate, per.duration);
                (void)xfical_mark_calendar_days (gtkcal, year, month,
                                                 nedate.year, nedate.month,
                                                 nedate.day, nedate.year,
                                                 nedate.month, nedate.day);
            }
        } 
    } /* ICAL_VTODO_COMPONENT */
}

void xfical_mark_calendar_recur(GtkCalendar *gtkcal, const xfical_appt *appt)
{
    guint year, month;
    icalcomponent_kind ikind = ICAL_VEVENT_COMPONENT;
    icalcomponent *icmp;

    gtk_calendar_get_date(gtkcal, &year, &month, NULL);
    gtk_calendar_clear_marks(gtkcal);
    if (appt->type == XFICAL_TYPE_EVENT)
        ikind = ICAL_VEVENT_COMPONENT;
    else if (appt->type == XFICAL_TYPE_TODO)
        ikind = ICAL_VTODO_COMPONENT;
    else if (appt->type == XFICAL_TYPE_JOURNAL)
        ikind = ICAL_VJOURNAL_COMPONENT;
    else
        g_critical ("%s: Unsupported Type", G_STRFUNC);

    icmp = icalcomponent_vanew(ikind
               , icalproperty_new_uid("RECUR_TEST")
               , NULL);
    appt_add_starttime_internal(appt, icmp);
    appt_add_endtime_internal(appt, icmp);
    appt_add_completedtime_internal(appt, icmp);
    appt_add_recur_internal(appt, icmp);
    appt_add_exception_internal(appt, icmp);
    xfical_mark_calendar_from_component(gtkcal, icmp, year, month+1);
    icalcomponent_free(icmp);
}

 /* Get all appointments from the file and mark calendar for EVENTs and TODOs
  */
static void xfical_mark_calendar_file (GtkCalendar *gtkcal, icalcomponent *base,
                                       guint year, guint month)
{
    icalcomponent *c;

    for (c = icalcomponent_get_first_component(base, ICAL_ANY_COMPONENT);
         c != 0;
         c = icalcomponent_get_next_component(base, ICAL_ANY_COMPONENT)) {
        xfical_mark_calendar_from_component(gtkcal, c, year, month);
    } 
}

void xfical_mark_calendar(GtkCalendar *gtkcal)
{
    gint i;
    guint year, month, day;

    gtk_calendar_get_date(gtkcal, &year, &month, &day);
    gtk_calendar_clear_marks(gtkcal);
    xfical_mark_calendar_file(gtkcal, ic_ical, year, month+1);
    for (i = 0; i < g_par.foreign_count; i++) {
        xfical_mark_calendar_file(gtkcal, ic_f_ical[i].ical, year, month+1);
    }
}

/* note that this not understand timezones, but gets always raw time,
 * which we need to convert to correct timezone */
static void add_appt_to_list(icalcomponent *c, icaltime_span *span , void *data)
{
    xfical_appt *appt;
    struct icaltimetype sdate, edate;
    struct tm end_tm;
    GDateTime *gdt_start;
    GDateTime *gdt_end;
    GDateTime *gdt_tmp;
    gchar *str;
    const gchar *i18_time;
    app_data *data1;
        /* Need to check that returned value is withing limits.
           Check more from BUG 5764 and 7886. */

    data1 = (app_data *)data;
    appt = g_new0(xfical_appt, 1);
    /* FIXME: we could check if UID is the same and only get all data
     * when UID changes. This seems to be fast enough as it is though */
    (void)get_appt_from_icalcomponent(c, appt);
    xfical_appt_get_fill_internal(appt, data1->file_type);
    gmtime_r(&span->end, &end_tm);
    gdt_start = g_date_time_new_from_unix_utc (span->start);
    gdt_end = g_date_time_new_from_unix_utc (span->end);

    /* BUG 7886. we are called with wrong span->end when we have full day
       event. This is libical bug and needs to be fixed properly later, but
       now I just work around it.
    FIXME: code whole loop correctly = function icalcomponent_foreach_recurrence
    */
    if (appt->allDay && !appt->use_duration) {
        gdt_tmp = gdt_end;
        gdt_end = g_date_time_add_days (gdt_tmp, -1);
        g_date_time_unref (gdt_tmp);
        /* now end is correct, but we still have been called wrongly on
           the last day */
    }

    /* end of bug workaround */
    /* FIXME: should we use interval instead ? */
    str = orage_gdatetime_to_icaltime (gdt_start, FALSE);
    sdate = icaltime_from_string (str);
    g_free (str);

    str = orage_gdatetime_to_icaltime (gdt_end, FALSE);
    edate = icaltime_from_string (str);
    g_free (str);

    /* BUG 7929. If calendar file contains same timezone definition than what
       the time is in, libical returns wrong time in span. But as the hour
       only changes with HOURLY repeating appointments, we can replace received
       hour with the hour from start time */
    if (appt->freq != XFICAL_FREQ_HOURLY
    &&  g_date_time_get_hour (gdt_start) != data1->orig_start_hour) {
        /* WHEN we arrive here, libical has done an extra UTC conversion,
           which we need to undo */
        sdate = convert_to_zone(sdate, "UTC");
        edate = convert_to_zone(edate, "UTC");
    }
    else {
        sdate = convert_to_zone(sdate, appt->start_tz_loc);
        edate = convert_to_zone(edate, appt->end_tz_loc);
    }

    g_date_time_unref (gdt_start);
    g_date_time_unref (gdt_end);

    sdate = icaltime_convert_to_zone(sdate, local_icaltimezone);
    edate = icaltime_convert_to_zone(edate, local_icaltimezone);

    i18_time = icaltime_as_ical_string (sdate);
    orage_gdatetime_unref (appt->starttimecur);
    appt->starttimecur = orage_icaltime_to_gdatetime (i18_time);

    i18_time = icaltime_as_ical_string (edate);
    g_date_time_unref (appt->endtimecur);
    appt->endtimecur = orage_icaltime_to_gdatetime (i18_time);
        /* Need to check that returned value is withing limits.
           Check more from BUG 5764 and 7886. */
    /* starttimecur and endtimecur are in local timezone. Compare that to
       limits, which are also localtimezone DATEs */
    if (g_date_time_compare (appt->endtimecur, data1->asdate) <= 0
     || g_date_time_compare (appt->starttimecur, data1->aedate) >= 0) {
        /* we do not need this. Free the memory */
        xfical_appt_free(appt);
    }
    else {/* add to list like with internal libical */
        *data1->list = g_list_prepend(*data1->list, appt);
    }
}

/* Fetch each appointment within the specified time period and add those
 * to the data GList. Note that each repeating appointment is real full 
 * appt */
static void xfical_get_each_app_within_time_internal (const gchar *a_day,
                                                      gint days,
                                                      xfical_type type,
                                                      icalcomponent *base,
                                                      const gchar *file_type,
                                                      GList **data)
{
    struct icaltimetype asdate, aedate;    /* period to check */
    icalcomponent *c=NULL;
    icalcomponent_kind ikind = ICAL_VEVENT_COMPONENT;
    icalcomponent *c2=NULL;
    icalproperty *p = NULL;
    struct icaltimetype start;
    app_data data1;

    /* setup period to test */
    asdate = icaltime_from_string(a_day);
    aedate = asdate;
    icaltime_adjust(&aedate, days, 0, 0, 0);

    if (type == XFICAL_TYPE_EVENT)
        ikind = ICAL_VEVENT_COMPONENT;
    else if (type == XFICAL_TYPE_TODO)
        ikind = ICAL_VTODO_COMPONENT;
    else if (type == XFICAL_TYPE_JOURNAL)
        ikind = ICAL_VJOURNAL_COMPONENT;
    else
        g_critical ("%s: Unsupported Type", G_STRFUNC);

    data1.list = data;
    data1.file_type = file_type;
        /* Need to check that returned value is withing limits.
           Check more from BUG 5764 and 7886. */
    data1.asdate = orage_icaltime_to_gdatetime (a_day);
    data1.aedate = icaltimetype_to_gdatetime (aedate);
    /* Hack for bug 8382: Take one more day earlier and later than needed
       due to UTC conversion. (And drop those days later then.) */
    icaltime_adjust(&asdate, -1, 0, 0, 0);
    icaltime_adjust(&aedate, 1, 0, 0, 0);
    for (c = icalcomponent_get_first_component(base, ikind);
         c != 0;
         c = icalcomponent_get_next_component(base, ikind)) {
        /* BUG 7929. If calendar file contains same timezone definition than
           what the time is in, libical returns wrong time in span.
           But as the hour only changes with HOURLY repeating appointments,
           we can replace received hour with the hour from start time */

        p = icalcomponent_get_first_property(c, ICAL_DTSTART_PROPERTY);
        start = icalproperty_get_dtstart(p);
        data1.orig_start_hour = start.hour;
        /* FIXME: hack to fix year to be newer than 1970 based on BUG 9507 */
        if (start.year < 1970) {
            c2 = icalcomponent_new_clone(c);
            p = icalcomponent_get_first_property(c2, ICAL_DTSTART_PROPERTY);
            start = icalproperty_get_dtstart(p);
            g_debug ("%s: Adjusting temporarily old DTSTART time %d", G_STRFUNC,
                     start.year);
            start.year = 1970;
            icalproperty_set_dtstart(p, start);
            icalcomponent_foreach_recurrence(c2, asdate, aedate
                    , add_appt_to_list, (void *)&data1);
        }
        /* FIXME: end of hack */
        else {
            icalcomponent_foreach_recurrence(c, asdate, aedate
                    , add_appt_to_list, (void *)&data1);
        }
    }

    g_date_time_unref (data1.asdate);
    g_date_time_unref (data1.aedate);
}

/* This will (probably) replace xfical_appt_get_next_on_day */
void xfical_get_each_app_within_time (GDateTime *a_day, const gint days,
                                      xfical_type type, const gchar *file_type,
                                      GList **data)
{
    gint i;
    gchar *str;

    str = orage_gdatetime_to_icaltime (a_day, TRUE);

    if (file_type[0] == 'O') {
        xfical_get_each_app_within_time_internal(str
                , days, type, ic_ical, file_type, data);
    }
#ifdef HAVE_ARCHIVE
    else if (file_type[0] == 'A') {
        xfical_get_each_app_within_time_internal(str
                , days, type, ic_aical, file_type, data);
    }
#endif
    else if (file_type[0] == 'F') {
        sscanf(file_type, "F%02d", &i);
        if (i < g_par.foreign_count && ic_f_ical[i].ical != NULL)
            xfical_get_each_app_within_time_internal(str
                    , days, type, ic_f_ical[i].ical, file_type, data);
         else {
             g_critical ("%s: unknown foreign file number %s",
                         G_STRFUNC, file_type);
         }
    }
    else {
        g_critical ("%s: unknown file type", G_STRFUNC);
    }

    g_free (str);
}

/* let's find next VEVENT or VTODO or VJOURNAL BEGIN or END
 * We rely that str is either BEGIN: or END: to show if we search the
 * beginning or the end */
static gchar *find_next(gchar *cur, gchar *end, gchar *str)
{
    gchar *next;
    gboolean found_valid = FALSE;

    next = cur;
    do {
        next = g_strstr_len(next, end-next, str);
        if (next) {
         /* we found it. Need to check that it is valid.
          * First skip BEGIN: or END: and point to the component type */
            next += strlen(str);
            if (g_str_has_prefix(next, "VEVENT\n")
            ||  g_str_has_prefix(next, "VEVENT\r\n")
            ||  g_str_has_prefix(next, "VTODO\n")
            ||  g_str_has_prefix(next, "VTODO\r\n")
            ||  g_str_has_prefix(next, "VJOURNAL\n")
            ||  g_str_has_prefix(next, "VJOURNAL\r\n"))
                found_valid = TRUE;
        }
    } while (next && !found_valid);
    return(next);
}

/* find the last str before cur. start from beg.
 * Idea is to find str, which is actually always BEGIN:
 * and then check that it is the beginning of VEVENT/VTODO/VJOURNAL
 * if it is not, repeat search, but move cur up to found string by
 * making len (prev-beg) smaller
 */
static gchar *find_prev(gchar *beg, gchar *cur, gchar *str)
{
    gchar *prev;
    gboolean found_valid = FALSE;

    prev = cur;
    do {
         prev = g_strrstr_len(beg, prev-beg, str);
         if (prev) {
         /* we found it. Need to check that it is valid.
          * First skip BEGIN: or END: and point to the component type */
            prev += strlen(str);
            if (g_str_has_prefix(prev, "VEVENT\n")
            ||  g_str_has_prefix(prev, "VEVENT\r\n")
            ||  g_str_has_prefix(prev, "VTODO\n")
            ||  g_str_has_prefix(prev, "VTODO\r\n")
            ||  g_str_has_prefix(prev, "VJOURNAL\n")
            ||  g_str_has_prefix(prev, "VJOURNAL\r\n"))
                found_valid = TRUE;
            else
                prev -= strlen(str);
         }
    } while (prev && !found_valid);
    return(prev);
}

 /* Read next EVENT/TODO/JOURNAL which contains the specified string 
  * from ical datafile.
  * str:     string to search
  * first:   get first appointment is TRUE, if not get next.
  * returns: NULL if failed.
  *          xfical_appt pointer to xfical_appt struct filled with data if 
  *          successfull. 
  *          You must deallocate the appt after the call
  */
static xfical_appt *xfical_appt_get_next_with_string_internal (
    const gchar *str, gboolean first, char *search_file, icalcomponent *base,
    const gchar *file_type)
{
    static gchar *text_upper, *text, *beg, *end;
    static gboolean upper_text;
    gchar *cur, *tmp, mem;
    gsize text_len, len;
    char *uid, ical_uid[XFICAL_UID_LEN+1];
    xfical_appt *appt;
    gboolean found_valid, search_done = FALSE;
    struct icaltimetype it;
    const char *stime;

    if (!ORAGE_STR_EXISTS(str))
        return(NULL);
    if (first) {
        if (!g_file_get_contents(search_file, &text, &text_len, NULL)) {
            g_critical ("%s: Could not open Orage ical file (%s)",
                        G_STRFUNC, search_file);
            return(NULL);
        }
        text_upper = g_utf8_strup(text, -1);
        if (text_len == strlen(text_upper)) {
            /* we can do upper case search since uppercase and original
             * string have same format. In other words we can find UID since
             * it starts in the same place in both strings (not 100 % sure, 
             * but works reliable enough until somebody files a bug...) */
            end = text_upper+text_len;
            beg = find_next(text_upper, end, "\nBEGIN:");
            upper_text = TRUE;
        }
        else { /* sorry, has to settle for normal comparison */
        /* let's find first BEGIN:VEVENT or BEGIN:VTODO or BEGIN:VJOURNAL
         * to start our search */
            end = text+text_len;
            beg = find_next(text, end, "\nBEGIN:");
            upper_text = FALSE;
            g_message ("%s: Can not do case independent comparison "
                       "(%" G_GSIZE_FORMAT "/%zu)", G_STRFUNC, text_len,
                       strlen(text_upper));
        }
        if (!beg) {
        /* the file can actually be empty and not have any events, so this
           may be quite ok */
            if (text_len > 100)
            {
                g_message ("%s: Could not find initial BEGIN:V-type from "
                           "Orage ical file (%s). Maybe file is empty.",
                           G_STRFUNC, search_file);
            }
            return(NULL);
        }
        beg -= strlen("\nBEGIN:"); /* we need to be able to find first, too */
        first = FALSE;
    }

    if (!first) {
        for (cur = g_strstr_len(beg, end-beg, str), found_valid = FALSE; 
             cur && !found_valid; ) {
            /* We have found a match, let's check that it is valid. 
             * We only accept SUMMARY, DESCRIPTION and LOCATION ical strings
             * to be valid. */
            /* First we need to find the beginning of our row */
            /* FIXME: this does not work if description has line changes */
            for (tmp = cur; tmp > beg && *tmp != '\n'; tmp--)
                ;
            tmp++; /* skip the '\n' */
            /* Then let's check that it is valid. Mark the end temporarily. */
            mem = *cur;
            *cur = '\0';
            /* FIXME: vcs files may contain charset like this:
             * SUMMARY;CHARSET=UTF-8:Text of summary
             * so matching to : is not ok 
             */
            if (g_str_has_prefix(tmp, "SUMMARY")
            ||  g_str_has_prefix(tmp, "DESCRIPTION")
            ||  g_str_has_prefix(tmp, "LOCATION")) 
                found_valid = TRUE;
            *cur = mem;
            if (!found_valid) {
                cur++;
                cur = g_strstr_len(cur, end-cur, str);
            }
        }
        if (!cur) {
            search_done = TRUE;
        }
        else {
            /* first find ^BEGIN:<type> which shows the start 
             * of the component.
             * then find UID: and get the appointment and finally locate the
             * ^END:<type> and setup search for he next time 
             */
            beg = find_prev(beg, cur, "\nBEGIN:");
            if (!beg) {
                g_critical ("%s: BEGIN:V-type not found %s", G_STRFUNC, str);
                search_done = TRUE;
            }
            else {
                uid = g_strstr_len(beg, end-beg, "UID:");
                if (!uid) {
                    g_warning ("%s: UID not found %s", G_STRFUNC, str);
                    search_done = TRUE;
                }
                else {
                    if (upper_text) {
                        /* we need to take UID from the original text, which
                         * has not been converted to upper case */
                        len = uid-text_upper;
                        tmp = text+len;
                        sscanf(tmp, "UID:%sXFICAL_UID_LEN", ical_uid);
                    }
                    else
                        sscanf(uid, "UID:%sXFICAL_UID_LEN", ical_uid);
                    if (strlen(ical_uid) > XFICAL_UID_LEN-2) {
                        g_critical ("%s: too long UID %s", G_STRFUNC, ical_uid);
                        return(NULL);
                    }
                    appt = appt_get_any(ical_uid, base, file_type);
                    if (!appt) {
                        g_warning ("%s: UID not found in ical file %s",
                                   G_STRFUNC, ical_uid);
                        search_done = TRUE;
                    }
                    else {
                        if (strcmp(g_par.local_timezone, "floating") == 0) {
                            orage_gdatetime_unref (appt->starttimecur);
                            appt->starttimecur = g_date_time_ref (appt->starttime);
                            orage_gdatetime_unref (appt->endtimecur);
                            appt->endtimecur = g_date_time_ref (appt->endtime);
                        }
                        else {
                            it = icaltimetype_from_gdatetime (appt->starttime,
                                                          appt->allDay);
                            it = convert_to_zone(it, appt->start_tz_loc);
                            it = icaltime_convert_to_zone(it
                                    , local_icaltimezone);
                            stime = icaltime_as_ical_string(it);
                            orage_gdatetime_unref (appt->starttimecur);
                            appt->starttimecur = orage_icaltime_to_gdatetime (stime);
                            it = icaltimetype_from_gdatetime (appt->endtime,
                                                          appt->allDay);
                            it = convert_to_zone(it, appt->end_tz_loc);
                            it = icaltime_convert_to_zone(it
                                    , local_icaltimezone);
                            stime = icaltime_as_ical_string(it);
                            orage_gdatetime_unref (appt->endtimecur);
                            appt->endtimecur = orage_icaltime_to_gdatetime (stime);
                        }
                        beg = find_next(uid, end, "\nEND:");
                        if (!beg) {
                            g_critical ("%s: END:V-type not found %s",
                                        G_STRFUNC, str);
                            search_done = TRUE;
                        }
                        else {
                            return(appt);
                        }
                    }
                }
            }
        }
    }

    g_free(text);
    g_free(text_upper);
    if (!search_done) {
        g_critical ("%s: illegal ending %s", G_STRFUNC, str);
    }
    return(NULL);
}

xfical_appt *xfical_appt_get_next_with_string (const gchar *str,
                                               const gboolean first,
                                               const gchar *file_type)
{
    gint i;

    if (file_type[0] == 'O') {
        return(xfical_appt_get_next_with_string_internal(str, first
                , g_par.orage_file , ic_ical, file_type));
    }
#ifdef HAVE_ARCHIVE
    else if (file_type[0] == 'A') {
        return(xfical_appt_get_next_with_string_internal(str, first
                , g_par.archive_file, ic_aical, file_type));
    }
#endif
    else if (file_type[0] == 'F') {
        sscanf(file_type, "F%02d", &i);
        if (i < g_par.foreign_count && ic_f_ical[i].ical != NULL)
            return(xfical_appt_get_next_with_string_internal(str, first
                    , g_par.foreign_data[i].file, ic_f_ical[i].ical, file_type));
        else {
            g_critical ("%s: unknown foreign file number %s",
                        G_STRFUNC, file_type);
            return(NULL);
        }
    }
    else {
        g_critical ("%s: unknown file type", G_STRFUNC);
        return(NULL);
    }
}

static gboolean is_importable_component (const ICalComponentKind kind)
{
    switch (kind)
    {
        case I_CAL_VEVENT_COMPONENT:
        case I_CAL_VTODO_COMPONENT:
        case I_CAL_VJOURNAL_COMPONENT:
#if 0
        case I_CAL_VFREEBUSY_COMPONENT
#endif
            return TRUE;

        default:
            return FALSE;
    }
}

/** Frees the internal icalcomponent only if it does not have a parent. If it
 *  does, it means we don't own it and we shouldn't free it.
 */
static void orage_calendar_component_free_icalcomp (OrageCalendarComponent *comp,
                                                    const gboolean free)
{
    if (comp->icalcomp == NULL)
        return;

    if (free)
        g_clear_object (&comp->icalcomp);

    /* Clean up */
    comp->need_sequence_inc = FALSE;
}

/** Generates a unique identificator, which can be used as part of the
 *  Message-ID header, or iCalendar component UID, or vCard UID. The resulting
 *  string doesn't contain any host name, it's a hexa-decimal string with no
 *  particular meaning. Free the returned string with g_free(), when no longer
 *  needed.
 *
 *  @return generated unique identificator as a newly allocated string
 */
static gchar *e_util_generate_uid (void)
{
    static volatile gint counter = 0;
    gchar *uid;
    GChecksum *checksum;

    checksum = g_checksum_new (G_CHECKSUM_SHA1);

    #define add_i64(_x) G_STMT_START { \
        gint64 i64 = (_x); \
        g_checksum_update (checksum, (const guchar *) &i64, sizeof (gint64)); \
    } G_STMT_END

    #define add_str(_x, _def) G_STMT_START { \
        const gchar *str = (_x); \
        if (!str) \
            str = (_def); \
        g_checksum_update (checksum, (const guchar *) str, strlen (str)); \
    } G_STMT_END

    add_i64 (g_get_monotonic_time ());
    add_i64 (g_get_real_time ());
    add_i64 (getpid ());
    add_i64 (getgid ());
    add_i64 (getppid ());
    add_i64 (g_atomic_int_add (&counter, 1));

    add_str (g_get_host_name (), "localhost");
    add_str (g_get_user_name (), "user");
    add_str (g_get_real_name (), "User");

    #undef add_i64
    #undef add_str

    uid = g_strdup (g_checksum_get_string (checksum));

    g_checksum_free (checksum);

    return uid;
}

/** Ensures that the mandatory calendar component properties (uid, dtstamp) do
 *  exist. If they don't exist, it creates them automatically.
 */
static void ensure_mandatory_properties (OrageCalendarComponent *comp)
{
    ICalProperty *prop;

    g_return_if_fail (comp->icalcomp != NULL);

    prop = i_cal_component_get_first_property (comp->icalcomp,
                                               I_CAL_UID_PROPERTY);

    if (prop)
        g_object_unref (prop);
    else
    {
        gchar *uid;

        uid = e_util_generate_uid ();
        i_cal_component_set_uid (comp->icalcomp, uid);
        g_free (uid);
    }

    prop = i_cal_component_get_first_property (comp->icalcomp,
                                               I_CAL_DTSTAMP_PROPERTY);
    if (prop)
        g_object_unref (prop);
    else
    {
        ICalTime *tt;

        tt = i_cal_time_new_current_with_zone (i_cal_timezone_get_utc_timezone ());

        prop = i_cal_property_new_dtstamp (tt);
        i_cal_component_take_property (comp->icalcomp, prop);

        g_object_unref (tt);
    }
}

/* Following code is copied from E-D-S, and in future may be removed. */
struct ics_file
{
    FILE *file;
    gboolean bof;
};

static gchar *get_line_fn (gchar *buf,
                           gsize size,
                           gpointer user_data)
{
    struct ics_file *fl = user_data;

    /* Skip the UTF-8 marker at the beginning of the file */
    if (fl->bof)
    {
        gchar *orig_buf = buf;
        gchar tmp[4];

        fl->bof = FALSE;

        if (fread (tmp, sizeof (gchar), 3, fl->file) != 3 || feof (fl->file))
            return NULL;

        if (((guchar) tmp[0]) != 0xEF ||
            ((guchar) tmp[1]) != 0xBB ||
            ((guchar) tmp[2]) != 0xBF)
        {
            if (size <= 3)
                return NULL;

            buf[0] = tmp[0];
            buf[1] = tmp[1];
            buf[2] = tmp[2];
            buf += 3;
            size -= 3;
        }

        if (!fgets (buf, size, fl->file))
            return NULL;

        return orig_buf;
    }

    return fgets (buf, size, fl->file);
}

static ICalComponent *e_cal_util_parse_ics_file (const gchar *filename)
{
    ICalParser *parser;
    ICalComponent *icalcomp;
    struct ics_file fl;

    fl.file = g_fopen (filename, "rb");
    if (!fl.file)
        return NULL;

    fl.bof = TRUE;

    parser = i_cal_parser_new ();

    icalcomp = i_cal_parser_parse (parser, get_line_fn, &fl);
    g_object_unref (parser);
    fclose (fl.file);

    return icalcomp;
}

GList *xfical_appt_new_from_file (GFile *file)
{
    gchar *ical_file_name;
    GList *list = NULL;

    ICalComponent *icomp;
    ICalComponent *subcomp;

    ical_file_name = g_file_get_path (file);
    icomp = e_cal_util_parse_ics_file (ical_file_name);
    if (icomp == NULL)
    {
        g_error ("%s: Cannot parse ISC file (%s) %s", G_STRFUNC,
                 ical_file_name, i_cal_error_strerror (i_cal_errno_return ()));
        return NULL;
    }

    if (i_cal_component_isa (icomp) != I_CAL_VCALENDAR_COMPONENT)
    {
        g_object_unref (icomp);
        g_error ("%s: File '%s' is not a VCALENDAR component", G_STRFUNC,
                 ical_file_name);

        return NULL;
    }

    for (subcomp = i_cal_component_get_first_component (icomp, I_CAL_ANY_COMPONENT);
         subcomp;
         g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp, I_CAL_ANY_COMPONENT))
    {
        ICalComponentKind kind;
        OrageCalendarComponent *comp;

        kind = i_cal_component_isa (subcomp);

        if (is_importable_component (kind))
        {
            comp = o_cal_component_new_from_icalcomponent (subcomp);
            if (comp)
                list = g_list_append (list, comp);
        }
        else if (kind != I_CAL_VTIMEZONE_COMPONENT)
        {
            /* We ignore TIMEZONE component; Orage only uses internal timezones
             * from libical.
             */
            g_warning ("%s: unknown component %s %s", G_STRFUNC,
                       i_cal_component_kind_to_string (kind),
                       ical_file_name);
        }
    }

    if (list == NULL)
    {
        g_warning ("%s: No valid ical components found", G_STRFUNC);
        return NULL;
    }

    return list;
}

static void orage_calendar_component_finalize (GObject *object)
{
    OrageCalendarComponent *comp = ORAGE_CALENDAR_COMPONENT (object);

    orage_calendar_component_free_icalcomp (comp, TRUE);

    /* Chain up to parent's finalize() method. */
    G_OBJECT_CLASS (orage_calendar_component_parent_class)->finalize (object);
}

static void orage_calendar_component_init (OrageCalendarComponent *comp)
{
    comp->icalcomp = NULL;
}

static void orage_calendar_component_class_init (OrageCalendarComponentClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = orage_calendar_component_finalize;
}

/** Sets the contents of a calendar component object from an #ICalComponent. If
 *  the @comp already had an #ICalComponent set into it, it will be freed
 *  automatically.
 *  @param comp A Orage calendar component object.
 *  @param icalcomp An #ICalComponent.
 *  @return TRUE on success, FALSE if @icalcomp is an unsupported component type
 */
static gboolean o_cal_component_set_icalcomponent (OrageCalendarComponent *comp,
                                                   ICalComponent *icalcomp)
{
    ICalComponentKind kind;

    g_return_val_if_fail (ORAGE_IS_CALENDAR_COMPONENT (comp), FALSE);

    if (comp->icalcomp == icalcomp)
        return TRUE;

    orage_calendar_component_free_icalcomp (comp, TRUE);

    if (icalcomp == NULL)
        return FALSE;

    kind = i_cal_component_isa (icalcomp);

    if (is_importable_component (kind) == FALSE)
        return FALSE;

    comp->icalcomp = g_object_ref (icalcomp);

    ensure_mandatory_properties (comp);

    return TRUE;
}

/** Creates a new empty Orage calendar component object. Once created, you
 *  should set it from an existing #icalcomponent structure by using
 *  o_cal_component_set_icalcomponent() or with a new empty component type by
 *  using o_cal_component_set_new_vtype().
 *
 *  @return A newly-created calendar component object.
 */
static OrageCalendarComponent *o_cal_component_new (void)
{
    return g_object_new (ORAGE_CALENDAR_COMPONENT_TYPE, NULL);
}

OrageCalendarComponent *o_cal_component_new_from_icalcomponent (
    ICalComponent *icalcomp)
{
    OrageCalendarComponent *comp;

    g_return_val_if_fail (icalcomp != NULL, NULL);

    comp = o_cal_component_new ();

    if (o_cal_component_set_icalcomponent (comp, icalcomp) == FALSE)
    {
        g_object_unref (comp);
        return NULL;
    }

    return comp;
}

static const gchar *o_cal_component_get_string_value (
    OrageCalendarComponent *ocal_comp, const gchar* (*getter)(ICalComponent *))
{
    const gchar *component_string;
    ICalComponent *icalcomp = ocal_comp->icalcomp;

    component_string = getter (icalcomp);

    if ((component_string == NULL) || (*component_string == '\0'))
        component_string = NULL;

    return component_string;
}

const gchar *o_cal_component_get_summary (OrageCalendarComponent *ocal_comp)
{
    return o_cal_component_get_string_value (ocal_comp,
                                             i_cal_component_get_summary);
}

const gchar *o_cal_component_get_location (OrageCalendarComponent *ocal_comp)
{
    return o_cal_component_get_string_value (ocal_comp,
                                             i_cal_component_get_location);
}

gboolean o_cal_component_is_all_day_event (OrageCalendarComponent *ocal_comp)
{
    ICalComponent *icalcomp = ocal_comp->icalcomp;
    ICalTime *dtstart;
    ICalTime *dtend;
    gboolean result;

    dtstart = i_cal_component_get_dtstart (icalcomp);
    dtend = i_cal_component_get_dtend (icalcomp);
    if (i_cal_time_is_date (dtstart))
        result = TRUE;
    else if (i_cal_time_is_date (dtend))
        result = TRUE;
    else
        result = FALSE;

    g_clear_object (&dtstart);
    g_clear_object (&dtend);

    return result;
}

xfical_type o_cal_component_get_type (OrageCalendarComponent *ocal_comp)
{
    ICalComponent *icalcomp = ocal_comp->icalcomp;
    ICalComponentKind kind;

    kind = i_cal_component_isa (icalcomp);

    switch (kind)
    {
        case ICAL_VEVENT_COMPONENT:
            return XFICAL_TYPE_EVENT;

        case ICAL_VTODO_COMPONENT:
            return XFICAL_TYPE_TODO;

        case ICAL_VJOURNAL_COMPONENT:
            return XFICAL_TYPE_JOURNAL;

        default:
            g_warning ("%s: Unknown ICalComponentKind", G_STRFUNC);
            return (xfical_type)-1;
    }
}
