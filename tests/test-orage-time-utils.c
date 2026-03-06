/*
 * Copyright (c) 2026 Erkki Moorits
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

#include "orage-time-utils.h"
#include <glib.h>

static void test_compare_dates (void)
{
    GDateTime *d1 = g_date_time_new_local (2026, 2, 23, 0, 0, 0);
    GDateTime *d2 = g_date_time_new_local (2026, 2, 24, 0, 0, 0);

    g_assert_cmpint (orage_gdatetime_compare_date (d1, d2), ==, -1);
    g_assert_cmpint (orage_gdatetime_compare_date (d2, d1), ==, 1);
    g_assert_cmpint (orage_gdatetime_compare_date (d1, d1), ==, 0);

    g_date_time_unref (d1);
    g_date_time_unref (d2);
}

int main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/utility/compare_dates", test_compare_dates);

    return g_test_run ();
}
