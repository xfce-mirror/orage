/*
 * Copyright (c) 2023-2026 Erkki Moorits
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

#include "orage-sync-ext-command.h"
#include "orage-task-runner.h"
#include "functions.h"
#include <gio/gio.h>
#include <glib.h>

void orage_sync_ext_command (G_GNUC_UNUSED GTask *task,
                             G_GNUC_UNUSED gpointer source_object,
                             gpointer task_data,
                             G_GNUC_UNUSED GCancellable *cancellable)
{
    gboolean succeed;
    GError *error = NULL;
    orage_task_runner_conf *sync_conf = (orage_task_runner_conf *)task_data;

    g_return_if_fail (sync_conf->command != NULL);

    g_message ("starting sync '%s' with command '%s'",
               sync_conf->description, sync_conf->command);

    if (sync_conf->sync_active)
    {
        g_debug ("sync '%s' already active", sync_conf->description);
        return;
    }

    succeed = orage_exec (sync_conf->command, &sync_conf->sync_active, &error);

    if (G_UNLIKELY (succeed == FALSE))
    {
        if (error != NULL)
        {
            g_warning ("sync command '%s' failed: %s",
                       sync_conf->command, error->message);
            g_clear_error (&error);
        }
        else
            g_warning ("sync command '%s' failed", sync_conf->command);
    }
}
