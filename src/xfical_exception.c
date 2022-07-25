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

#include "xfical_exception.h"
#include "functions.h"
#include <glib.h>

struct _xfical_exception
{
    GDateTime *time;
    xfical_exception_type type;
    gboolean all_day;
};

xfical_exception *xfical_exception_new (GDateTime *gdt,
                                        const gboolean all_day,
                                        const xfical_exception_type type)
{
    xfical_exception *recur_exception;

    recur_exception = g_new (xfical_exception, 1);
    recur_exception->time = g_date_time_ref (gdt);
    recur_exception->type = type;
    recur_exception->all_day = all_day;

    return recur_exception;
}

void xfical_exception_free (xfical_exception *recur_exception)
{
    g_date_time_unref (recur_exception->time);
    g_free (recur_exception);
}

GDateTime *xfical_exception_get_time (const xfical_exception *recur_exception)
{
    return recur_exception->time;
}

xfical_exception_type xfical_exception_get_type (const xfical_exception *recur_exception)
{
    return recur_exception->type;
}

gchar *xfical_exception_to_i18 (const xfical_exception *recur_exception)
{
    gchar type_chr;
    gchar *p_time;
    gchar *text;
    const xfical_exception_type type = recur_exception->type;

    switch (type)
    {
        case EXDATE:
            type_chr = '-';
            break;

        case RDATE:
            type_chr = '+';
            break;

        default:
            g_error ("%s: unknown exception type '%d'", G_STRFUNC, type);
            type_chr = '\0';
            break;
    }

    p_time = orage_gdatetime_to_i18_time (recur_exception->time,
                                          recur_exception->all_day);
    text = g_strdup_printf("%s %c", p_time, type_chr);
    g_free (p_time);

    return text;
}
