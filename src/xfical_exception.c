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

#ifndef NDEBUG
#define XFICAL_EXCEPTION_DEBUG 1 /* Debug info can be removed after 4.17 */
#endif

struct _xfical_exception
{
    GDateTime *time;
    xfical_exception_type type;
    gboolean all_day;

    gint ref_count;  /* (atomic) */
};

xfical_exception *xfical_exception_new (GDateTime *gdt,
                                        const gboolean all_day,
                                        const xfical_exception_type type)
{
    xfical_exception *except;
#if XFICAL_EXCEPTION_DEBUG
    gchar *time;
#endif

    except = g_new (xfical_exception, 1);
    except->time = g_date_time_ref (gdt);
    except->type = type;
    except->all_day = all_day;
    except->ref_count = 1;

#if XFICAL_EXCEPTION_DEBUG
    time = g_date_time_format (gdt, "%x %R");
    g_debug ("  NEW exception: %p, refcount=%d, gdt=%p, time='%s'",
             except, except->ref_count, gdt, time);
    g_free (time);
#endif

    return except;
}

xfical_exception *xfical_exception_ref (xfical_exception *except)
{
#if XFICAL_EXCEPTION_DEBUG
    gchar *time;
    GDateTime *gdt;
#endif

    g_return_val_if_fail (except != NULL, NULL);
    g_return_val_if_fail (except->ref_count > 0, NULL);

    g_atomic_int_inc (&except->ref_count);

#if XFICAL_EXCEPTION_DEBUG
    gdt = except->time;
    time = g_date_time_format (gdt, "%x %R");
    g_debug ("  REF exception: %p, refcount=%d, gdt=%p, time='%s'",
             except, except->ref_count, gdt, time);
    g_free (time);
#endif

    return except;
}

void xfical_exception_unref (xfical_exception *except)
{
#if XFICAL_EXCEPTION_DEBUG
    gchar *time;
#endif

    g_return_if_fail (except != NULL);
    g_return_if_fail (except->ref_count > 0);

#if XFICAL_EXCEPTION_DEBUG
    time = g_date_time_format (except->time, "%x %R");
    g_debug ("UNREF exception: %p, refcount=%d, gdt=%p, time='%s'",
             except, except->ref_count, except->time, time);
    g_free (time);
#endif

    if (g_atomic_int_dec_and_test (&except->ref_count))
    {
#if XFICAL_EXCEPTION_DEBUG
        g_debug ("UNREF exception: free %p", except);
#endif
        g_date_time_unref (except->time);
        g_free (except);
    }
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
