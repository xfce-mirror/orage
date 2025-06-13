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

#ifndef ORAGE_TIME_UTIL_H
#define ORAGE_TIME_UTIL_H 1

#include <glib.h>
#include <time.h>

G_BEGIN_DECLS

/* Unreferences dt if not NULL, and set it to NULL */
#define orage_clear_date_time(dt) g_clear_pointer (dt, g_date_time_unref)

/** Compares the dates if dt1 and dt2. The times are ignored.
 *  Returns: negative, 0 or positive
 */
gint orage_date_time_compare_date (GDateTime *dt1, GDateTime *dt2);

G_END_DECLS

#endif
