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

#ifndef ORAGE_TASK_RUNNER_H
#define ORAGE_TASK_RUNNER_H 1

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _OrageTaskRunnerClass OrageTaskRunnerClass;
typedef struct _OrageTaskRunner      OrageTaskRunner;

#define ORAGE_TASK_RUNNER_TYPE (orage_task_runner_get_type ())
#define ORAGE_TASK_RUNNER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), ORAGE_TASK_RUNNER_TYPE, OrageTaskRunner))

GType orage_task_runner_get_type (void) G_GNUC_CONST;

struct _orage_task_runner_conf
{
    gchar *description;
    gchar *command;
    guint period;
    gboolean sync_active;
};

typedef struct _orage_task_runner_conf orage_task_runner_conf;

/** Add new task function.
 *  @param task_runner instance of task runner
 *  @param task_func task function
 *  @param conf periodic task configuration, data is owned by caller
 */
void orage_task_runner_add (OrageTaskRunner *task_runner,
                            GTaskThreadFunc task_func,
                            orage_task_runner_conf *conf);

/** Remove task from task runner.
 *  @param task_runner instance of task runner
 *  @param conf periodic task configuration, data is owned by caller
 */
void orage_task_runner_remove (OrageTaskRunner *task_runner,
                               const orage_task_runner_conf *conf);

/** Run all tasks.
 *  @param task_runner instance of task runner
 */
void orage_task_runner_trigger (OrageTaskRunner *task_runner);

/** Interrupt all running tasks.
 *  @param task_runner instance of task runner
 */
void orage_task_runne_interrupt (OrageTaskRunner *task_runner);

G_END_DECLS

#endif
