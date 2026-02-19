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

#include "orage-task-runner.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>

struct _orage_task_runner_data
{
    GTask *task;
    guint timer_id;
    GTaskThreadFunc task_func;
    orage_task_runner_conf *conf;
};

typedef struct _orage_task_runner_data orage_task_runner_data;

struct _OrageTaskRunner
{
    GObject __parent__;

    GSList* task_runner_callbacks;
};

struct _OrageTaskRunnerClass
{
    GObjectClass __parent__;
};

G_DEFINE_TYPE (OrageTaskRunner, orage_task_runner, G_TYPE_OBJECT)

static void remove_timer (gpointer data);
static void orage_task_runner_free (orage_task_runner_data *data);

static void orage_task_runner_finalize (GObject *object)
{
    OrageTaskRunner *task_runner = ORAGE_TASK_RUNNER (object);

    orage_task_runne_interrupt (task_runner);
    g_slist_free_full (task_runner->task_runner_callbacks,
                       (GDestroyNotify)orage_task_runner_free);

    (*G_OBJECT_CLASS (orage_task_runner_parent_class)->finalize) (object);
}

static void orage_task_runner_class_init (OrageTaskRunnerClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = orage_task_runner_finalize;
}

static void orage_task_runner_init (G_GNUC_UNUSED OrageTaskRunner *task_runner)
{
}

static void task_ready_callback (G_GNUC_UNUSED GObject *source_object,
                                 G_GNUC_UNUSED GAsyncResult *res,
                                 gpointer user_data)
{
    orage_task_runner_data *task_data = (orage_task_runner_data *)user_data;

    task_data->task = NULL;
}

static gboolean start_task_runner_thread (gpointer data)
{
    GCancellable *cancel;
    orage_task_runner_data *task_data = (orage_task_runner_data *)data;

    if (task_data->task != NULL)
    {
        g_info ("task @ %p already started", data);
        return TRUE;
    }

    cancel = g_cancellable_new ();
    task_data->task = g_task_new (NULL, cancel, task_ready_callback, data);

    g_task_set_task_data (task_data->task, task_data->conf, NULL);
    g_task_run_in_thread (task_data->task, task_data->task_func);

    g_object_unref (cancel);
    g_object_unref (task_data->task);

    return TRUE;
}

static void remove_timer (gpointer data)
{
    const orage_task_runner_data *task_data =
        (const orage_task_runner_data *)data;

    (void)g_source_remove (task_data->timer_id);
}

static gint task_callback_conf_compare (gconstpointer pa, gconstpointer pb)
{
    const orage_task_runner_data *data_a = (const orage_task_runner_data*)pa;
    const orage_task_runner_conf *data_b = (const orage_task_runner_conf*)pb;

    if (data_a->conf == pb)
        return 0;

    if (data_a->conf->period != data_b->period)
        return 1;

    if (strcmp (data_a->conf->description, data_b->description))
        return 1;

    if (strcmp (data_a->conf->command, data_b->command))
        return 1;

    return 0;
}

static void task_runner_and_restart_timer (gpointer data,
                                           G_GNUC_UNUSED gpointer user_data)
{
    orage_task_runner_data *task_data = (orage_task_runner_data *)data;

    remove_timer (data);
    start_task_runner_thread (data);
    task_data->timer_id = g_timeout_add_seconds (task_data->conf->period,
                                                 start_task_runner_thread,
                                                 data);
}

static void cancel_task_runner (gpointer data, G_GNUC_UNUSED gpointer user_data)
{
    orage_task_runner_data *task_data = (orage_task_runner_data *)data;

    if (task_data->task)
        g_cancellable_cancel (g_task_get_cancellable (task_data->task));
}

static orage_task_runner_conf *orage_task_runner_conf_clone (
    const orage_task_runner_conf *conf)
{
    orage_task_runner_conf *cloned_conf;

    cloned_conf = g_new (orage_task_runner_conf, 1);
    cloned_conf->description = g_strdup (conf->description);
    cloned_conf->command = g_strdup (conf->command);
    cloned_conf->period = conf->period;
    cloned_conf->sync_active = conf->sync_active;

    return cloned_conf;
}

static void orage_task_runner_conf_free (orage_task_runner_conf *conf)
{
    g_return_if_fail (conf != NULL);

    g_free (conf->command);
    g_free (conf->description);
    g_free (conf);
}

static void orage_task_runner_free (orage_task_runner_data *data)
{
    remove_timer (data);
    cancel_task_runner (data, NULL);
    orage_task_runner_conf_free (data->conf);
    g_free (data);
}

void orage_task_runner_add (OrageTaskRunner *task_runner,
                            GTaskThreadFunc task_func,
                            orage_task_runner_conf *task_runner_conf)
{
    orage_task_runner_data *task_data = g_new0 (orage_task_runner_data, 1);

    task_data->task_func = task_func;
    task_data->conf = orage_task_runner_conf_clone (task_runner_conf);
    task_data->timer_id = g_timeout_add_seconds (task_runner_conf->period,
                                                 start_task_runner_thread,
                                                 task_data);
    task_runner->task_runner_callbacks =
            g_slist_append (task_runner->task_runner_callbacks, task_data);
}

void orage_task_runner_remove (OrageTaskRunner *task_runner,
                               const orage_task_runner_conf *conf)
{
    GSList *found;

    found = g_slist_find_custom (task_runner->task_runner_callbacks, conf,
                                 task_callback_conf_compare);

    if (found == NULL)
    {
        g_debug ("task runner config not found; nothing to remove");
        return;
    }

    task_runner->task_runner_callbacks =
            g_slist_remove (task_runner->task_runner_callbacks, found->data);

    orage_task_runner_free ((orage_task_runner_data *)found->data);
}

void orage_task_runner_trigger (OrageTaskRunner *task_runner)
{
    g_slist_foreach (task_runner->task_runner_callbacks,
                     task_runner_and_restart_timer, NULL);
}

void orage_task_runne_interrupt (OrageTaskRunner *task_runner)
{
    g_slist_foreach (task_runner->task_runner_callbacks, cancel_task_runner,
                     NULL);
}
