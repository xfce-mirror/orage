/*
 * Copyright (c) 2025 Erkki Moorits
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

#include "orage-month-cell.h"
#include "functions.h"

#define TODAY "today"
#define OUT_OF_MONTH "out-of-month"
#define HIGHLIGHTED "highlighted"

struct _OrageMonthCell
{
    /* Add GtkOverlay as a main container for this widgets */
    GtkBox parent;

    guint single_click_timeout_id;
    GtkWidget *event_box;
    GtkWidget *main_box;
    GtkWidget *day_label;
    GDateTime *date;
    gboolean different_month;
    gboolean selected;
};

enum
{
    SIGNAL_CLICKED,
    SIGNAL_RIGHT_CLICKED,
    SIGNAL_DOUBLE_CLICKED,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};
static guint double_click_time = 250;

G_DEFINE_TYPE (OrageMonthCell, orage_month_cell, GTK_TYPE_BOX)

static void remove_children_except_first (GtkWidget *box)
{
    GList *children, *iter;
    gboolean first = TRUE;

    g_return_if_fail (GTK_IS_BOX (box));

    children = gtk_container_get_children (GTK_CONTAINER (box));
    for (iter = children; iter != NULL; iter = iter->next)
    {
        GtkWidget *child = GTK_WIDGET (iter->data);

        if (first)
        {
            first = FALSE;
            continue;
        }

        gtk_container_remove (GTK_CONTAINER (box), child);
    }

    g_list_free (children);
}

static void block_single_click (gpointer user_data)
{
    OrageMonthCell *self = ORAGE_MONTH_CELL (user_data);

    self->single_click_timeout_id = 0;
}

static gboolean on_event_box_clicked (G_GNUC_UNUSED GtkWidget *widget,
                                      GdkEventButton *event,
                                      gpointer user_data)
{
    OrageMonthCell *self = ORAGE_MONTH_CELL (user_data);

    if (self->date == NULL)
        return TRUE;

    if (event->type == GDK_BUTTON_PRESS)
    {
        if (event->button == 1)
        {
            if (self->single_click_timeout_id == 0)
            {
                self->single_click_timeout_id =
                    g_timeout_add_once (double_click_time, block_single_click,
                                        self);
                orage_month_cell_emit_clicked (self);
            }
        }
        else if (event->button == 3)
            orage_month_cell_emit_right_clicked (self);
    }
    else if (event->type == GDK_2BUTTON_PRESS)
    {
        if (self->single_click_timeout_id)
        {
            g_source_remove (self->single_click_timeout_id);
            self->single_click_timeout_id = 0;
        }

        orage_month_cell_emit_double_clicked (self);
    }

    return TRUE;
}

static void orage_month_cell_class_init (OrageMonthCellClass *klass)
{
    GtkSettings *settings = gtk_settings_get_default ();

    if (settings)
    {
        g_object_get (settings, "gtk-double-click-time", &double_click_time,
                      NULL);
    }

    signals[SIGNAL_CLICKED] = g_signal_new ("clicked",
                                            G_TYPE_FROM_CLASS (klass),
                                            G_SIGNAL_RUN_FIRST,
                                            0, NULL, NULL, NULL,
                                            G_TYPE_NONE,
                                            0);
    signals[SIGNAL_RIGHT_CLICKED] = g_signal_new ("right-clicked",
                                                  G_TYPE_FROM_CLASS (klass),
                                                  G_SIGNAL_RUN_FIRST,
                                                  0, NULL, NULL, NULL,
                                                  G_TYPE_NONE,
                                                  0);
    signals[SIGNAL_DOUBLE_CLICKED] = g_signal_new ("double-clicked",
                                                   G_TYPE_FROM_CLASS (klass),
                                                   G_SIGNAL_RUN_FIRST,
                                                   0, NULL, NULL, NULL,
                                                   G_TYPE_NONE,
                                                   0);
}

static void orage_month_cell_init (OrageMonthCell *self)
{
    self->different_month = TRUE;

    gtk_widget_set_name (GTK_WIDGET (self), "orage-month-cell");

    self->event_box = gtk_event_box_new ();
    gtk_box_pack_start (GTK_BOX (self), self->event_box, TRUE, TRUE, 0);

    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER(self->event_box), self->main_box);

    self->day_label = gtk_label_new (NULL);
    gtk_style_context_add_class (gtk_widget_get_style_context (self->day_label),
                                 "day-label");

    gtk_box_pack_start (GTK_BOX (self->main_box), self->day_label, FALSE, FALSE,
                        0);

    g_signal_connect (self->event_box, "button-press-event",
                      G_CALLBACK (on_event_box_clicked), self);
}

GtkWidget *orage_month_cell_new (void)
{
    return g_object_new (ORAGE_MONTH_CELL_TYPE, NULL);
}

void orage_month_cell_clear (OrageMonthCell *self)
{
    GtkStyleContext *context;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    orage_gdatetime_unref (self->date);
    self->date = NULL;
    self->different_month = TRUE;

    context = gtk_widget_get_style_context (GTK_WIDGET (self));
    gtk_style_context_remove_class (context, TODAY);
    gtk_style_context_remove_class (context, HIGHLIGHTED);

    gtk_label_set_text (GTK_LABEL (self->day_label), NULL);

    remove_children_except_first (self->main_box);
}

void orage_month_cell_set_date (OrageMonthCell *self, GDateTime *date)
{
    GtkStyleContext *context;
    GDateTime *today;
    gchar *text;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    if (self->date &&
        date &&
        orage_gdatetime_days_between (self->date, date) == 0)
    {
        return;
    }

    orage_gdatetime_unref (self->date);
    self->date = g_date_time_ref (date);

    text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));

    gtk_label_set_text (GTK_LABEL (self->day_label), text);
    g_free (text);

    context = gtk_widget_get_style_context (GTK_WIDGET (self));
    today = g_date_time_new_now_local ();
    if (orage_gdatetime_days_between (self->date, today) == 0)
        gtk_style_context_add_class (context, TODAY);
    else
        gtk_style_context_remove_class (context, TODAY);

    g_date_time_unref (today);
}

GDateTime *orage_month_cell_get_date (OrageMonthCell *self)
{
    g_return_val_if_fail (ORAGE_IS_MONTH_CELL (self), NULL);

    return g_date_time_ref (self->date);
}

void orage_month_cell_set_different_month (OrageMonthCell *self,
                                           const gboolean different)
{
    GtkStyleContext *context;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    if (self->different_month == different)
        return;

    self->different_month = different;
    context = gtk_widget_get_style_context (GTK_WIDGET (self));

    if (different)
        gtk_style_context_add_class (context, OUT_OF_MONTH);
    else
        gtk_style_context_remove_class (context, OUT_OF_MONTH);
}

void orage_month_cell_set_highlight (OrageMonthCell *self,
                                     const gboolean highlighted)
{
    GtkStyleContext *context;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    if (self->different_month)
        return;

    context = gtk_widget_get_style_context (GTK_WIDGET (self));

    if (highlighted)
        gtk_style_context_add_class (context, HIGHLIGHTED);
    else
        gtk_style_context_remove_class (context, HIGHLIGHTED);
}

void orage_month_cell_set_selected (OrageMonthCell *self,
                                    const gboolean selected)
{
    GtkStateFlags flags;

    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    if (self->selected == selected)
        return;

    self->selected = selected;

    flags = gtk_widget_get_state_flags (GTK_WIDGET (self));

    if (selected)
        flags |= GTK_STATE_FLAG_SELECTED;
    else
        flags &= ~GTK_STATE_FLAG_SELECTED;

    gtk_widget_set_state_flags (GTK_WIDGET (self), flags, TRUE);
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

void orage_month_cell_set_css_last (OrageMonthCell *self,
                                    const gboolean last_row,
                                    const gboolean last_column)
{
    GtkStyleContext *context;

    if (last_row || last_column)
    {
        context = gtk_widget_get_style_context (GTK_WIDGET (self));

        if (last_row)
            gtk_style_context_add_class (context, "last-row");

        if (last_column)
            gtk_style_context_add_class (context, "last-column");
    }
}

void orage_month_cell_add_widget (OrageMonthCell *self, GtkWidget *widget)
{
    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    gtk_box_pack_start (GTK_BOX (self->main_box), widget, FALSE, FALSE, 0);
    gtk_widget_show (widget);
}

void orage_month_cell_add_text (OrageMonthCell *self, const gchar *text)
{
    GtkWidget *label;

    label = gtk_label_new (text);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand (label, FALSE);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
    gtk_widget_set_tooltip_text (label, text);

    orage_month_cell_add_widget (self, label);
}

void orage_month_cell_emit_clicked (OrageMonthCell *self)
{
    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    g_signal_emit (self, signals[SIGNAL_CLICKED], 0);
}

void orage_month_cell_emit_right_clicked (OrageMonthCell *self)
{
    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    g_signal_emit (self, signals[SIGNAL_RIGHT_CLICKED], 0);
}

void orage_month_cell_emit_double_clicked (OrageMonthCell *self)
{
    g_return_if_fail (ORAGE_IS_MONTH_CELL (self));

    g_signal_emit (self, signals[SIGNAL_DOUBLE_CLICKED], 0);
}
