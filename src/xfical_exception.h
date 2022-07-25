/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2022 Erkki Moorits
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
       Free Software Foundation
       51 Franklin Street, 5th Floor
       Boston, MA 02110-1301 USA

 */

#ifndef XFICAL_EXCEPTION_H
#define XFICAL_EXCEPTION_H 1

#include <glib.h>

typedef enum
{
    EXDATE,
    RDATE
} xfical_exception_type;

typedef struct _xfical_exception xfical_exception;

xfical_exception *xfical_exception_new (GDateTime *gdt,
                                        gboolean all_day,
                                        xfical_exception_type type);
void xfical_exception_free (xfical_exception *recur_exception);

GDateTime *xfical_exception_get_time (const xfical_exception *recur_exception);
xfical_exception_type xfical_exception_get_type (const xfical_exception *recur_exception);

#endif
