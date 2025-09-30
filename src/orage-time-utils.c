/*
 * Copyright (c) 2025 Erkki Moorits
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

#define _XOPEN_SOURCE /* glibc2 needs this */
#define _XOPEN_SOURCE_EXTENDED 1 /* strptime needs this in posix systems */

#include <config.h>

#include "orage-time-utils.h"
#include "orage-i18n.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <libical/ical.h>
#include <stddef.h>
#include <time.h>

#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
#include <langinfo.h>
#endif

GDateTime *orage_calendar_get_date (GtkCalendar *cal,
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
        g_error ("%s failed %04d-%02d-%02d %02d:%02d", G_STRFUNC,
                 year, month, day, hh, mm);
    }

    return gdt;
}

void orage_calendar_set_date (GtkCalendar *cal, GDateTime *gdt)
{
    guint cur_year, cur_month, cur_mday;
    gint year;
    gint month;
    gint day;

    g_date_time_get_ymd (gdt, &year, &month, &day);
    month -= 1;
    gtk_calendar_get_date (cal, &cur_year, &cur_month, &cur_mday);

    if (((gint)cur_year == year) && ((gint)cur_month == month))
        gtk_calendar_select_day (cal, day);
    else
    {
        gtk_calendar_select_day (cal, 1); /* need to avoid illegal day/month */
        gtk_calendar_select_month (cal, month, year);
        gtk_calendar_select_day (cal, day);
    }
}

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

gint orage_gdatetime_compare_year_month (GDateTime *gdt1, GDateTime *gdt2)
{
    gint y1;
    gint y2;
    gint m1;
    gint m2;
    gint d1;
    gint d2;

    g_date_time_get_ymd (gdt1, &y1, &m1, &d1);
    g_date_time_get_ymd (gdt2, &y2, &m2, &d2);

    if (m1 < m2)
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

gint orage_gdatetime_days_between (GDateTime *gdt1, GDateTime *gdt2)
{
    GDate gd1;
    GDate gd2;

    if ((gdt1 == NULL) && (gdt2 == NULL))
        return 0;
    else if (gdt1 == NULL)
        return -1;
    else if (gdt2 == NULL)
        return 1;

    g_date_set_dmy (&gd1, g_date_time_get_day_of_month (gdt1),
                    g_date_time_get_month (gdt1),
                    g_date_time_get_year (gdt1));

    g_date_set_dmy (&gd2, g_date_time_get_day_of_month (gdt2),
                    g_date_time_get_month (gdt2),
                    g_date_time_get_year (gdt2));

    return g_date_days_between (&gd1, &gd2);
}

gchar *orage_gdatetime_to_i18_time (GDateTime *gdt, const gboolean date_only)
{
    const gchar *fmt = date_only ? "%x" : "%x %R";

    return g_date_time_format (gdt, fmt);
}

gchar *orage_gdatetime_to_i18_time_with_zone (GDateTime *gdt)
{
    gchar *time_text;
    gchar *time_and_zone;
    const gchar *tzid_text;

    time_text = orage_gdatetime_to_i18_time (gdt, FALSE);
    tzid_text = g_time_zone_get_identifier (g_date_time_get_timezone (gdt));
    time_and_zone = g_strdup_printf ("%s %s", time_text, tzid_text);
    g_free (time_text);

    return time_and_zone;
}

gchar *orage_gdatetime_to_icaltime (GDateTime *gdt, const gboolean date_only)
{
    gint year;
    gint month;
    gint day;
    gchar *str;
    size_t len;

    g_date_time_get_ymd (gdt, &year, &month, &day);

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

struct icaltimetype orage_gdatetime_to_icaltimetype (GDateTime *gdt,
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

void orage_gdatetime_unref (GDateTime *gdt)
{
    if (gdt == NULL)
        return;

    g_date_time_unref (gdt);
}

gint orage_get_first_weekday (void)
{
    int week_start;

#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
    union
    {
        unsigned int word;
        char *string;
    } langinfo;
    gint week_1stday = 0;
    gint first_weekday = 1;
    guint week_origin;

    langinfo.string = nl_langinfo (_NL_TIME_FIRST_WEEKDAY);
    first_weekday = langinfo.string[0];
    langinfo.string = nl_langinfo (_NL_TIME_WEEK_1STDAY);
    week_origin = langinfo.word;
    if (week_origin == 19971130) /* Sunday */
        week_1stday = 0;
    else if (week_origin == 19971201) /* Monday */
        week_1stday = 1;
    else
        g_warning ("Unknown value of _NL_TIME_WEEK_1STDAY");

    week_start = (week_1stday + first_weekday - 1) % 7;

#else
    gchar *week_start_str;

#if 0  /* Week start from Orage */
    /* Translate to calendar:week_start:0 if you want Sunday to be the first
     * day of the week to calendar:week_start:1 if you want Monday to be the
     * first day of the week, and so on.
     */
    week_start_str = _("calendar:week_start:0");
#else /* Week start from GTK. */
    /* Use a define to hide the string from xgettext */
#define GTK_WEEK_START "calendar:week_start:0"
    week_start_str = dgettext ("gtk30", GTK_WEEK_START);
#endif

    if (strncmp (week_start_str, "calendar:week_start:", 20) == 0)
        week_start = *(week_start_str + 20) - '0';
    else
        week_start = -1;

    if (week_start < 0 || week_start > 6)
    {
        g_warning ("Whoever translated calendar:week_start:0 did so wrongly");
        week_start = 0;
    }
#endif

    return (week_start - 1 + 7) % 7;
}

GDateTime *orage_icaltime_to_gdatetime (const gchar *icaltime)
{
    struct tm t = {0};
    char *ret;

    ret = strptime (icaltime, "%Y%m%dT%H%M%S", &t);
    if (ret == NULL)
    {
        /* Not all format string matched, so it must be DATE. Depending on OS
         * and libs, it is not always guaranteed that all required fields are
         * filled. Convert only with date formatter.
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
        if (mktime (&t) == (time_t) - 1)
        {
            g_warning ("%s failed %d %d %d",
                       G_STRFUNC, t.tm_year, t.tm_mon, t.tm_mday);

            return NULL;
        }

        t.tm_hour = 0;
        t.tm_min = 0;
        t.tm_sec = 0;
    }
    else if (ret[0] != '\0')
    {
        /* icaltime was not processed completely. UTC times end to Z, which is
         * ok.
         */
        if (ret[0] != 'Z' || ret[1] != '\0') /* real error */
            g_error ("%s icaltime='%s' ret='%s'", G_STRFUNC, icaltime, ret);
    }

    t.tm_year += 1900;
    t.tm_mon += 1;

    return g_date_time_new_local (t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour,
                                  t.tm_min, t.tm_sec);
}

gchar *orage_icaltime_to_i18_time (const gchar *icaltime)
{
    /* timezone is not converted */
    GDateTime *gdt;
    gchar *ct;
    gboolean date_only;

    gdt = orage_icaltime_to_gdatetime (icaltime);
    date_only = (strchr (icaltime, 'T') == NULL);
    ct = orage_gdatetime_to_i18_time (gdt, date_only);
    g_date_time_unref (gdt);

    return ct;
}

GDateTime *orage_icaltimetype_to_gdatetime (struct icaltimetype *icaltime)
{
    return g_date_time_new_local (icaltime->year, icaltime->month,
                                  icaltime->day, icaltime->hour,
                                  icaltime->minute, icaltime->second);
}

GDateTime *orage_icaltimetype_to_gdatetime2 (struct icaltimetype t)
{
    const gchar *i18_date = icaltime_as_ical_string (t);

    return orage_icaltime_to_gdatetime (i18_date);
}

GDateTime *orage_time_t_to_gdatetime (const time_t t, const gchar *tz_id)
{
    GTimeZone *tz;
    GDateTime *gdt;
    GDateTime *gdt_tmp;;

    tz = g_time_zone_new_identifier (tz_id);
    if (tz == NULL)
    {
        g_debug ("%s @ %d: failed to convert timezone '%s', "
                 "using local timezone", G_STRFUNC, __LINE__, tz_id);
        tz = g_time_zone_new_local ();
    }

    gdt_tmp = g_date_time_new_from_unix_utc (t);
    gdt = g_date_time_to_timezone (gdt_tmp, tz);
    g_date_time_unref (gdt_tmp);
    g_time_zone_unref (tz);

    return gdt;
}
