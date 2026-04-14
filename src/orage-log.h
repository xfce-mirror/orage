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

#ifndef ORAGE_LOG_H
#define ORAGE_LOG_H 1

/* Handles Orage debug logging. Inspired by GNOME Calendar and GLib logging.
 *
 * Orage adds a lightweight filtering layer on top of GLib logging. It does
 * not modify or replace GLib logging, but decides whether a message should
 * be emitted before passing it to GLib.
 *
 * Logging is controlled by two environment variables:
 *
 * - ORAGE_LOG_LEVEL: minimum enabled log level
 * - G_MESSAGES_DEBUG: enabled log domains (GLib semantics)
 *
 *
 * Level filtering (ORAGE_LOG_LEVEL)
 * ---------------------------------
 *
 * ORAGE_LOG_LEVEL defines the minimum severity of messages allowed by Orage.
 * Messages below the selected level are suppressed.
 *
 * If ORAGE_LOG_LEVEL is not set, no additional filtering is applied.
 *
 * Supported values:
 * - "error"
 * - "critical"
 * - "warning"
 * - "message"
 * - "info"
 * - "debug"
 *
 *
 * Domain filtering (G_MESSAGES_DEBUG)
 * ----------------------------------
 *
 * Domain filtering follows GLib behaviour and applies only to INFO and
 * DEBUG messages.
 *
 * - If unset: INFO and DEBUG are disabled
 * - If "all": all domains are enabled
 * - Otherwise: only matching domains are enabled
 *
 *
 * Final decision
 * --------------
 *
 * A message is emitted if:
 *
 * - It passes ORAGE_LOG_LEVEL filtering (if enabled), and
 * - For INFO/DEBUG messages: G_MESSAGES_DEBUG allows the domain
 *
 * After filtering, messages are forwarded to GLib, which handles
 * formatting and output.
 */

#include <glib.h>

G_BEGIN_DECLS

/**
 * orage_log_init:
 *
 * Initializes Orage logging.
 *
 * This function configures domain-based debug logging for Orage. The set of
 * log domains for which debug output is enabled is read from the environment
 * variable %G_MESSAGES_DEBUG, using the same semantics as GLib.
 *
 * If %G_MESSAGES_DEBUG is not set and the environment variable
 * %DEBUG_INVOCATION is set to "1", debug logging is enabled for all domains
 * (equivalent to %G_MESSAGES_DEBUG=all).
 *
 * The function installs Orage's custom log writer and is safe to call multiple
 * times; initialization is performed only once.
 *
 * This function must be called early during application startup, before any
 * log output is expected.
 */
void orage_log_init (void);

/**
 * orage_log_is_message_enabled:
 * @level: log message severity level (GLogLevelFlags)
 * @fields: structured log fields array
 * @n_fields: number of elements in @fields
 *
 * Determines whether a log message should be emitted based on the current
 * logging configuration.
 *
 * Returns: %TRUE if the message should be logged, %FALSE otherwise.
 */
gboolean orage_log_is_message_enabled (GLogLevelFlags level,
                                       const GLogField *fields,
                                       gsize n_fields);

/**
 * orage_log_update_levels_from_env:
 *
 * Updates the internal log level filtering configuration based on the
 * ORAGE_LOG_LEVEL environment variable.
 *
 * The environment variable defines the minimum log level to be shown.
 * All less severe log levels are internally suppressed by Orage.
 *
 * Supported values are: "debug", "info", "message", "warning",
 * "critical", and "error". Any unknown value results in no filtering.
 *
 * This function only affects Orage's internal log filtering logic.
 * It does NOT modify or override GLib's logging system behaviour.
 * In particular, it does not change how GLib routes, formats, or
 * emits log messages.
 *
 * This function is typically called during initialization, but may
 * also be used to refresh configuration at runtime.
 */
void orage_log_update_levels_from_env (void);

G_END_DECLS

#endif
