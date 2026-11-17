/*
 * Copyright (c) 2021-2023 Erkki Moorits
 * Copyright (c) 2006-2011 Juha Kautto (juha@xfce.org)
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

typedef struct _orage_timezone_array
{
    int    count;     /* how many timezones we have */
    char **city;      /* pointer to timezone location name strings */
    int  *utc_offset; /* pointer to int array holding utc offsets */
    int  *dst;        /* pointer to int array holding dst settings */
    char **tz;        /* pointer to timezone name strings */
    char **prev;      /* pointer to previous time change strings */
    char **next;      /* pointer to next time change strings */
    int  *next_utc_offset; /* pointer to int array holding utc offsets */
    char **country;   /* pointer to country name strings */
    char **cc;        /* pointer to country code strings */
} orage_timezone_array;

/**
 * get_orage_timezones:
 * @ical: %TRUE to include only timezones supported by libical, %FALSE otherwise
 *
 * Loads timezone information from the operating system's zoneinfo database.
 * The timezone data is parsed and cached in memory for subsequent calls.
 *
 * If the timezone data has already been loaded, the cached data is returned.
 * The returned array always includes the default UTC and floating timezone
 * entries.
 *
 * The returned data is owned by the timezone module and must not be freed
 * by the caller. Call free_orage_timezones() to release the allocated memory.
 *
 * Returns: (transfer none): The array of timezone information.
 */
orage_timezone_array get_orage_timezones (int ical);

/**
 * free_orage_timezones:
 *
 * Frees the cached timezone information and releases all associated memory.
 * It is safe to call this function even if get_orage_timezones() has not
 * been called or if the timezone data has already been freed.
 *
 * After this function returns, the timezone data can be loaded again by
 * calling get_orage_timezones().
 */
void free_orage_timezones (void);
