/*  xfce4
 *  Copyright (C) 2006-2011 Juha Kautto (juha@xfce.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This pibrary is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


/* silence -Wformat-y2k warning when using %c or %x (see man strftime.3) */
static inline size_t
_strftime (char *s,
           size_t max,
           const char *fmt,
           const struct tm *tm)
{
    return strftime(s, max, fmt, tm);
}

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

orage_timezone_array get_orage_timezones(int details, int ical);
extern void free_orage_timezones (void);
