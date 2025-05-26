/* Miscellaneous time-related utilities
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Federico Mena <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Damon Chaplin <damon@ximian.com>
 */

#include "orage-time-util.h"
#include <ctype.h>
#include <string.h>

#ifdef G_OS_WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif

/* The gmtime() in Microsoft's C library is MT-safe */
#define gmtime_r(tp, tmp) (gmtime (tp) ? (*(tmp) = *gmtime (tp), (tmp)) : 0)
#endif

#define REFORMATION_DAY                                                       \
  639787 /* First day of the reformation, counted from 1 Jan 1 */
#define MISSING_DAYS 11   /* They corrected out 11 days */
#define THURSDAY 4        /* First day of reformation */
#define SATURDAY 6        /* Offset value; 1 Jan 1 was a Saturday */
#define ISODATE_LENGTH 17 /* 4+2+2+1+2+2+2+1 + 1 */

/* Number of days in a month, using 0 (Jan) to 11 (Dec). For leap years,
 * add 1 to February (month 1). */
static const gint days_in_month[12]
        = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**************************************************************************
 * time_t manipulation functions.
 *
 * NOTE: these use the Unix timezone functions like mktime() and localtime()
 * and so should not be used in Evolution. New Evolution code should use
 * ICalTime values rather than time_t values wherever possible.
 **************************************************************************/

/**
 * time_add_day:
 * @time: A time_t value.
 * @days: Number of days to add.
 *
 * Adds a day onto the time, using local time.
 * Note that if clocks go forward due to daylight savings time, there are
 * some non-existent local times, so the hour may be changed to make it a
 * valid time. This also means that it may not be wise to keep calling
 * time_add_day() to step through a certain period - if the hour gets changed
 * to make it valid time, any further calls to time_add_day() will also return
 * this hour, which may not be what you want.
 *
 * Returns: a time_t value containing @time plus the days added.
 */
time_t
time_add_day (time_t time, gint days)
{
    struct tm *tm;

    tm = localtime (&time);
    tm->tm_mday += days;
    tm->tm_isdst = -1;

    return mktime (tm);
}

/**
 * time_add_week:
 * @time: A time_t value.
 * @weeks: Number of weeks to add.
 *
 * Adds the given number of weeks to a time value.
 *
 * Returns: a time_t value containing @time plus the weeks added.
 */
time_t
time_add_week (time_t time, gint weeks)
{
    return time_add_day (time, weeks * 7);
}

/**
 * time_day_begin:
 * @t: A time_t value.
 *
 * Returns the start of the day, according to the local time.
 *
 * Returns: the time corresponding to the beginning of the day.
 */
time_t
time_day_begin (time_t t)
{
    struct tm tm;

    tm = *localtime (&t);
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    tm.tm_isdst = -1;

    return mktime (&tm);
}

gint
ocal_date_time_compare_date (GDateTime *dt1, GDateTime *dt2)
{
    GDate d1, d2;

    if (!dt1 && !dt2)
        return 0;
    else if (!dt1)
        return -1;
    else if (!dt2)
        return 1;

    g_date_set_dmy (&d1, g_date_time_get_day_of_month (dt1),
                    g_date_time_get_month (dt1), g_date_time_get_year (dt1));

    g_date_set_dmy (&d2, g_date_time_get_day_of_month (dt2),
                    g_date_time_get_month (dt2), g_date_time_get_year (dt2));

    return g_date_days_between (&d2, &d1);
}

/**
 * time_day_end:
 * @t: A time_t value.
 *
 * Returns the end of the day, according to the local time.
 *
 * Returns: the time corresponding to the end of the day.
 */
time_t
time_day_end (time_t t)
{
    struct tm tm;

    tm = *localtime (&t);
    tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
    tm.tm_mday++;
    tm.tm_isdst = -1;

    return mktime (&tm);
}

/**************************************************************************
 * General time functions.
 **************************************************************************/

/**
 * time_days_in_month:
 * @year: The year.
 * @month: The month.
 *
 * Returns the number of days in the month. Year is the normal year, e.g. 2001.
 * Month is 0 (Jan) to 11 (Dec).
 *
 * Returns: number of days in the given month/year.
 */
gint
time_days_in_month (gint year, gint month)
{
    gint days;

    g_return_val_if_fail (year >= 1900, 0);
    g_return_val_if_fail ((month >= 0) && (month < 12), 0);

    days = days_in_month[month];
    if (month == 1 && time_is_leap_year (year))
        days++;

    return days;
}

/**
 * time_day_of_year:
 * @day: The day.
 * @month: The month.
 * @year: The year.
 *
 * Returns the 1-based day number within the year of the specified date.
 * Year is the normal year, e.g. 2001. Month is 0 to 11.
 *
 * Returns: the day of the year.
 */
gint
time_day_of_year (gint day, gint month, gint year)
{
    gint i;

    for (i = 0; i < month; i++)
    {
        day += days_in_month[i];

        if (i == 1 && time_is_leap_year (year))
            day++;
    }

    return day;
}

/**
 * time_day_of_week:
 * @day: The day.
 * @month: The month.
 * @year: The year.
 *
 * Returns the day of the week for the specified date, 0 (Sun) to 6 (Sat).
 * For the days that were removed on the Gregorian reformation, it returns
 * Thursday. Year is the normal year, e.g. 2001. Month is 0 to 11.
 *
 * Returns: the day of the week for the given date.
 */
gint
time_day_of_week (gint day, gint month, gint year)
{
    gint n;

    n = (year - 1) * 365 + time_leap_years_up_to (year - 1)
            + time_day_of_year (day, month, year);

    if (n < REFORMATION_DAY)
        return (n - 1 + SATURDAY) % 7;

    if (n >= (REFORMATION_DAY + MISSING_DAYS))
        return (n - 1 + SATURDAY - MISSING_DAYS) % 7;

    return THURSDAY;
}

/**
 * time_is_leap_year:
 * @year: The year.
 *
 * Returns whether the specified year is a leap year. Year is the normal year,
 * e.g. 2001.
 *
 * Returns: TRUE if the year is leap, FALSE if not.
 */
gboolean
time_is_leap_year (gint year)
{
    if (year <= 1752)
        return !(year % 4);
    else
        return (!(year % 4) && (year % 100)) || !(year % 400);
}

/**
 * time_leap_years_up_to:
 * @year: The year.
 *
 * Returns the number of leap years since year 1 up to (but not including) the
 * specified year. Year is the normal year, e.g. 2001.
 *
 * Returns: number of leap years.
 */
gint
time_leap_years_up_to (gint year)
{
    /* There is normally a leap year every 4 years, except at the turn of
     * centuries since 1700. But there is a leap year on centuries since 1700
     * which are divisible by 400. */
    return (year / 4 - ((year > 1700) ? (year / 100 - 17) : 0)
            + ((year > 1600) ? ((year - 1600) / 400) : 0));
}

/**
 * isodate_from_time_t:
 * @t: A time value.
 *
 * Creates an ISO 8601 UTC representation from a time value.
 *
 * Returns: String with the ISO 8601 representation of the UTC time.
 **/
gchar *
isodate_from_time_t (time_t t)
{
    gchar *ret;
    struct tm stm;
    const gchar fmt[] = "%04d%02d%02dT%02d%02d%02dZ";

    gmtime_r (&t, &stm);
    ret = g_malloc (ISODATE_LENGTH);
    g_snprintf (ret, ISODATE_LENGTH, fmt, (stm.tm_year + 1900), (stm.tm_mon + 1),
                stm.tm_mday, stm.tm_hour, stm.tm_min, stm.tm_sec);

    return ret;
}
