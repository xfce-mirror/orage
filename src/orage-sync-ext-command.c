/*
 * Copyright (c) 2023 Erkki Moorits
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
    GError *error = NULL;
    orage_task_runner_conf *sync_conf = (orage_task_runner_conf *)task_data;

    g_message ("sync '%s'", sync_conf->site);

    if (sync_conf->sync_active)
    {
        g_debug ("%s sync already active", G_STRFUNC);
        return;
    }

    if (orage_exec (sync_conf->site, &sync_conf->sync_active, &error) == FALSE)
    {
        g_message ("sync command '%s' failed with message '%s'",
                   sync_conf->site, error->message);

        g_clear_error (&error);
    }
}
