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

#include "functions.h"

#include <glib.h>

/* TODO: Remove g_strdup() when orage_process_text_commands() no longer
 * modifies the input string.
 */
#define CONST_STRING_FIX 1

static void test_orage_process_text_commands_input_is_not_modified (void)
{
#if CONST_STRING_FIX
    gchar *text = g_strdup ("I am <&Y1980>");
    gchar *result;

    result = orage_process_text_commands (text);
    g_assert_cmpstr (text, ==, "I am <&Y1980>");
    g_assert_cmpstr (result, ==, "I am 46");

    g_free (result);
    g_free (text);
#else
    const gchar *text = "I am <&Y1980>";
    gchar *result;

    result = orage_process_text_commands (text);
    g_assert_cmpstr (text, ==, "I am <&Y1980>");
    g_assert_cmpstr (result, ==, "I am 46");

    g_free (result);
#endif
}

static void test_orage_process_text_commands_null (void)
{
    gchar *result;

    result = orage_process_text_commands (NULL);
    g_assert_null (result);
}

static void test_orage_process_text_commands_no_commands (void)
{
    gchar *result;

    result = orage_process_text_commands ("Hello world");
    g_assert_cmpstr (result, ==, "Hello world");

    g_free (result);
}

static void test_orage_process_text_commands_empty (void)
{
    gchar *result;

    result = orage_process_text_commands ("");
    g_assert_cmpstr (result, ==, "");

    g_free (result);
}

static void test_orage_process_text_commands_multiple_years (void)
{
#if CONST_STRING_FIX
    gchar *text = g_strdup ("Born <&Y1980>, graduated <&Y2000>");
    gchar *result;

    result = orage_process_text_commands (text);
    g_assert_cmpstr (result, ==, "Born 46, graduated 26");

    g_free (result);
    g_free (text);
#else
    gchar *result;

    result = orage_process_text_commands ("Born <&Y1980>, graduated <&Y2000>");
    g_assert_cmpstr (result, ==, "Born 46, graduated 26");

    g_free (result);
#endif
}

static void test_orage_process_text_commands_invalid_year (void)
{
#if CONST_STRING_FIX
    gchar *text1 = g_strdup ("Test <&Yabc>");
    gchar *text2 = g_strdup ("Test <&Y>");
    gchar *text3 = g_strdup ("Test <&Y0>");
    gchar *text4 = g_strdup ("Test <&Y2050>");
    gchar *result;

    result = orage_process_text_commands (text1);
    g_assert_cmpstr (result, ==, "Test <&Yabc>");
    g_free (result);

    result = orage_process_text_commands (text2);
    g_assert_cmpstr (result, ==, "Test <&Y>");
    g_free (result);

    result = orage_process_text_commands (text3);
    g_assert_cmpstr (result, ==, "Test <&Y0>");
    g_free (result);

    result = orage_process_text_commands (text4);
    g_assert_cmpstr (result, ==, "Test <&Y2050>");
    g_free (result);

    g_free (text1);
    g_free (text2);
    g_free (text3);
    g_free (text4);
#else
    gchar *result;

    result = orage_process_text_commands ("Test <&Yabc>");
    g_assert_cmpstr (result, ==, "Test <&Yabc>");
    g_free (result);

    result = orage_process_text_commands ("Test <&Y>");
    g_assert_cmpstr (result, ==, "Test <&Y>");
    g_free (result);

    result = orage_process_text_commands ("Test <&Y0>");
    g_assert_cmpstr (result, ==, "Test <&Y0>");
    g_free (result);

    result = orage_process_text_commands ("Test <&Y2050>");
    g_assert_cmpstr (result, ==, "Test <&Y2050>");
    g_free (result);
#endif
}

static void test_orage_process_text_commands_unsupported_command (void)
{
#if CONST_STRING_FIX
    gchar *text1 = g_strdup ("Test <&X123>");
    gchar *result;

    result = orage_process_text_commands (text1);
    g_assert_cmpstr (result, ==, "Test <&X123>");
    g_free (result);

    g_free (text1);
#else
    gchar *result;

    result = orage_process_text_commands ("Test <&X123>");
    g_assert_cmpstr (result, ==, "Test <&X123>");
    g_free (result);
#endif
}

int main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/functions/orage_process_text_commands_input_is_not_modified",
                     test_orage_process_text_commands_input_is_not_modified);
    g_test_add_func ("/functions/orage_process_text_commands_null",
                     test_orage_process_text_commands_null);
    g_test_add_func ("/functions/orage_process_text_commands_no_commands",
                     test_orage_process_text_commands_no_commands);
    g_test_add_func ("/functions/orage_process_text_commands_empty",
                     test_orage_process_text_commands_empty);
    g_test_add_func ("/functions/orage_process_text_commands_multiple_years",
                     test_orage_process_text_commands_multiple_years);
    g_test_add_func ("/functions/orage_process_text_commands_invalid_year",
                     test_orage_process_text_commands_invalid_year);
    g_test_add_func ("/functions/orage_process_text_commands_unsupported_command",
                     test_orage_process_text_commands_unsupported_command);

    /* Do not abort tests on warnings or other non-fatal log messages. */
    g_log_set_always_fatal (0);

    return g_test_run ();
}
