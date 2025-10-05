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

#include <config.h>

#include "orage-window-next.h"

#include "event-list.h"
#include "functions.h"
#include "ical-code.h"
#include "interface.h"
#include "orage-about.h"
#include "orage-appointment-window.h"
#include "orage-css.h"
#include "orage-event.h"
#include "orage-i18n.h"
#include "orage-month-cell.h"
#include "orage-month-view.h"
#include "orage-time-utils.h"
#include "orage-week-window.h"
#include "parameters.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <stdio.h>

#define MONTH_PAGE "month"

#define FORMAT_BOLD "<b> %s </b>"

typedef enum
{
    ORAGE_WINDOW_ACTION_FILE_MENU,
    ORAGE_WINDOW_ACTION_NEW_APPOINTMENT,
#ifdef ENABLE_SYNC
    ORAGE_WINDOW_ACTION_REFRESH,
#endif
    ORAGE_WINDOW_ACTION_EXCHANGE,
    ORAGE_WINDOW_ACTION_CLOSE,
    ORAGE_WINDOW_ACTION_QUIT,

    ORAGE_WINDOW_ACTION_EDIT_MENU,
    ORAGE_WINDOW_ACTION_PREFERENCES,

    ORAGE_WINDOW_ACTION_VIEW_MENU,
    ORAGE_WINDOW_ACTION_VIEW_DATE,
    ORAGE_WINDOW_ACTION_VIEW_WEEK,
    ORAGE_WINDOW_ACTION_SELECT_TODAY,

    ORAGE_WINDOW_ACTION_HELP_MENU,
    ORAGE_WINDOW_ACTION_HELP,
    ORAGE_WINDOW_ACTION_ABOUT
} OrageWindowAction;

struct _OrageWindowNext
{
    GtkApplicationWindow __parent__;

    GDateTime *selected_date;

    GtkAccelGroup *accel_group;
    GtkWidget *menubar;

    GtkWidget *file_menu;
    GtkWidget *file_menu_label;
    GtkWidget *file_menu_new;
    GtkWidget *file_menu_close;
    GtkWidget *file_menu_quit;
    GtkWidget *paned;
    GtkStack *stack_view;
    GtkWidget *main_view;
    GtkWidget *main_box;
    OrageMonthView *month_view;

    GtkWidget *back_button;
    GtkWidget *next_button;

    GtkWidget *info_box;

    /* Info box contents. */
    GtkWidget *todo_box;
    GtkWidget *todo_rows_box;

    GtkWidget *event_box;
    GtkWidget *event_label;
    GtkWidget *event_rows_box;
};

static void orage_window_next_constructed (GObject *object);
static void orage_window_next_finalize (GObject *object);

static void orage_window_next_interface_init (OrageWindowInterface *interface);
static void orage_window_next_restore_state (OrageWindowNext *window);

static void orage_window_next_add_days (OrageWindowNext *self, gint days);

static void on_new_activated (OrageWindowNext *window);

#ifdef ENABLE_SYNC
static void on_refresh_activated (OrageWindowNext *window);
#endif

static void on_exchange_activated (OrageWindowNext *window);
static void on_close_activated (OrageWindowNext *window);
static void on_quit_activated (OrageWindowNext *window);
static void on_preferences_activated (OrageWindowNext *window);
static void on_view_selected_date_activated (OrageWindowNext *window);
static void on_view_selected_week_activated (OrageWindowNext *window);
static void on_select_today_activated (OrageWindowNext *window);
static void on_help_activated (OrageWindowNext *window);
static void on_about_activated (OrageWindowNext *window);

static XfceGtkActionEntry action_entries[] =
{
    {ORAGE_WINDOW_ACTION_FILE_MENU, "<Actions>/OrageWindow/file-menu", "", XFCE_GTK_MENU_ITEM, N_("_File"), NULL, NULL, NULL},
    {ORAGE_WINDOW_ACTION_NEW_APPOINTMENT, "<Actions>/OrageWindow/new-appointment", "<control>n", XFCE_GTK_IMAGE_MENU_ITEM, N_("New"), N_("Create a new appointment"), "document-new", G_CALLBACK (on_new_activated)},
#ifdef ENABLE_SYNC
    {ORAGE_WINDOW_ACTION_REFRESH, "<Actions>/OrageWindow/refresh", "", XFCE_GTK_IMAGE_MENU_ITEM, N_("Refresh"), N_("Reload external files"), "view-refresh", G_CALLBACK (on_refresh_activated)},
#endif
    {ORAGE_WINDOW_ACTION_EXCHANGE, "<Actions>/OrageWindow/exchange", "", XFCE_GTK_MENU_ITEM, N_("_Exchange data"), N_("Import/export data"), "", G_CALLBACK (on_exchange_activated)},
    {ORAGE_WINDOW_ACTION_CLOSE, "<Actions>/OrageWindow/close", "<control>w", XFCE_GTK_IMAGE_MENU_ITEM, N_("Close"), N_("Close window"), "window-close", G_CALLBACK (on_close_activated)},
    {ORAGE_WINDOW_ACTION_QUIT, "<Actions>/OrageWindow/quit", "<control>q", XFCE_GTK_IMAGE_MENU_ITEM, N_("_Quit"), N_("Quit program"), "application-exit", G_CALLBACK (on_quit_activated)},

    {ORAGE_WINDOW_ACTION_EDIT_MENU, "<Actions>/OrageWindow/edit-menu", "", XFCE_GTK_MENU_ITEM, N_ ("_Edit"), NULL, NULL, NULL},
    {ORAGE_WINDOW_ACTION_PREFERENCES, "<Actions>/OrageWindow/preferences", "", XFCE_GTK_IMAGE_MENU_ITEM, N_("_Preferences"), N_("Orage preferences"), "preferences-system", G_CALLBACK (on_preferences_activated)},

    {ORAGE_WINDOW_ACTION_VIEW_MENU, "<Actions>/OrageWindow/view-menu", "", XFCE_GTK_MENU_ITEM, N_ ("_View"), NULL, NULL, NULL},
    {ORAGE_WINDOW_ACTION_VIEW_DATE, "<Actions>/OrageWindow/view-date", "", XFCE_GTK_MENU_ITEM, N_("View selected _date"), N_("View selected date"), "", G_CALLBACK (on_view_selected_date_activated)},
    {ORAGE_WINDOW_ACTION_VIEW_WEEK, "<Actions>/OrageWindow/view-week", "", XFCE_GTK_MENU_ITEM, N_("View selected _week"), N_("View selected _week"), "", G_CALLBACK (on_view_selected_week_activated)},
    {ORAGE_WINDOW_ACTION_SELECT_TODAY, "<Actions>/OrageWindow/select-today", "", XFCE_GTK_MENU_ITEM, N_("Select _Today"), N_("Go to today"), "", G_CALLBACK (on_select_today_activated)},

    {ORAGE_WINDOW_ACTION_HELP_MENU, "<Actions>/OrageWindow/help-menu", "", XFCE_GTK_MENU_ITEM, N_("_Help"), NULL, NULL, NULL},
    {ORAGE_WINDOW_ACTION_HELP, "<Actions>/OrageWindow/help", "F1", XFCE_GTK_IMAGE_MENU_ITEM, N_("_Help"), N_("Display Orage user manual"), "help-browser", G_CALLBACK (on_help_activated)},
    {ORAGE_WINDOW_ACTION_ABOUT, "<Actions>/OrageWindow/about", "", XFCE_GTK_IMAGE_MENU_ITEM, N_("_About"), N_("Display information about Orage"), "help-about", G_CALLBACK (on_about_activated)},
};

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id (action_entries, G_N_ELEMENTS (action_entries), id)

G_DEFINE_TYPE_WITH_CODE (OrageWindowNext, orage_window_next, GTK_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (ORAGE_WINDOW_TYPE, orage_window_next_interface_init))

static void on_menu_item_new_activated (G_GNUC_UNUSED GtkMenuItem* item,
                                        gpointer user_data)
{
    GDateTime *gdt;
    GtkWidget *appointment_window;

    g_return_if_fail (ORAGE_IS_WINDOW (user_data));

    /* Orage has always a day selected here, so it is safe to read it. */
    gdt = orage_window_next_get_selected_date (ORAGE_WINDOW (user_data));
    appointment_window = orage_appointment_window_new (gdt);
    gtk_window_present (GTK_WINDOW (appointment_window));
    g_date_time_unref (gdt);
}

static void on_new_activated (OrageWindowNext *window)
{
    on_menu_item_new_activated (NULL, window);
}

#ifdef ENABLE_SYNC
static void on_refresh_activated (G_GNUC_UNUSED OrageWindowNext *window)
{
    OrageApplication *app = ORAGE_APPLICATION (g_application_get_default ());

    orage_task_runner_trigger (orage_application_get_sync (app));
}
#endif

static void on_exchange_activated (G_GNUC_UNUSED OrageWindowNext *window)
{
    orage_external_interface ();
}

static void on_close_activated (OrageWindowNext *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (window));

    gtk_window_close (GTK_WINDOW (window));
}

static void on_quit_activated (OrageWindowNext *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (window));

    /* TODO/FIXME: Close the program after gtk_widget_destroy(). Currently,
     * Orage stores its configuration after the shutdown command, but the
     * write_parameters() function fails to run properly if called after
     * gtk_widget_destroy().
     */
#if 1
    GtkApplication *app = GTK_APPLICATION (g_application_get_default ());
    g_application_quit (G_APPLICATION (app));
#else
    gtk_widget_destroy (GTK_WIDGET (window));
#endif
}

static void on_preferences_activated (G_GNUC_UNUSED OrageWindowNext *window)
{
    show_parameters ();
}

static void on_view_selected_date_activated (
    G_GNUC_UNUSED OrageWindowNext *window)
{
    (void)create_el_win (NULL);
}

static void on_view_selected_week_activated (OrageWindowNext *window)
{
    orage_week_window_build (window->selected_date);
}

static void on_select_today_activated (OrageWindowNext *window)
{
    orage_window_next_select_today (ORAGE_WINDOW (window));
}

static void on_help_activated (G_GNUC_UNUSED OrageWindowNext *window)
{
    orage_open_help_page ();
}

static void on_about_activated (OrageWindowNext *window)
{
    g_return_if_fail (GTK_IS_WINDOW (window));

    orage_show_about (GTK_WINDOW (window));
}

static void on_next_clicked (G_GNUC_UNUSED GtkButton *button,
                             gpointer user_data)
{
    OrageWindowNext *nw = ORAGE_WINDOW_NEXT (user_data);
    GDateTime *gdt;
    const gchar *visible_name =
        gtk_stack_get_visible_child_name (nw->stack_view);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
    {
        gdt = nw->selected_date;
        nw->selected_date = g_date_time_add_months (nw->selected_date, 1);
        g_date_time_unref (gdt);

        orage_month_view_set_month (nw->month_view, nw->selected_date);
    }
    else
    {
        /* Requested page handling is not yet implemented. */
        g_return_if_reached ();
    }
}

static void on_back_clicked (G_GNUC_UNUSED GtkButton *button,
                             gpointer user_data)
{
    OrageWindowNext *nw = ORAGE_WINDOW_NEXT (user_data);
    GDateTime *gdt;
    const gchar *visible_name =
        gtk_stack_get_visible_child_name (nw->stack_view);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
    {
        gdt = nw->selected_date;
        nw->selected_date = g_date_time_add_months (nw->selected_date, -1);
        g_date_time_unref (gdt);

        orage_month_view_set_month (nw->month_view, nw->selected_date);
    }
    else
    {
        /* Requested page handling is not yet implemented. */
        g_return_if_reached ();
    }
}

static gboolean on_key_press (GtkWidget *widget, GdkEventKey *event,
                              G_GNUC_UNUSED gpointer user_data)
{
    OrageWindowNext *self = ORAGE_WINDOW_NEXT (widget);
    gboolean alt_down = (event->state & GDK_MOD1_MASK) != 0;
    gboolean result;

    switch (event->keyval)
    {
        case GDK_KEY_Left:
            if (alt_down)
                gtk_button_clicked (GTK_BUTTON (self->back_button));
            else
                orage_window_next_add_days (self, -1);

            result = TRUE;
            break;

        case GDK_KEY_Right:
            if (alt_down)
                gtk_button_clicked (GTK_BUTTON (self->next_button));
            else
                orage_window_next_add_days (self, 1);

            result = TRUE;
            break;

        case GDK_KEY_Up:
            orage_window_next_add_days (self, -7);
            result = TRUE;
            break;

        case GDK_KEY_Down:
            orage_window_next_add_days (self, 7);
            result = TRUE;
            break;

        default:
            result = FALSE;
            break;
    }

    return result;
}

static void on_date_selected (G_GNUC_UNUSED OrageMonthView *view,
                              OrageMonthCell *cell,
                              gpointer user_data)
{
    GDateTime *gdt;

    gdt = orage_month_cell_get_date (cell);
    orage_window_next_select_date (ORAGE_WINDOW (user_data), gdt);
    g_date_time_unref (gdt);

    orage_window_next_build_events (ORAGE_WINDOW (user_data));
}

static void on_date_selected_right_clicked (G_GNUC_UNUSED OrageMonthView *view,
                                            OrageMonthCell *cell,
                                            gpointer user_data)
{
    GtkWidget *menu;
    GtkWidget *new_event;
    GDateTime *gdt;

    gdt = orage_month_cell_get_date (cell);
    orage_window_next_select_date (ORAGE_WINDOW (user_data), gdt);
    g_date_time_unref (gdt);

    menu = gtk_menu_new ();

    new_event = gtk_menu_item_new_with_label (_("New event"));

    g_signal_connect (new_event, "activate",
                      G_CALLBACK (on_menu_item_new_activated), user_data);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), new_event);

    gtk_widget_show_all (menu);
    gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static void on_date_selected_double_clicked (G_GNUC_UNUSED OrageMonthView *view,
                                             OrageMonthCell *cell,
                                             G_GNUC_UNUSED gpointer user_data)
{
    GDateTime *gdt;

    gdt = orage_month_cell_get_date (cell);

    if (g_par.show_days)
        orage_week_window_build (gdt);
    else
        (void)create_el_win (gdt);

    g_date_time_unref (gdt);
}

static void on_month_reload_requested (G_GNUC_UNUSED OrageMonthView *view,
                                       gpointer user_data)
{
    orage_window_next_update_appointments (ORAGE_WINDOW (user_data));
}

static void cb_update_month_events (void *caller_param, OrageEvent *event)
{
    OrageMonthView *month_view = ORAGE_MONTH_VIEW (caller_param);

    orage_month_view_set_event (month_view, event);
}

static void menu_clean (GtkMenu *menu)
{
    GList *children, *lp;
    GtkWidget *submenu;

    children = gtk_container_get_children (GTK_CONTAINER (menu));
    for (lp = children; lp != NULL; lp = lp->next)
    {
        submenu = gtk_menu_item_get_submenu (lp->data);
        if (submenu != NULL)
            gtk_widget_destroy (submenu);

        gtk_container_remove (GTK_CONTAINER (menu), lp->data);
    }

    g_list_free (children);
}

/** Remove all children from GtkBox. */
static void clear_gtk_box (GtkWidget *box)
{
    GList *iter;
    GList *children;

    children = gtk_container_get_children (GTK_CONTAINER (box));
    for (iter = children; iter != NULL; iter = g_list_next (iter))
        gtk_widget_destroy (GTK_WIDGET (iter->data));

    g_list_free (children);
}

static gchar *create_event_label_text (GDateTime *gdt)
{
    GDateTime *gdt_end;
    gchar *event_label_str, *start_date_str, *end_str;

    start_date_str = orage_gdatetime_to_i18_time (gdt, TRUE);
    if (g_par.show_event_days == 1)
        event_label_str = g_strdup_printf (_("Events for %s:"), start_date_str);
    else
    {
        gdt_end = g_date_time_add_days (gdt, g_par.show_event_days - 1);
        end_str = orage_gdatetime_to_i18_time (gdt_end, TRUE);
        g_date_time_unref (gdt_end);
        event_label_str = g_strdup_printf (_("Events for %s - %s:"),
                                           start_date_str, end_str);
        g_free (end_str);
    }

    g_free (start_date_str);

    return event_label_str;
}

static void orage_window_next_update_file_menu (OrageWindowNext *self,
                                                GtkWidget *menu)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (self));

    menu_clean (GTK_MENU (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_NEW_APPOINTMENT),
        G_OBJECT (self),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
#ifdef ENABLE_SYNC
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_REFRESH), G_OBJECT (self),
        GTK_MENU_SHELL (menu));
#endif
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_EXCHANGE), G_OBJECT (self),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_CLOSE), G_OBJECT (self),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_QUIT), G_OBJECT (self),
        GTK_MENU_SHELL (menu));

    gtk_widget_show_all (GTK_WIDGET (menu));
}

static void orage_window_next_update_edit_menu (OrageWindowNext *self,
                                                GtkWidget *menu)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (self));

    menu_clean (GTK_MENU (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_PREFERENCES), G_OBJECT (self),
        GTK_MENU_SHELL (menu));

    gtk_widget_show_all (GTK_WIDGET (menu));
}

static void orage_window_next_update_view_menu (OrageWindowNext *self,
                                                GtkWidget *menu)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (self));

    menu_clean (GTK_MENU (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_VIEW_DATE), G_OBJECT (self),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_VIEW_WEEK), G_OBJECT (self),
        GTK_MENU_SHELL (menu));

    xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_SELECT_TODAY), G_OBJECT (self),
        GTK_MENU_SHELL (menu));

    gtk_widget_show_all (GTK_WIDGET (menu));
}

static void orage_window_next_update_help_menu (OrageWindowNext *self,
                                                GtkWidget *menu)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (self));

    menu_clean (GTK_MENU (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_HELP), G_OBJECT (self),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_ABOUT), G_OBJECT (self),
        GTK_MENU_SHELL (menu));

    gtk_widget_show_all (GTK_WIDGET (menu));
}

static GDateTime *orage_window_next_get_first_date (OrageWindowNext *self)
{
    const gchar *visible_name =
        gtk_stack_get_visible_child_name (self->stack_view);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
        return orage_month_view_get_first_date (self->month_view);
    else
    {
        /* Requested page handinling is not yet implemented. */
        g_return_val_if_reached (g_date_time_new_now_local ());
    }
}

static GDateTime *orage_window_next_get_last_date (OrageWindowNext *self)
{
    const gchar *visible_name =
        gtk_stack_get_visible_child_name (self->stack_view);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
        return orage_month_view_get_last_date (self->month_view);
    else
    {
        /* Requested page handinling is not yet implemented. */
        g_return_val_if_reached (g_date_time_new_now_local ());
    }
}

static void orage_window_next_add_days (OrageWindowNext *self,
                                        const gint days)
{
    OrageWindow *gen_window;
    GDateTime *gdt;
    const gchar *visible_name =
        gtk_stack_get_visible_child_name (self->stack_view);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
    {
        gdt = self->selected_date;
        self->selected_date = g_date_time_add_days (gdt, days);
        g_date_time_unref (gdt);
        gen_window = ORAGE_WINDOW (self);
        orage_month_view_mark_date (self->month_view, self->selected_date);
        orage_window_next_build_events (gen_window);
    }
    else
    {
        /* Requested page handinling is not yet implemented. */
        g_return_if_reached ();
    }
}

static void orage_window_next_restore_state (OrageWindowNext *window)
{
    GtkWindow *gtk_window = GTK_WINDOW (window);

    gtk_window_set_position (gtk_window, GTK_WIN_POS_NONE);

    if (g_par.size_x || g_par.size_y)
        gtk_window_set_default_size (gtk_window, g_par.size_x, g_par.size_y);

    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move (gtk_window, g_par.pos_x, g_par.pos_y);

    gtk_paned_set_position (GTK_PANED (window->paned), g_par.paned_pos);
}

static void orage_window_next_add_menu (OrageWindowNext *self,
                                        OrageWindowAction action,
                                        GCallback cb_update_menu)
{
    GtkWidget *item;
    GtkWidget *submenu;
    GtkWidget *menu = self->menubar;

    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (self));

    item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (action),
                                                     G_OBJECT (self),
                                                     GTK_MENU_SHELL (menu));

    submenu = g_object_new (GTK_TYPE_MENU, NULL);
    gtk_menu_set_accel_group (GTK_MENU (submenu), self->accel_group);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (submenu));
    g_signal_connect_swapped (G_OBJECT (submenu), "show",
                              G_CALLBACK (cb_update_menu), self);
}

static void orage_window_next_add_menubar (OrageWindowNext *self)
{
    self->menubar = gtk_menu_bar_new ();
    orage_window_next_add_menu (self, ORAGE_WINDOW_ACTION_FILE_MENU,
                                G_CALLBACK (orage_window_next_update_file_menu));
    orage_window_next_add_menu (self, ORAGE_WINDOW_ACTION_EDIT_MENU,
                                G_CALLBACK (orage_window_next_update_edit_menu));
    orage_window_next_add_menu (self, ORAGE_WINDOW_ACTION_VIEW_MENU,
                                G_CALLBACK (orage_window_next_update_view_menu));
    orage_window_next_add_menu (self, ORAGE_WINDOW_ACTION_HELP_MENU,
                                G_CALLBACK (orage_window_next_update_help_menu));
    gtk_widget_show_all (self->menubar);
}

static void orage_window_next_add_todo_box (OrageWindowNext *self)
{
    GtkScrolledWindow *sw;
    GtkWidget *todo_label;

    self->todo_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    g_object_set (self->todo_box, "vexpand", TRUE,
                                  "valign", GTK_ALIGN_FILL,
                                  NULL);
    gtk_box_pack_start (GTK_BOX (self->info_box), self->todo_box, TRUE,
                        TRUE, 0);
    todo_label = gtk_label_new (_("To do:"));
    gtk_style_context_add_class (gtk_widget_get_style_context (todo_label),
                                 "todo-label");

    gtk_box_pack_start (GTK_BOX (self->todo_box), todo_label, FALSE, FALSE, 0);
    g_object_set (todo_label, "xalign", 0.0, "yalign", 0.5, NULL);
    sw = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
    gtk_scrolled_window_set_policy (sw, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (sw, GTK_SHADOW_NONE);
    g_object_set (sw, "vexpand", TRUE, NULL);
    gtk_box_pack_start (GTK_BOX (self->todo_box), GTK_WIDGET (sw), TRUE, TRUE,
                        0);
    self->todo_rows_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add (GTK_CONTAINER (sw), self->todo_rows_box);
}

static void orage_window_next_add_event_box (OrageWindowNext *self)
{
    GtkScrolledWindow *sw;
    GtkWidget *event_label;
    gchar *event_label_text;

    self->event_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    g_object_set (self->event_box, "vexpand", TRUE,
                                   "valign", GTK_ALIGN_FILL,
                                   NULL);
    gtk_box_pack_start (GTK_BOX (self->info_box), self->event_box, TRUE,
                        TRUE, 0);
    event_label = gtk_label_new (NULL);
    self->event_label = event_label;
    if (g_par.show_event_days)
    {
        event_label_text = create_event_label_text (self->selected_date);
        gtk_style_context_add_class (gtk_widget_get_style_context (event_label),
                                     "event-label");
        gtk_label_set_text (GTK_LABEL (event_label), event_label_text);
        g_free (event_label_text);
    }

    g_object_set (event_label, "xalign", 0.0, "yalign", 0.5, NULL);
    gtk_box_pack_start (GTK_BOX (self->event_box), event_label, FALSE, FALSE,
                        0);
    sw = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
    gtk_scrolled_window_set_policy (sw, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (sw, GTK_SHADOW_NONE);
    g_object_set (sw, "expand", TRUE, NULL);
    gtk_box_pack_start (GTK_BOX (self->event_box), GTK_WIDGET (sw), TRUE, TRUE,
                        0);
    self->event_rows_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add (GTK_CONTAINER (sw), self->event_rows_box);
}

static void insert_rows (GList **list, GDateTime *gdt, xfical_type ical_type,
                         gchar *file_type)
{
    xfical_appt *appt;

    for (appt = xfical_appt_get_next_on_day (gdt, TRUE, 0, ical_type, file_type);
         appt;
         appt = xfical_appt_get_next_on_day (gdt, FALSE, 0, ical_type , file_type))
    {
        *list = g_list_prepend (*list, appt);
    }
}

static gint event_order (gconstpointer a, gconstpointer b)
{
    xfical_appt *appt1, *appt2;

    appt1 = (xfical_appt *)a;
    appt2 = (xfical_appt *)b;

    return g_date_time_compare (appt1->starttimecur, appt2->starttimecur);
}

static gint todo_order (gconstpointer a, gconstpointer b)
{
    xfical_appt *appt1, *appt2;

    appt1 = (xfical_appt *)a;
    appt2 = (xfical_appt *)b;

    if (appt1->use_due_time && !appt2->use_due_time)
        return (-1);
    if (!appt1->use_due_time && appt2->use_due_time)
        return (1);

    return g_date_time_compare (appt1->endtimecur, appt2->endtimecur);
}

static void todo_clicked (GtkWidget *widget, GdkEventButton *event,
                          G_GNUC_UNUSED gpointer *user_data)
{
    gchar *uid;
    GtkWidget *appointment_window;

    if (event->type == GDK_2BUTTON_PRESS)
    {
        uid = g_object_get_data (G_OBJECT(widget), "UID");
        appointment_window = orage_appointment_window_new_update (uid);
        gtk_window_present (GTK_WINDOW (appointment_window));
    }
}

static void add_info_row (xfical_appt *appt, GtkBox *parent_box,
                          const gboolean todo)
{
    GtkWidget *ev;
    GtkWidget *label;
    gchar *tip;
    gchar *tmp;
    gchar *tmp_title;
    gchar *tmp_note;
    gchar *tip_title;
    gchar *tip_location;
    gchar *tip_note;
    gchar *s_time;
    gchar *s_timeonly;
    gchar *e_time;
    gchar *c_time;
    gchar *na;
    GDateTime *today;
    GDateTime *gdt_end_time;

    /* Add data into the vbox. */
    ev = gtk_event_box_new ();
    tmp_title = appt->title ? orage_process_text_commands(appt->title)
                            : g_strdup (_("No title defined"));
    s_time = orage_gdatetime_to_i18_time (appt->starttimecur, appt->allDay);
    today = g_date_time_new_now_local ();
    if (todo)
    {
        e_time = appt->use_due_time
               ? orage_gdatetime_to_i18_time (appt->endtimecur, appt->allDay)
               : g_strdup (s_time);
        tmp = g_strdup_printf (" %s  %s", e_time, tmp_title);
        g_free (e_time);
    }
    else
    {
        s_timeonly = g_date_time_format (appt->starttimecur, "%R");
        if (orage_gdatetime_compare_date (today, appt->starttimecur) == 0)
            tmp = g_strdup_printf (" %s* %s", s_timeonly, tmp_title);
        else
        {
            if (g_par.show_event_days > 1)
                tmp = g_strdup_printf (" %s  %s", s_time, tmp_title);
            else
                tmp = g_strdup_printf (" %s  %s", s_timeonly, tmp_title);
        }
        g_free (s_timeonly);
    }

    label = gtk_label_new (tmp);

    g_free (tmp);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);

    g_object_set (label, "xalign", 0.0, "yalign", 0.5,
                         "margin-start", 5,
                         "hexpand", TRUE,
                         "halign", GTK_ALIGN_FILL,
                         NULL);

    gtk_container_add (GTK_CONTAINER (ev), label);
    gtk_box_pack_start (parent_box, ev, FALSE, FALSE, 0);
    g_object_set_data_full (G_OBJECT (ev), "UID", g_strdup (appt->uid), g_free);
    g_signal_connect (ev, "button-press-event", G_CALLBACK (todo_clicked),
                      NULL);

    /* Set color. */
    if (todo)
    {
        if (appt->use_due_time)
            gdt_end_time = g_date_time_ref (appt->endtimecur);
        else
            gdt_end_time = g_date_time_new_local (9999, 12, 31, 23, 59, 59);

        if (g_date_time_compare (gdt_end_time, today) < 0)
        {
            /* gone */
            gtk_widget_set_name (label, ORAGE_TODO_COMPLETED);
        }
        else if ((g_date_time_compare (appt->starttimecur, today) <= 0) &&
                 (g_date_time_compare (gdt_end_time, today) >= 0))
        {
            gtk_widget_set_name (label, ORAGE_TODO_ACTUAL_NOW);
        }

        g_date_time_unref (gdt_end_time);
    }

    /* Set tooltip hint. */
    tip_title = g_markup_printf_escaped (FORMAT_BOLD, tmp_title);
    if (appt->location)
    {
        tmp = g_markup_printf_escaped (FORMAT_BOLD, appt->location);
        tip_location = g_strdup_printf (_(" Location: %s\n"), tmp);
        g_free (tmp);
    }
    else
        tip_location = g_strdup ("");

    if (appt->note)
    {
        tmp_note = orage_process_text_commands (appt->note);
        tmp_note = orage_limit_text (tmp_note, 50, 10);
        tmp = g_markup_escape_text (tmp_note, strlen (tmp_note));
        tip_note = g_strdup_printf (_("\n Note:\n%s"), tmp);
        g_free (tmp);
    }
    else
        tip_note = g_strdup ("");

    if (todo)
    {
        na = _("Never");
        e_time = appt->use_due_time
               ? orage_gdatetime_to_i18_time (appt->endtimecur, appt->allDay)
               : g_strdup (na);
        c_time = (appt->completed && appt->completedtime)
               ? orage_gdatetime_to_i18_time (appt->completedtime, appt->allDay)
               : g_strdup (na);

        tip = g_strdup_printf (
            _("Title: %s\n%s Start:\t%s\n Due:\t%s\n Done:\t%s%s"),
            tip_title, tip_location, s_time, e_time, c_time, tip_note);

        g_free (c_time);
    }
    else
    {
        /* It is event. */
        e_time = orage_gdatetime_to_i18_time (appt->endtimecur, appt->allDay);
        tip = g_strdup_printf (_("Title: %s\n%s Start:\t%s\n End:\t%s%s"),
                               tip_title, tip_location, s_time, e_time,
                               tip_note);
    }

    gtk_widget_set_tooltip_markup (ev, tip);

    g_date_time_unref (today);
    g_free (tip_title);
    g_free (tip_location);
    g_free (tip_note);
    g_free (tmp_title);
    g_free (s_time);
    g_free (e_time);
    g_free (tip);
}

static void info_process (gpointer a, gpointer pbox, gboolean todo)
{
    xfical_appt *appt = (xfical_appt *)a;

    if (appt->priority < g_par.priority_list_limit)
        add_info_row (appt, GTK_BOX (pbox), todo);

    xfical_appt_free (appt);
}

static void info_process_event (gpointer a, gpointer pbox)
{
    info_process (a, pbox, FALSE);
}

static void info_process_todo (gpointer a, gpointer pbox)
{
    info_process (a, pbox, TRUE);
}

static void orage_window_next_build_mainbox_todo_info (OrageWindowNext *self)
{
    GDateTime *gdt;
    xfical_type ical_type;
    gchar file_type[8];
    gint i;
    GList *todo_list = NULL;

    g_return_if_fail (self != NULL);

    if (g_par.show_todos)
    {
        gdt = g_date_time_new_now_local ();
        ical_type = XFICAL_TYPE_TODO;

        /* First search base orage file. */
        g_strlcpy (file_type, "O00.", sizeof (file_type));
        insert_rows (&todo_list, gdt, ical_type, file_type);

        /* Then process all foreign files. */
        for (i = 0; i < g_par.foreign_count; i++)
        {
            g_snprintf (file_type, sizeof (file_type), "F%02d.", i);
            insert_rows (&todo_list, gdt, ical_type, file_type);
        }

        g_date_time_unref (gdt);
    }

    if (todo_list)
    {
        clear_gtk_box (self->todo_rows_box);
        todo_list = g_list_sort (todo_list, todo_order);
        g_list_foreach (todo_list, info_process_todo, self->todo_rows_box);
        g_list_free (todo_list);
        gtk_widget_show_all (self->todo_box);
    }
    else
        gtk_widget_hide (self->todo_box);
}

static void orage_window_next_build_mainbox_event_info (OrageWindowNext *self)
{
    xfical_type ical_type;
    gchar file_type[8];
    gchar *text;
    gint i;
    GList *event_list = NULL;
    GDateTime *gdt;

    g_return_if_fail (self != NULL);

    if (g_par.show_event_days)
    {
        gdt = g_date_time_ref (self->selected_date);
        ical_type = XFICAL_TYPE_EVENT;
        g_strlcpy (file_type, "O00.", sizeof (file_type));
        xfical_get_each_app_within_time (gdt, g_par.show_event_days, ical_type,
                                         file_type, &event_list);
        for (i = 0; i < g_par.foreign_count; i++)
        {
            g_snprintf (file_type, sizeof (file_type), "F%02d.", i);
            xfical_get_each_app_within_time (gdt, g_par.show_event_days,
                                             ical_type, file_type, &event_list);
        }

        g_date_time_unref (gdt);
    }

    if (event_list)
    {
        text = create_event_label_text (self->selected_date);
        gtk_label_set_text (GTK_LABEL (self->event_label), text);
        g_free (text);
        clear_gtk_box (self->event_rows_box);

        event_list = g_list_sort (event_list, event_order);
        g_list_foreach (event_list, info_process_event, self->event_rows_box);
        g_list_free (event_list);
        gtk_widget_show_all (self->event_box);
    }
    else
        gtk_widget_hide (self->event_box);
}

static void orage_window_next_class_init (OrageWindowNextClass *klass)
{
    GObjectClass *object_class;

    gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "OrageWindowNext");

    xfce_gtk_translate_action_entries (action_entries,
                                       G_N_ELEMENTS (action_entries));

    object_class = G_OBJECT_CLASS (klass);
    object_class->constructed = orage_window_next_constructed;
    object_class->finalize = orage_window_next_finalize;
}

static void orage_window_next_init (OrageWindowNext *self)
{
    GtkWidget *switcher;
    GtkWidget *spacer_left;
    GtkWidget *spacer_right;
    GtkBox *switcher_box;
    GtkBox *main_box;
    GtkPaned *paned;
    const size_t n_elements = G_N_ELEMENTS (action_entries);

    self->selected_date = g_date_time_new_now_utc ();
    self->accel_group = gtk_accel_group_new ();

    xfce_gtk_accel_map_add_entries (action_entries, n_elements);
    xfce_gtk_accel_group_connect_action_entries (self->accel_group,
                                                 action_entries, n_elements,
                                                 self);

    gtk_window_add_accel_group (GTK_WINDOW (self), self->accel_group);

    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    main_box = GTK_BOX (self->main_box);
    gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (main_box));

    orage_window_next_add_menubar (self);
    gtk_box_pack_start (main_box, self->menubar, FALSE, FALSE, 0);

    switcher_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));

    /* Previous button. */
    self->back_button = gtk_button_new_from_icon_name ("go-previous",
                                                       GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text (self->back_button, _("Previous"));
    gtk_box_pack_start (switcher_box, self->back_button, FALSE, FALSE, 4);

    /* Left spacer. */
    spacer_left = gtk_label_new (NULL);
    gtk_widget_set_hexpand (spacer_left, TRUE);
    gtk_box_pack_start (switcher_box, spacer_left, TRUE, TRUE, 0);

    /* Switcher. */
    switcher = gtk_stack_switcher_new ();
    gtk_box_pack_start (switcher_box, switcher, FALSE, FALSE, 0);

    /* Right spacer. */
    spacer_right = gtk_label_new (NULL);
    gtk_widget_set_hexpand (spacer_right, TRUE);
    gtk_box_pack_start (switcher_box, spacer_right, TRUE, TRUE, 0);

    /* Next button. */
    self->next_button = gtk_button_new_from_icon_name ("go-next",
                                                       GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text (self->next_button, _("Next"));
    gtk_box_pack_start (switcher_box, self->next_button, FALSE, FALSE, 4);

    gtk_box_pack_start (main_box, GTK_WIDGET (switcher_box), FALSE, FALSE, 0);

    self->stack_view = GTK_STACK (gtk_stack_new ());
    gtk_stack_set_transition_type (self->stack_view,
                                   GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher),
                                  self->stack_view);

    self->month_view = ORAGE_MONTH_VIEW (orage_month_view_new (BY_LOCALE));
#if 0
    gtk_stack_add_titled (self->stack_view,
                          gtk_label_new ("TODO: Day view"), "day", _("Day"));
    gtk_stack_add_titled (self->stack_view,
                          gtk_label_new ("TODO: Week view"), "week", _("Week"));
#endif
    gtk_stack_add_titled (self->stack_view, GTK_WIDGET (self->month_view),
                          MONTH_PAGE, _("Month"));
#if 0
    gtk_stack_add_titled (self->stack_view,
                          gtk_label_new ("TODO: Year view"), "year", _("Year"));
#endif

    self->info_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    self->paned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
    paned = GTK_PANED (self->paned);
    gtk_paned_set_wide_handle (paned, TRUE);
    gtk_paned_pack1 (paned, GTK_WIDGET (self->stack_view), TRUE, FALSE);
    gtk_paned_pack2 (paned, self->info_box, TRUE, TRUE);

    gtk_box_pack_start (main_box, GTK_WIDGET (paned), TRUE, TRUE, 0);

    orage_window_next_add_todo_box (self);
    orage_window_next_add_event_box (self);

    gtk_widget_show_all (self->main_box);

    gtk_stack_set_visible_child_name (self->stack_view, MONTH_PAGE);

    gtk_widget_add_events (GTK_WIDGET (self), GDK_KEY_PRESS_MASK);
    gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
    gtk_widget_grab_focus (GTK_WIDGET (self));

    g_signal_connect (self->back_button, "clicked",
                      G_CALLBACK (on_back_clicked), self);
    g_signal_connect (self->next_button, "clicked",
                      G_CALLBACK (on_next_clicked), self);

    g_signal_connect (self->month_view, "date-selected",
                      G_CALLBACK (on_date_selected), self);
    g_signal_connect (self->month_view, "date-selected-right-clicked",
                      G_CALLBACK (on_date_selected_right_clicked), self);
    g_signal_connect (self->month_view, "date-selected-double-clicked",
                      G_CALLBACK (on_date_selected_double_clicked), NULL);
    g_signal_connect (self->month_view, "reload-requested",
                      G_CALLBACK (on_month_reload_requested), self);

    g_signal_connect (self, "key-press-event", G_CALLBACK (on_key_press), NULL);
}

static void orage_window_next_interface_init (OrageWindowInterface *iface)
{
    iface->update_appointments = orage_window_next_update_appointments;
    iface->build_info = orage_window_next_build_info;
    iface->build_events = orage_window_next_build_events;
    iface->build_todo = orage_window_next_build_todo;
    iface->initial_load = orage_window_next_initial_load;
    iface->select_today = orage_window_next_select_today;
    iface->select_date = orage_window_next_select_date;
    iface->get_selected_date = orage_window_next_get_selected_date;
    iface->show_menubar = orage_window_next_show_menubar;
    iface->hide_todo = orage_window_next_hide_todo;
    iface->hide_event = orage_window_next_hide_event;
    iface->raise = orage_window_next_raise;
    iface->set_calendar_options = orage_window_next_set_calendar_options;
    iface->save_window_state = orage_window_next_save_window_state;
}

static void orage_window_next_constructed (GObject *object)
{
    OrageWindowNext *self = (OrageWindowNext *)object;

    orage_window_next_restore_state (self);

    G_OBJECT_CLASS (orage_window_next_parent_class)->constructed (object);
}

static void orage_window_next_finalize (GObject *object)
{
    OrageWindowNext *self = (OrageWindowNext *)object;

    g_date_time_unref (self->selected_date);

    G_OBJECT_CLASS (orage_window_next_parent_class)->finalize (object);
}

GtkWidget *orage_window_next_new (OrageApplication *application)
{
    return g_object_new (ORAGE_WINDOW_NEXT_TYPE,
                         "application", GTK_APPLICATION (application),
                         NULL);
}

void orage_window_next_build_events (OrageWindow *window)
{
    if (xfical_file_open (TRUE) == FALSE)
        return;

    orage_window_next_build_mainbox_event_info (ORAGE_WINDOW_NEXT (window));

    xfical_file_close (TRUE);
}

void orage_window_next_build_info (OrageWindow *window)
{
    OrageWindowNext *self = ORAGE_WINDOW_NEXT (window);

    orage_window_next_build_mainbox_todo_info (self);
    orage_window_next_build_mainbox_event_info (self);
}

void orage_window_next_build_todo (OrageWindow *window)
{
    if (xfical_file_open (TRUE) == FALSE)
        return;

    orage_window_next_build_mainbox_todo_info (ORAGE_WINDOW_NEXT (window));

    xfical_file_close (TRUE);
}

void orage_window_next_hide_event (OrageWindow *window)
{
    g_return_if_fail (window != NULL);

    gtk_widget_hide (ORAGE_WINDOW_NEXT (window)->event_box);
}

void orage_window_next_hide_todo (OrageWindow *window)
{
    gtk_widget_hide (ORAGE_WINDOW_NEXT (window)->todo_box);
}

void orage_window_next_update_appointments (OrageWindow *window)
{
    GDateTime *gdt_start;
    GDateTime *gdt_end;
    OrageWindowNext *self;
    const gchar *visible_name;
    enum {E_MONTH_PAGE, E_UNDEFINED_PAGE} displayed_page;

    g_return_if_fail (window != NULL);

    self = ORAGE_WINDOW_NEXT (window);
    visible_name = gtk_stack_get_visible_child_name (self->stack_view);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
        displayed_page = E_MONTH_PAGE;
    else
    {
        displayed_page = E_UNDEFINED_PAGE;

        /* Requested page handling is not yet implemented. */
        g_return_if_reached ();
    }

    if (xfical_file_open (TRUE) == FALSE)
        return;

    gdt_start = orage_window_next_get_first_date (self);
    gdt_end = orage_window_next_get_last_date (self);

    switch (displayed_page)
    {
        case E_MONTH_PAGE:
            xfical_list_events_in_range (gdt_start, gdt_end,
                                         cb_update_month_events,
                                         self->month_view);
            break;

        default:
            g_assert_not_reached ();
    }

    g_date_time_unref (gdt_start);
    g_date_time_unref (gdt_end);

    xfical_file_close (TRUE);
}

void orage_window_next_initial_load (OrageWindow *window)
{
    (void)g_idle_add_once ((GSourceOnceFunc)orage_window_next_update_appointments,
                           window);
}

void orage_window_next_select_today (OrageWindow *window)
{
    GDateTime *gdt;

    gdt = g_date_time_new_now_local ();
    orage_window_next_select_date (window, gdt);
    g_date_time_unref (gdt);
}

void orage_window_next_select_date (OrageWindow *window, GDateTime *gdt)
{
    OrageWindowNext *nw;
    const gchar *visible_name;

    g_return_if_fail (window != NULL);

    nw = ORAGE_WINDOW_NEXT (window);
    visible_name = gtk_stack_get_visible_child_name (nw->stack_view);

    g_date_time_unref (nw->selected_date);
    nw->selected_date = g_date_time_ref (gdt);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
        orage_month_view_mark_date (nw->month_view, nw->selected_date);
    else
    {
        /* Requested page handinling is not yet implemented. */
        g_return_if_reached ();
    }
}

GDateTime *orage_window_next_get_selected_date (OrageWindow *window)
{
    OrageWindowNext *nxtwindow;

    g_return_val_if_fail (window != NULL, NULL);
    nxtwindow = ORAGE_WINDOW_NEXT (window);

    return g_date_time_ref (nxtwindow->selected_date);
}

void orage_window_next_show_menubar (OrageWindow *window, const gboolean show)
{
    OrageWindowNext *nxtwindow;

    g_return_if_fail (window != NULL);
    nxtwindow = ORAGE_WINDOW_NEXT (window);

    if (show)
        gtk_widget_show (nxtwindow->menubar);
    else
        gtk_widget_hide (nxtwindow->menubar);
}

void orage_window_next_raise (OrageWindow *window)
{
    GtkWindow *gtk_window = GTK_WINDOW (window);

    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move (gtk_window, g_par.pos_x, g_par.pos_y);

    if (g_par.select_always_today)
        orage_window_next_select_today (window);

    if (g_par.set_stick)
        gtk_window_stick (gtk_window);

    gtk_window_set_keep_above (gtk_window, g_par.set_ontop);
    gtk_window_present (gtk_window);
}

void orage_window_next_set_calendar_options (G_GNUC_UNUSED OrageWindow *window,
                                             G_GNUC_UNUSED guint options)
{
    /* New calendar window does not have any options yet. */
}

void orage_window_next_save_window_state (OrageWindow *window)
{
    GtkWindow *gtk_window = GTK_WINDOW (window);

    gtk_window_get_size (gtk_window, &g_par.size_x, &g_par.size_y);
    gtk_window_get_position (gtk_window, &g_par.pos_x, &g_par.pos_y);
    g_par.paned_pos = gtk_paned_get_position (
            GTK_PANED (ORAGE_WINDOW_NEXT(window)->paned));
}
