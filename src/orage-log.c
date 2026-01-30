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

/* Handles Orage debug logging. Greatly influenced by GNOME Calendar logging
 * and GLib logging.
 */

#include "orage-log.h"

#include <glib.h>
#include <stdio.h>
#include <unistd.h>

#define LOG_STREAM stdout

#define DEFAULT_LEVELS (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE)
#define INFO_LEVELS (G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG)
#define ALERT_LEVELS (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING)

/* orage_log_domains is guaranteed to be non-NULL after init */
static gchar *orage_log_domains;

static gboolean log_domain_is_enabled (const gchar *domain,
                                       const gsize domain_length)
{
    gsize len;
    const gchar *start;
    const gchar *domains = orage_log_domains;
    const gchar *p = domains;

    while (*p != '\0')
    {
        while (*p == ' ')
            p++;

        if (*p == '\0')
            break;

        start = p;

        while (*p != '\0' && *p != ' ')
            p++;

        len = p - start;

        if ((len == domain_length) && (memcmp (start, domain, domain_length) == 0))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean is_debug_enabled_for_domain (const gchar *domain,
                                             const gsize domain_length)
{
    if (g_strcmp0 (orage_log_domains, "all") == 0)
        return TRUE;

    return log_domain_is_enabled (domain, domain_length);
}

static void get_message (const GLogField *field,
                         const gchar **msg, gsize *msg_len)
{
    *msg = (const gchar *)field->value;
    *msg_len = (field->length < 0) ? strlen (*msg) : (gsize)field->length;
}

static gboolean should_drop_message (const GLogLevelFlags level,
                                     const GLogField *fields,
                                     const gsize n_fields)
{
    gsize i;
    gsize domain_length;
    const gchar *domain;

    if (level & DEFAULT_LEVELS)
        return FALSE;

    if (g_log_get_debug_enabled ())
        return FALSE;

    if ((level & INFO_LEVELS) == 0)
        return TRUE;

    if (orage_log_domains == NULL)
        return TRUE;

    if (g_strcmp0 (orage_log_domains, "all") == 0)
        return FALSE;

    domain = NULL;
    domain_length = 0;
    for (i = 0; i < n_fields; i++)
    {
        if (g_strcmp0 (fields[i].key, "GLIB_DOMAIN") == 0)
        {
            get_message (&fields[i], &domain, &domain_length);
            break;
        }
    }

    if (domain == NULL)
        return TRUE;

    return (log_domain_is_enabled (domain, domain_length) == FALSE);
}

static const gchar *log_level_to_color (const GLogLevelFlags log_level,
                                        const gboolean use_color)
{
    if (use_color == FALSE)
        return "";

    switch (log_level & G_LOG_LEVEL_MASK)
    {
        case G_LOG_LEVEL_ERROR:
            return "\033[1;31m"; /* red */

        case G_LOG_LEVEL_CRITICAL:
            return "\033[1;35m"; /* magenta */

        case G_LOG_LEVEL_WARNING:
            return "\033[1;33m"; /* yellow */

        case G_LOG_LEVEL_MESSAGE:
        case G_LOG_LEVEL_INFO:
        case G_LOG_LEVEL_DEBUG:
            return "\033[1;32m"; /* green */

        default:
            return "";
    }
}

static const gchar *color_set_blue (const gboolean use_color)
{
    return use_color ? "\033[34m" : "";
}

static const gchar *color_set_red (const gboolean use_color)
{
    return use_color ? "\033[31m" : "";
}

static const gchar *color_reset (const gboolean use_color)
{
    return use_color ? "\033[0m" : "";
}

static void append_level_prefix (GString *gstring,
                                 const GLogLevelFlags log_level,
                                 const gboolean use_color)
{
    g_string_append (gstring, log_level_to_color (log_level, use_color));

    switch (log_level & G_LOG_LEVEL_MASK)
    {
        case G_LOG_LEVEL_ERROR:
            g_string_append (gstring, "ERROR");
            break;

        case G_LOG_LEVEL_CRITICAL:
            g_string_append (gstring, "CRITICAL");
            break;

        case G_LOG_LEVEL_WARNING:
            g_string_append (gstring, "WARNING");
            break;

        case G_LOG_LEVEL_MESSAGE:
            g_string_append (gstring, "Message");
            break;

        case G_LOG_LEVEL_INFO:
            g_string_append (gstring, "INFO");
            break;

        case G_LOG_LEVEL_DEBUG:
            g_string_append (gstring, "DEBUG");
            break;

        default:
            g_string_append (gstring, "LOG");
            break;
    }

    g_string_append (gstring, color_reset (use_color));

    if (log_level & G_LOG_FLAG_RECURSION)
        g_string_append (gstring, " (recursed)");

    if (log_level & ALERT_LEVELS)
        g_string_append (gstring, " **");
}

static gboolean is_safe_char (const gchar *p, const gchar *end,
                              const gunichar wc)
{
    const gchar *p1;

    if (wc == '\r')
    {
        p1 = p + 1;
        return (p1 < end) && (*p1 == '\n');
    }

    if ((wc < 0x20) && (wc != '\t') && (wc != '\n') && (wc != '\r'))
        return FALSE;

    if (wc == 0x7f)
        return FALSE;

    if ((wc >= 0x80) && (wc < 0xa0))
        return FALSE;

    return TRUE;
}

static void escape_string (GString *string)
{
    const char *p = string->str;
    gunichar wc;
    gboolean safe;
    gchar *tmp;
    guint pos;

    while (p < string->str + string->len)
    {
        wc = g_utf8_get_char_validated (p, -1);
        if (wc == (gunichar)-1 || wc == (gunichar)-2)
        {
            pos = p - string->str;

            /* Emit invalid UTF-8 as hex escapes */
            tmp = g_strdup_printf ("\\x%02x", (guint)(guchar)*p);
            g_string_erase (string, pos, 1);
            g_string_insert (string, pos, tmp);

            p = string->str + (pos + 4); /* Skip over escape sequence */
            g_free (tmp);
            continue;
        }

        safe = is_safe_char (p, string->str + string->len, wc);

        if (safe == FALSE)
        {
            pos = p - string->str;

            /* Largest char we escape is 0x9f, so we don't have to worry
             * about 8-digit \Uxxxxyyyy.
             */
            tmp = g_strdup_printf ("\\u%04x", wc);
            g_string_erase (string, pos, g_utf8_next_char (p) - p);
            g_string_insert (string, pos, tmp);
            g_free (tmp);

            p = string->str + (pos + 6); /* Skip over escape sequence */
        }
        else
            p = g_utf8_next_char (p);
    }
}

static void append_time_string (GString *gstring, const gboolean use_color)
{
    gint64 now;
    gint millis;
    GDateTime *dt;
    gchar *time_str;

    now = g_get_real_time ();
    millis = (now / 1000) % 1000;

    dt = g_date_time_new_from_unix_local (now / G_USEC_PER_SEC);
    if (dt != NULL)
    {
        time_str = g_date_time_format (dt, "%H:%M:%S");

        g_string_append_printf (gstring, "%s%s.%03d%s: ",
                                color_set_blue (use_color),
                                time_str,
                                millis,
                                color_reset (use_color));

        g_free (time_str);
        g_date_time_unref (dt);
    }
    else
    {
        g_string_append_printf (gstring, "%s(error)%s: ",
                                color_set_red (use_color),
                                color_reset (use_color));
    }
}

static char *log_writer_format_fields_utf8 (GLogLevelFlags level,
                                            const GLogField *fields,
                                            const gsize n_fields,
                                            const gboolean use_color)
{
    static const char NULL_MSG[] = "(null) message";
    gsize i;
    const gchar *log_domain = NULL;
    const gchar *message = NULL;
    const gchar *code_func = NULL;
    const gchar *code_line = NULL;
    gsize log_domain_length;
    gssize message_length = -1;
    gssize code_func_length = -1;
    gssize code_line_length = -1;
    GString *gstring;
    GString *tmp_msg;
    const GLogField *field;
    gboolean add_code_ref;

    for (i = 0; i < n_fields; i++)
    {
        field = &fields[i];

        if (g_strcmp0 (field->key, "MESSAGE") == 0)
        {
            message = (const gchar *)field->value;
            message_length = field->length;
        }
        else if (g_strcmp0 (field->key, "GLIB_DOMAIN") == 0)
            get_message (field, &log_domain, &log_domain_length);
        else if (g_strcmp0 (field->key, "CODE_FUNC") == 0)
        {
            code_func = (const gchar *)field->value;
            code_func_length = field->length;
        }
        else if (g_strcmp0 (field->key, "CODE_LINE") == 0)
        {
            code_line = (const gchar *)field->value;
            code_line_length = field->length;
        }
    }

    gstring = g_string_new (NULL);

    append_time_string (gstring, use_color);

    if (log_domain)
    {
        g_string_append_len (gstring, log_domain, log_domain_length);
        g_string_append_c (gstring, '-');
        add_code_ref = is_debug_enabled_for_domain (log_domain,
                                                    log_domain_length);
    }
    else
    {
        g_string_append (gstring, "** ");
        add_code_ref = FALSE;
    }

    append_level_prefix (gstring, level, use_color);
    g_string_append (gstring, ": ");

    if (add_code_ref && code_func)
    {
        g_string_append_len (gstring, code_func, code_func_length);

        if (code_line)
        {
            g_string_append_c (gstring, '@');
            g_string_append_len (gstring, code_line, code_line_length);
        }

        g_string_append (gstring, ": ");
    }

    if (message)
    {
        tmp_msg = g_string_new_len (message, message_length);
        escape_string (tmp_msg);
        g_string_append_len (gstring, tmp_msg->str, tmp_msg->len);
        g_string_free (tmp_msg, TRUE);
    }
    else
    {
        message = NULL_MSG;
        message_length = sizeof (NULL_MSG) - 1;
        g_string_append_len (gstring, message, message_length);
    }

    g_string_append_c (gstring, '\n');

    return g_string_free (gstring, FALSE);
}

static GLogWriterOutput orage_log_writer (GLogLevelFlags level,
                                          const GLogField *fields,
                                          const gsize n_fields,
                                          gpointer user_data)
{
    int fno;
    char *out;

    g_return_val_if_fail (fields != NULL, G_LOG_WRITER_UNHANDLED);
    g_return_val_if_fail (n_fields > 0, G_LOG_WRITER_UNHANDLED);

    if (should_drop_message (level, fields, n_fields))
        return G_LOG_WRITER_HANDLED;

    fno = fileno (LOG_STREAM);
    if (G_UNLIKELY (fno < 0))
        return G_LOG_WRITER_UNHANDLED;

    out = log_writer_format_fields_utf8 (level, fields, n_fields,
                                         g_log_writer_supports_color (fno));
    fputs (out, LOG_STREAM);
    fflush (LOG_STREAM);
    g_free (out);

    return G_LOG_WRITER_HANDLED;
}

void orage_log_init (void)
{
    const gchar *env;
    static gsize initialized = FALSE;

    if (g_once_init_enter (&initialized))
    {
        env = g_getenv ("G_MESSAGES_DEBUG");
        if (env)
            orage_log_domains = g_strdup (env);
        else if (g_strcmp0 (g_getenv ("DEBUG_INVOCATION"), "1") == 0)
            orage_log_domains = g_strdup ("all");
        else
            orage_log_domains = g_strdup ("");

        g_log_set_writer_func (orage_log_writer, NULL, NULL);

        g_once_init_leave (&initialized, TRUE);
    }
}
