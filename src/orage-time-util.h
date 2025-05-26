/* Miscellaneous time-related utilities
 *
 * Copyright (C) 1998 The Free Software Foundation
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

#ifndef __TIME_UTIL_H__
#define __TIME_UTIL_H_

#include <glib.h>
#include <time.h>

G_BEGIN_DECLS

/* Unreferences dt if not NULL, and set it to NULL */
#define ocal_clear_date_time(dt) g_clear_pointer (dt, g_date_time_unref)

/* Returns the number of days in the month. Year is the normal year, e.g. 2001.
 * Month is 0 (Jan) to 11 (Dec). */
gint time_days_in_month (gint year, gint month);

/* Returns the 1-based day number within the year of the specified date.
 * Year is the normal year, e.g. 2001. Month is 0 to 11. */
gint time_day_of_year (gint day, gint month, gint year);

/* Returns the day of the week for the specified date, 0 (Sun) to 6 (Sat).
 * For the days that were removed on the Gregorian reformation, it returns
 * Thursday. Year is the normal year, e.g. 2001. Month is 0 to 11. */
gint time_day_of_week (gint day, gint month, gint year);

/* Returns whether the specified year is a leap year. Year is the normal year,
 * e.g. 2001. */
gboolean time_is_leap_year (gint year);

/* Returns the number of leap years since year 1 up to (but not including) the
 * specified year. Year is the normal year, e.g. 2001. */
gint time_leap_years_up_to (gint year);

/* Convert to or from an ISO 8601 representation of a time, in UTC,
 * e.g. "20010708T183000Z". */
gchar *isodate_from_time_t (time_t t);
time_t time_from_isodate (const gchar *str);

/* Add or subtract a number of days, weeks or months. */
time_t time_add_day (time_t time, gint days);
time_t time_add_week (time_t time, gint weeks);

/* Returns the beginning or end of the day. */
time_t time_day_begin (time_t t);
time_t time_day_end (time_t t);

/* Compares the dates if dt1 and dt2. The times are ignored.
 *
 * Returns: negative, 0 or positive */
gint ocal_date_time_compare_date (GDateTime *dt1, GDateTime *dt2);

G_END_DECLS

#endif // __TIME_UTIL_H__
