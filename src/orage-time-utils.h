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

#ifndef ORAGE_TIME_UTILS_H
#define ORAGE_TIME_UTILS_H 1

#include <glib.h>
#include <gtk/gtk.h>
#include <libical/ical.h>

/**
 * orage_calendar_to_gdatetime:
 * @cal: (nonnull): a #GtkCalendar instance
 * @hh: hour value (0–23)
 * @mm: minute value (0–59)
 *
 * Creates a new #GDateTime object from the currently selected date
 * in the given @cal widget, using the provided hour and minute values.
 *
 * The returned #GDateTime is constructed in the local timezone.
 * The caller takes ownership of the returned object and must unreference it
 * with `g_date_time_unref()` when no longer needed.
 *
 * If the given date and time combination is invalid, the function
 * will raise a fatal error via `g_error()`.
 *
 * Returns: (transfer full): a newly allocated #GDateTime, or never %NULL
 */
GDateTime *orage_calendar_get_date (GtkCalendar *cal, gint hh, gint mm);

/**
 * orage_calendar_set_date:
 * @cal: (nonnull): a #GtkCalendar instance
 * @gdt: (nonnull): a #GDateTime representing the date to select
 *
 * Sets the given @cal widget to the date specified by @gdt.
 *
 * If the currently displayed month and year in @cal match those in @gdt,
 * only the day is updated. Otherwise, the calendar will first change to
 * the appropriate month and year, then select the specified day.
 *
 * This function modifies the state of @cal and does not return a value.
 */
void orage_calendar_set_date (GtkCalendar *cal, GDateTime *gdt);

/**
 * orage_gdatetime_compare_date:
 * @gdt1: (nonnull): first #GDateTime to compare
 * @gdt2: (nonnull): second #GDateTime to compare
 *
 * Compares two #GDateTime objects by their **date components only**
 * (year, month, and day), ignoring the time of day.
 *
 * This function extracts the year, month, and day from both @gdt1 and @gdt2
 * and performs a lexicographical comparison in the order of day, month, and
 * year.
 *
 * Returns:
 *   - `0` if the dates are equal
 *   - `-1` if @gdt1 is earlier than @gdt2
 *   - `1` if @gdt1 is later than @gdt2
 */
gint orage_gdatetime_compare_date (GDateTime *gdt1, GDateTime *gdt2);

/**
 * orage_gdatetime_compare_year_month:
 * @gdt1: (nonnull): first #GDateTime to compare
 * @gdt2: (nonnull): second #GDateTime to compare
 *
 * Compares two #GDateTime objects by their **year and month components only**,
 * ignoring the day and time of day.
 *
 * This function extracts the year and month from both @gdt1 and @gdt2,
 * then performs a lexicographical comparison: month first, then year.
 *
 * Returns:
 *   - `0` if the year and month are equal
 *   - `-1` if @gdt1 is earlier than @gdt2
 *   - `1` if @gdt1 is later than @gdt2
 */
gint orage_gdatetime_compare_year_month (GDateTime *gdt1, GDateTime *gdt2);


/**
 * orage_gdatetime_days_between:
 * @gdt1: first #GDateTime value
 * @gdt2: second #GDateTime value
 *
 * Calculates the number of days between two #GDateTime objects.
 *
 * Both @gdt1 and @gdt2 are converted to #GDate objects, and the difference
 * is computed in full days. The result can be negative if @gdt2 occurs
 * before @gdt1.
 *
 * If one of the parameters is %NULL, the function returns:
 * - `0` if both are %NULL
 * - `-1` if only @gdt1 is %NULL
 * - `1` if only @gdt2 is %NULL
 *
 * Returns: the number of days between the two dates. Can be negative if
 *          @gdt2 < @gdt1.
 */
gint orage_gdatetime_days_between (GDateTime *gdt1, GDateTime *gdt2);

/**
 * orage_gdatetime_to_i18_time:
 * @gdt: (nonnull): a #GDateTime to format
 * @date_only: whether to include only the date (%TRUE) or both date and time
 *             (%FALSE)
 *
 * Formats the given @gdt as a localized string according to the current locale.
 *
 * If @date_only is %TRUE, the format string `"%x"` is used, which produces
 * a date representation based on the system locale.
 * If @date_only is %FALSE, the format string `"%x %R"` is used, combining
 * the localized date with a 24-hour time (hours and minutes).
 *
 * The returned string is newly allocated and must be freed by the caller
 * using `g_free()`.
 *
 * Returns: (transfer full): a newly allocated localized string representing the
 *          date (and time)
 */
gchar *orage_gdatetime_to_i18_time (GDateTime *gdt, gboolean date_only);

/**
 * orage_gdatetime_to_i18_time_with_zone:
 * @gdt: (nonnull): a #GDateTime object to format
 *
 * Formats the given @gdt as a localized string including both the date, time,
 * and the timezone identifier.
 *
 * The function first formats the date and time according to the system locale
 * (using `orage_gdatetime_to_i18_time()`), then appends the timezone
 * identifier from @gdt.
 *
 * The returned string is newly allocated and must be freed by the caller
 * using `g_free()`.
 *
 * Returns: (transfer full): a newly allocated localized string containing
 *          date, time, and timezone.
 */
gchar *orage_gdatetime_to_i18_time_with_zone (GDateTime *gdt);

/**
 * orage_gdatetime_to_icaltime:
 * @gdt: (nonnull): a #GDateTime to convert
 * @date_only: if %TRUE, returns only the date in `yyyymmdd` format;
 *             if %FALSE, returns full date and time in `yyyymmddThhmmss` format
 *
 * Creates a string representation of the given @gdt suitable for
 * **iCalendar (iCal)** export. Unlike `g_date_time_format()`, this function
 * adds leading zeros to the year, month, day, hour, minute, and second as
 * required by the iCal specification.
 *
 * Note: This function is intended **only for exporting GDateTime values** in
 * iCal format. Do not use this function for Orage internal date/time
 * calculations; use #GDateTime instead.
 *
 * The returned string is newly allocated and must be freed by the caller
 * using `g_free()`.
 *
 * Returns: (transfer full): a newly allocated string representing the date or
 *          date-time in iCal-compliant format.
 */
gchar *orage_gdatetime_to_icaltime (GDateTime *gdt, gboolean date_only);

/**
 * orage_gdatetime_to_icaltimetype:
 * @gdt: a #GDateTime instance to convert
 * @date_only: %TRUE if the result should contain only date (no time)
 *
 * Converts a #GDateTime instance to an #icaltimetype. This implementation uses
 * an intermediate string representation before conversion.
 *
 * Returns: the resulting #icaltimetype structure
 */
struct icaltimetype orage_gdatetime_to_icaltimetype (GDateTime *gdt,
                                                     gboolean date_only);

/**
 * orage_gdatetime_unref:
 * @gdt: (nullable): pointer to the #GDateTime object to unreference
 *
 * Unreferences the given #GDateTime object if it is not %NULL.
 *
 * This is a convenience wrapper around `g_date_time_unref()` that first
 * checks whether the pointer is %NULL, preventing a potential segmentation
 * fault.
 *
 */
void orage_gdatetime_unref (GDateTime *gdt);

/**
 * orage_get_first_weekday:
 *
 * Determines the first day of the week according to the system locale or
 * Orage/GTK configuration, and returns it in a format compatible with
 * libical (0 = Monday, 6 = Sunday).
 *
 * This function is based on GtkCalendar's internal logic but modified
 * to produce a Monday-to-Sunday index as expected by iCalendar.
 *
 * Returns: the first weekday, as an integer from 0 (Monday) to 6 (Sunday)
 */
gint orage_get_first_weekday (void);

/**
 * orage_icaltime_to_gdatetime:
 * @icaltime: (nonnull): a string containing a date or date-time in iCal format
 *
 * Converts an iCal-formatted string to a newly allocated #GDateTime object.
 *
 * The function supports both full date-time strings (`yyyymmddThhmmss`) and
 * date-only strings (`yyyymmdd`). For date-only strings, the time components
 * are set to 00:00:00.
 *
 * Note: This function is intended **only for importing iCal date/time values**.
 * Do not use this function for Orage internal date/time calculations; use
 * #GDateTime directly.
 *
 * If the input string does not contain valid date/time information, the
 * function returns %NULL.
 *
 * Returns: (transfer full): a newly allocated #GDateTime representing the input
 *          date/time, or %NULL if the conversion failed.
 */
GDateTime *orage_icaltime_to_gdatetime (const gchar *icaltime);

/**
 * orage_icaltime_to_i18_time:
 * @icaltime: (nullable): iCal formatted time string (e.g. "20250930T104500Z" or
 *            "20250930")
 *
 * Converts an iCal-formatted time string into a localized, human-readable
 * time or date string.
 *
 * If the input string does not contain the `'T'` separator, it is
 * treated as a date-only value (no time component). Timezone
 * information is not converted — the result reflects the local
 * interpretation of the input.
 *
 * Returns: (transfer full): A newly-allocated localized string representing
 * the given time or date. The caller must free the returned string
 * with g_free() when no longer needed.
 */
gchar *orage_icaltime_to_i18_time (const gchar *icaltime);

/**
 * orage_icaltimetype_to_gdatetime:
 * @icaltime: (not nullable): an #icaltimetype structure to convert
 *
 * Converts an iCalendar @icaltime into a GLib #GDateTime.
 * The returned #GDateTime is created in the local time zone.
 *
 * Returns: (transfer full) (nullable): a new #GDateTime, or %NULL if
 * the given values are invalid. The caller must free the returned value
 * with g_date_time_unref().
 *
 * Notes:
 * - The time zone information in @icaltime is not used; the conversion
 *   always assumes the system local time zone.
 * - If @icaltime only contains a date (without time), the resulting
 *   #GDateTime will be created with the time set to 00:00:00.
 */
GDateTime *orage_icaltimetype_to_gdatetime (struct icaltimetype *icaltime);

/**
 * icaltimetype_to_gdatetime:
 * @t: an #icaltimetype value
 *
 * Converts a #icaltimetype structure into a #GDateTime object. The resulting
 * #GDateTime reflects the same date and time components as the input.
 *
 * Returns: (transfer full): a newly-allocated #GDateTime corresponding
 * to @t. The caller must free it with g_date_time_unref() when no longer
 * needed.
 */
GDateTime *orage_icaltimetype_to_gdatetime2 (struct icaltimetype t);

#endif
