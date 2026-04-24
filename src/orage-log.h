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

G_END_DECLS

#endif
