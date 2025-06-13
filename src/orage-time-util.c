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
#include <glib.h>

/**************************************************************************
 * time_t manipulation functions.
 *
 * NOTE: these use the Unix timezone functions like mktime() and localtime()
 * and so should not be used in Evolution. New Evolution code should use
 * ICalTime values rather than time_t values wherever possible.
 **************************************************************************/

gint orage_date_time_compare_date (GDateTime *dt1, GDateTime *dt2)
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
