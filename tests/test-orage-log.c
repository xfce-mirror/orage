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

#include "orage-log.h"

#include <glib.h>

/** Helper to simulate environment */
static gboolean test_log (const GLogLevelFlags level,
                          const gchar *debug_env,
                          const gchar *domain,
                          const gchar *orage_level)
{
    GLogField fields[1];
    gsize n_fields = 0;

    if (debug_env)
        g_setenv ("G_MESSAGES_DEBUG", debug_env, TRUE);
    else
        g_unsetenv ("G_MESSAGES_DEBUG");

    if (orage_level)
        g_setenv ("ORAGE_LOG_LEVEL", orage_level, TRUE);
    else
        g_unsetenv ("ORAGE_LOG_LEVEL");

    orage_log_update_levels_from_env ();

    /* Setup domain */
    if (domain != NULL)
    {
        fields[0].key = "GLIB_DOMAIN";
        fields[0].value = domain;
        fields[0].length = -1;
        n_fields = 1;
    }

    return orage_log_is_message_enabled (level, fields, n_fields);
}

static void test_default_levels (void)
{
    g_assert_true  (test_log (G_LOG_LEVEL_ERROR, NULL, NULL, NULL));
    g_assert_true  (test_log (G_LOG_LEVEL_CRITICAL, NULL, NULL, NULL));
    g_assert_true  (test_log (G_LOG_LEVEL_WARNING, NULL, NULL, NULL));
    g_assert_true  (test_log (G_LOG_LEVEL_MESSAGE, NULL, NULL, NULL));

    g_assert_false (test_log (G_LOG_LEVEL_INFO, NULL, NULL, NULL));
    g_assert_false (test_log (G_LOG_LEVEL_DEBUG, NULL, NULL, NULL));
}

static void test_log_level_threshold (void)
{
    g_assert_true  (test_log (G_LOG_LEVEL_ERROR, NULL, NULL, "error"));
    g_assert_false (test_log (G_LOG_LEVEL_CRITICAL, NULL, NULL, "error"));

    g_assert_true  (test_log (G_LOG_LEVEL_CRITICAL, NULL, NULL, "critical"));
    g_assert_false (test_log (G_LOG_LEVEL_WARNING, NULL, NULL, "critical"));

    g_assert_true  (test_log (G_LOG_LEVEL_WARNING, NULL, NULL, "warning"));
    g_assert_false (test_log (G_LOG_LEVEL_MESSAGE, NULL, NULL, "warning"));

    g_assert_true  (test_log (G_LOG_LEVEL_MESSAGE, NULL, NULL, "message"));
    g_assert_false (test_log (G_LOG_LEVEL_INFO, NULL, NULL, "message"));

    g_assert_true  (test_log (G_LOG_LEVEL_INFO, NULL, NULL, "info"));
    g_assert_false (test_log (G_LOG_LEVEL_DEBUG, NULL, NULL, "info"));

    g_assert_true  (test_log (G_LOG_LEVEL_DEBUG, NULL, NULL, "debug"));
}

static void test_debug_all (void)
{
    g_assert_true (test_log (G_LOG_LEVEL_INFO, "all", NULL, NULL));
    g_assert_true (test_log (G_LOG_LEVEL_DEBUG, "all", NULL, NULL));
}

static void test_domain_filter (void)
{
    g_assert_true (test_log (G_LOG_LEVEL_INFO, "orage", "orage", NULL));
    g_assert_false (test_log (G_LOG_LEVEL_INFO, "orage", "gtk", NULL));
}

static void test_combined (void)
{
    g_assert_true (test_log (G_LOG_LEVEL_INFO, "orage", "orage", "info"));
    g_assert_false (test_log (G_LOG_LEVEL_INFO, "orage", "gtk", "info"));
    g_assert_false (test_log (G_LOG_LEVEL_DEBUG, "all", "orage", "info"));
}

int main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/log/default", test_default_levels);
    g_test_add_func ("/log/threshold", test_log_level_threshold);
    g_test_add_func ("/log/debug_all", test_debug_all);
    g_test_add_func ("/log/domain", test_domain_filter);

#if 0
    g_test_add_func ("/log/combined", test_combined);
#endif

    return g_test_run ();
}
