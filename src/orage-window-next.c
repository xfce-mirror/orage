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
#include "interface.h"
#include "orage-about.h"
#include "orage-appointment-window.h"
#include "orage-i18n.h"
#include "orage-month-view.h"
#include "orage-week-window.h"
#include "parameters.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <stdio.h>

#define MONTH_PAGE "month"

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
    GtkStack *stack_view;
    GtkWidget *main_view;
    OrageMonthView *month_view;

    GtkWidget *back_button;
    GtkWidget *next_button;
};

static void orage_window_next_constructed (GObject *object);
static void orage_window_next_finalize (GObject *object);

static void orage_window_next_interface_init (OrageWindowInterface *interface);
static GDateTime *orage_window_next_get_selected_date (OrageWindowNext *window);
static void orage_window_next_restore_geometry (OrageWindowNext *window);

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
    {ORAGE_WINDOW_ACTION_FILE_MENU, "<Actions>/OrageWindow/file-menu", "", XFCE_GTK_MENU_ITEM, N_ ("_File"), NULL, NULL, NULL},
    {ORAGE_WINDOW_ACTION_NEW_APPOINTMENT, "<Actions>/OrageWindow/new-appointment", "<control>n", XFCE_GTK_IMAGE_MENU_ITEM, N_("New"), N_ ("Create a new appointment"), "document-new", G_CALLBACK (on_new_activated)},
#ifdef ENABLE_SYNC
    {ORAGE_WINDOW_ACTION_REFRESH, "<Actions>/OrageWindow/refresh", "", XFCE_GTK_IMAGE_MENU_ITEM, N_("Refresh"), N_ ("Reload external files"), "view-refresh", G_CALLBACK (on_refresh_activated)},
#endif
    {ORAGE_WINDOW_ACTION_EXCHANGE, "<Actions>/OrageWindow/exchange", "", XFCE_GTK_MENU_ITEM, N_("_Exchange data"), N_ ("Import/export data"), "", G_CALLBACK (on_exchange_activated)},
    {ORAGE_WINDOW_ACTION_CLOSE, "<Actions>/OrageWindow/close", "<control>w", XFCE_GTK_IMAGE_MENU_ITEM, N_("Close"), N_ ("Close window"), "window-close", G_CALLBACK (on_close_activated)},
    {ORAGE_WINDOW_ACTION_QUIT, "<Actions>/OrageWindow/quit", "<control>q", XFCE_GTK_IMAGE_MENU_ITEM, N_("_Quit"), N_ ("Quit program"), "application-exit", G_CALLBACK (on_quit_activated)},

    {ORAGE_WINDOW_ACTION_EDIT_MENU, "<Actions>/OrageWindow/edit-menu", "", XFCE_GTK_MENU_ITEM, N_ ("_Edit"), NULL, NULL, NULL},
    {ORAGE_WINDOW_ACTION_PREFERENCES, "<Actions>/OrageWindow/preferences", "", XFCE_GTK_IMAGE_MENU_ITEM, N_("_Preferences"), N_ ("Orage preferences"), "preferences-system", G_CALLBACK (on_preferences_activated)},

    {ORAGE_WINDOW_ACTION_VIEW_MENU, "<Actions>/OrageWindow/view-menu", "", XFCE_GTK_MENU_ITEM, N_ ("_View"), NULL, NULL, NULL},
    {ORAGE_WINDOW_ACTION_VIEW_DATE, "<Actions>/OrageWindow/view-date", "", XFCE_GTK_MENU_ITEM, N_("View selected _date"), N_ ("View selected date"), "", G_CALLBACK (on_view_selected_date_activated)},
    {ORAGE_WINDOW_ACTION_VIEW_WEEK, "<Actions>/OrageWindow/view-week", "", XFCE_GTK_MENU_ITEM, N_("View selected _week"), N_ ("View selected _week"), "", G_CALLBACK (on_view_selected_week_activated)},
    {ORAGE_WINDOW_ACTION_SELECT_TODAY, "<Actions>/OrageWindow/select-today", "", XFCE_GTK_MENU_ITEM, N_("Select _Today"), N_ ("Go to today"), "", G_CALLBACK (on_select_today_activated)},

    {ORAGE_WINDOW_ACTION_HELP_MENU, "<Actions>/OrageWindow/help-menu", "", XFCE_GTK_MENU_ITEM, N_ ("_Help"), NULL, NULL, NULL},
    {ORAGE_WINDOW_ACTION_HELP, "<Actions>/OrageWindow/help", "F1", XFCE_GTK_IMAGE_MENU_ITEM, N_("_Help"), N_ ("Display Orage user manual"), "help-browser", G_CALLBACK (on_help_activated)},
    {ORAGE_WINDOW_ACTION_ABOUT, "<Actions>/OrageWindow/about", "", XFCE_GTK_IMAGE_MENU_ITEM, N_("_About"), N_ ("Display information about Orage"), "help-about", G_CALLBACK (on_about_activated)},
};

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id (action_entries, G_N_ELEMENTS (action_entries), id)

G_DEFINE_TYPE_WITH_CODE (OrageWindowNext, orage_window_next, GTK_TYPE_APPLICATION_WINDOW,
                         G_IMPLEMENT_INTERFACE (ORAGE_WINDOW_TYPE, orage_window_next_interface_init))

static void on_new_activated (OrageWindowNext *window)
{
    GDateTime *gdt;
    GtkWidget *appointment_window;

    g_return_if_fail (ORAGE_IS_WINDOW (window));

    /* Orage has always a day selected here, so it is safe to read it. */
    gdt = orage_window_next_get_selected_date (window);
    appointment_window = orage_appointment_window_new (gdt);
    gtk_window_present (GTK_WINDOW (appointment_window));
    g_date_time_unref (gdt);
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

static void on_view_selected_date_activated (G_GNUC_UNUSED OrageWindowNext *window)
{
    (void)create_el_win (NULL);
}

static void on_view_selected_week_activated (OrageWindowNext *window)
{
    GDateTime *gdt;

    g_return_if_fail (ORAGE_IS_WINDOW (window));

    gdt = orage_cal_to_gdatetime (
        orage_window_next_get_calendar (ORAGE_WINDOW (window)), 1, 1);
    orage_week_window_build (gdt);
    g_date_time_unref (gdt);
}

static void on_select_today_activated (OrageWindowNext *window)
{
    g_return_if_fail (ORAGE_IS_WINDOW (window));

    orage_select_today (orage_window_next_get_calendar (ORAGE_WINDOW (window)));
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

        orage_month_view_select_month (nw->month_view, nw->selected_date);
        orage_window_next_mark_appointments (ORAGE_WINDOW (user_data));
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

        orage_month_view_select_month (nw->month_view, nw->selected_date);
        orage_window_next_mark_appointments (ORAGE_WINDOW (user_data));
    }
    else
    {
        /* Requested page handling is not yet implemented. */
        g_return_if_reached ();
    }
}

static void on_list_event_callback (GDateTime *gdt_start, GDateTime *gdt_end,
                                    void *param)
{
    OrageWindowNext *window = ORAGE_WINDOW_NEXT (param);
    const gchar *visible_name =
        gtk_stack_get_visible_child_name (window->stack_view);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
        orage_month_view_mark_dates (window->month_view, gdt_start, gdt_end);
    else
    {
        /* Requested page handling is not yet implemented. */
        g_return_if_reached ();
    }
}

static gboolean on_key_press (GtkWidget *widget, GdkEventKey *event,
                              gpointer user_data)
{
    OrageWindowNext *self = ORAGE_WINDOW_NEXT (widget);

    switch (event->keyval)
    {
        case GDK_KEY_Left:
            gtk_button_clicked (GTK_BUTTON (self->back_button));
            return TRUE;

        case GDK_KEY_Right:
            gtk_button_clicked (GTK_BUTTON (self->next_button));
            return TRUE;

        default:
            return FALSE;
    }
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

static void update_file_menu (OrageWindowNext *window, GtkWidget *menu)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (window));

    menu_clean (GTK_MENU (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_NEW_APPOINTMENT), G_OBJECT (window),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
#ifdef ENABLE_SYNC
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_REFRESH), G_OBJECT (window),
        GTK_MENU_SHELL (menu));
#endif
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_EXCHANGE), G_OBJECT (window),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_CLOSE), G_OBJECT (window),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_QUIT), G_OBJECT (window),
        GTK_MENU_SHELL (menu));

    gtk_widget_show_all (GTK_WIDGET (menu));
}

static void update_edit_menu (OrageWindowNext *window, GtkWidget *menu)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (window));

    menu_clean (GTK_MENU (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_PREFERENCES), G_OBJECT (window),
        GTK_MENU_SHELL (menu));

    gtk_widget_show_all (GTK_WIDGET (menu));
}

static void update_view_menu (OrageWindowNext *window, GtkWidget *menu)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (window));

    menu_clean (GTK_MENU (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_VIEW_DATE), G_OBJECT (window),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_VIEW_WEEK), G_OBJECT (window),
        GTK_MENU_SHELL (menu));

    xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_SELECT_TODAY), G_OBJECT (window),
        GTK_MENU_SHELL (menu));

    gtk_widget_show_all (GTK_WIDGET (menu));
}

static void update_help_menu (OrageWindowNext *window, GtkWidget *menu)
{
    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (window));

    menu_clean (GTK_MENU (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_HELP), G_OBJECT (window),
        GTK_MENU_SHELL (menu));
    xfce_gtk_menu_item_new_from_action_entry (
        get_action_entry (ORAGE_WINDOW_ACTION_ABOUT), G_OBJECT (window),
        GTK_MENU_SHELL (menu));

    gtk_widget_show_all (GTK_WIDGET (menu));
}

static void orage_window_next_clear_events (OrageWindowNext *window)
{
    g_debug ("TODO: %s", G_STRFUNC);
    (void)window;
}

static GDateTime *orage_window_next_get_first_date (OrageWindowNext *window)
{
    const gchar *visible_name =
        gtk_stack_get_visible_child_name (window->stack_view);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
        return orage_month_view_get_first_date (window->month_view);
    else
    {
        /* Requested page handinling is not yet implemented. */
        g_return_val_if_reached (g_date_time_new_now_local ());
    }
}

static GDateTime *orage_window_next_get_last_date (OrageWindowNext *window)
{
    const gchar *visible_name =
        gtk_stack_get_visible_child_name (window->stack_view);

    if (g_strcmp0 (MONTH_PAGE, visible_name) == 0)
        return orage_month_view_get_last_date (window->month_view);
    else
    {
        /* Requested page handinling is not yet implemented. */
        g_return_val_if_reached (g_date_time_new_now_local ());
    }
}

static void orage_window_next_restore_geometry (OrageWindowNext *window)
{
    GtkWindow *gwin = GTK_WINDOW (window);

    gtk_window_set_position (gwin, GTK_WIN_POS_NONE);

    if (g_par.size_x || g_par.size_y)
        gtk_window_set_default_size (gwin, g_par.size_x, g_par.size_y);

    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move (gwin, g_par.pos_x, g_par.pos_y);
}

static void orage_window_next_add_menu (OrageWindowNext *window,
                                        OrageWindowAction action,
                                        GCallback cb_update_menu)
{
    GtkWidget *item;
    GtkWidget *submenu;
    GtkWidget *menu = window->menubar;

    g_return_if_fail (ORAGE_IS_WINDOW_NEXT (window));

    item = xfce_gtk_menu_item_new_from_action_entry (get_action_entry (action),
                                                     G_OBJECT (window),
                                                     GTK_MENU_SHELL (menu));

    submenu = g_object_new (GTK_TYPE_MENU, NULL);
    gtk_menu_set_accel_group (GTK_MENU (submenu), window->accel_group);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (submenu));
    g_signal_connect_swapped (G_OBJECT (submenu), "show",
                              G_CALLBACK (cb_update_menu), window);
}

static void orage_window_next_add_menubar (OrageWindowNext *self)
{
    self->menubar = gtk_menu_bar_new ();
    orage_window_next_add_menu (self, ORAGE_WINDOW_ACTION_FILE_MENU,
                                G_CALLBACK (update_file_menu));
    orage_window_next_add_menu (self, ORAGE_WINDOW_ACTION_EDIT_MENU,
                                G_CALLBACK (update_edit_menu));
    orage_window_next_add_menu (self, ORAGE_WINDOW_ACTION_VIEW_MENU,
                                G_CALLBACK (update_view_menu));
    orage_window_next_add_menu (self, ORAGE_WINDOW_ACTION_HELP_MENU,
                                G_CALLBACK (update_help_menu));
    gtk_widget_show_all (self->menubar);
}

static void orage_window_next_class_init (OrageWindowNextClass *klass)
{
    GObjectClass *object_class;

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
    const size_t n_elements = G_N_ELEMENTS (action_entries);

    self->selected_date = g_date_time_new_now_utc ();
    self->accel_group = gtk_accel_group_new ();

    xfce_gtk_accel_map_add_entries (action_entries, n_elements);
    xfce_gtk_accel_group_connect_action_entries (self->accel_group,
                                                 action_entries, n_elements,
                                                 self);

    gtk_window_add_accel_group (GTK_WINDOW (self), self->accel_group);

    main_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 2));
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
    gtk_box_pack_start (main_box, GTK_WIDGET (self->stack_view), TRUE, TRUE, 0);
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

    gtk_widget_show_all (GTK_WIDGET (main_box));

    gtk_stack_set_visible_child_name (self->stack_view, MONTH_PAGE);

    gtk_widget_add_events (GTK_WIDGET (self), GDK_KEY_PRESS_MASK);
    gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
    gtk_widget_grab_focus (GTK_WIDGET (self));

    g_signal_connect (self->back_button, "clicked", G_CALLBACK (on_back_clicked),
                      self);
    g_signal_connect (self->next_button, "clicked", G_CALLBACK (on_next_clicked),
                      self);

    g_signal_connect (self, "key-press-event", G_CALLBACK (on_key_press), NULL);
}

static void orage_window_next_interface_init (OrageWindowInterface *iface)
{
    iface->mark_appointments = orage_window_next_mark_appointments;
    iface->build_info = orage_window_next_build_info;
    iface->build_events = orage_window_next_build_events;
    iface->build_todo = orage_window_next_build_todo;
    iface->initial_load = orage_window_next_initial_load;
    iface->show_menubar = orage_window_next_show_menubar;
    iface->hide_todo = orage_window_next_hide_todo;
    iface->hide_event = orage_window_next_hide_event;
    iface->get_calendar = orage_window_next_get_calendar;
    iface->raise = orage_window_next_raise;
}

static void orage_window_next_constructed (GObject *object)
{
    OrageWindowNext *self = (OrageWindowNext *)object;

    orage_window_next_restore_geometry (self);

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

GDateTime *orage_window_next_get_selected_date (OrageWindowNext *window)
{
    OrageWindowNext *nxtwindow = ORAGE_WINDOW_NEXT (window);

    return g_date_time_ref (nxtwindow->selected_date);
}

void orage_window_next_build_events (OrageWindow *window)
{
#if 0
    if (!xfical_file_open(TRUE))
        return;

    build_mainbox_event_info (window);
    xfical_file_close(TRUE);
#else
    g_debug ("TODO: %s", G_STRFUNC);
    (void)window;
#endif
}

void orage_window_next_build_info (OrageWindow *window)
{
#if 0
    build_mainbox_todo_info (window);
    build_mainbox_event_info (window);
#else
    g_debug ("TODO: %s", G_STRFUNC);
    (void)window;
#endif
}

void orage_window_next_build_todo (OrageWindow *window)
{
#if 0
    if (!xfical_file_open (TRUE))
        return;
    build_mainbox_todo_info (window);
    xfical_file_close (TRUE);
#else
    g_debug ("TODO: %s", G_STRFUNC);
    (void)window;
#endif
}

GtkCalendar *orage_window_next_get_calendar (OrageWindow *window)
{
#if 0
    return GTK_CALENDAR (ORAGE_WINDOW_CLASSIC (window)->mCalendar);
#else
    g_debug ("TODO: %s: this function create memory leak", G_STRFUNC);
    (void)window;
    return GTK_CALENDAR (gtk_calendar_new ());
#endif
}

void orage_window_next_hide_event (OrageWindow *window)
{
#if 0
    gtk_widget_hide (ORAGE_WINDOW_CLASSIC (window)->mEvent_vbox);
#else
    g_debug ("TODO: %s", G_STRFUNC);
    (void)window;
#endif
}

void orage_window_next_hide_todo (OrageWindow *window)
{
#if 0
    gtk_widget_hide (ORAGE_WINDOW_CLASSIC (window)->mTodo_vbox);
#else
    g_debug ("TODO: %s", G_STRFUNC);
    (void)window;
#endif
}

void orage_window_next_mark_appointments (OrageWindow *window)
{
    GDateTime *gdt_start;
    GDateTime *gdt_end;
    OrageWindowNext *nxtwindow;

    g_return_if_fail (window != NULL);

    if (xfical_file_open (TRUE) == FALSE)
        return;

    nxtwindow = ORAGE_WINDOW_NEXT (window);
    gdt_start = orage_window_next_get_first_date (nxtwindow);
    gdt_end = orage_window_next_get_last_date (nxtwindow);

#if 1
    gchar *begin_text = g_date_time_format_iso8601 (gdt_start);
    gchar *end_text = g_date_time_format_iso8601 (gdt_end);

    g_debug ("TODO: %s: start='%s' / end='%s'", G_STRFUNC, begin_text, end_text);

    g_free (begin_text);
    g_free (end_text);
#endif

    orage_window_next_clear_events (nxtwindow);
    xfical_list_events_in_range (gdt_start, gdt_end,
                                 on_list_event_callback, nxtwindow);

    g_date_time_unref (gdt_start);
    g_date_time_unref (gdt_end);

    xfical_file_close (TRUE);
}

void orage_window_next_initial_load (OrageWindow *window)
{
    (void)g_idle_add_once ((GSourceOnceFunc)orage_window_next_mark_appointments,
                           window);
}

void orage_window_next_raise (OrageWindow *window)
{
    GtkWindow *gtk_window = GTK_WINDOW (window);

    if (g_par.pos_x || g_par.pos_y)
        gtk_window_move (gtk_window, g_par.pos_x, g_par.pos_y);

    if (g_par.select_always_today)
        orage_select_today (orage_window_next_get_calendar (window));

    if (g_par.set_stick)
        gtk_window_stick (gtk_window);

    gtk_window_set_keep_above (gtk_window, g_par.set_ontop);
    gtk_window_present (gtk_window);
}

void orage_window_next_show_menubar (OrageWindow *window, const gboolean show)
{
    OrageWindowNext *nxtwindow = ORAGE_WINDOW_NEXT (window);

    if (show)
        gtk_widget_show (nxtwindow->menubar);
    else
        gtk_widget_hide (nxtwindow->menubar);
}
