/*
 * Copyright (c) 2021-2025 Erkki Moorits
 * Copyright (c) 2005-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2003-2005 Mickael Graf (korbinus at xfce.org)
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "functions.h"
#include "ical-code.h"
#include "interface.h"
#include "orage-i18n.h"
#include "orage-window.h"
#include "parameters.h"

enum {
    DRAG_TARGET_URILIST = 0
   ,DRAG_TARGET_STRING
};
static const GtkTargetEntry file_drag_targets[] =
{
    { "text/uri-list", 0, DRAG_TARGET_URILIST }
};
static int file_drag_target_count = 1;
static const GtkTargetEntry uid_drag_targets[] =
{
    { "STRING", 0, DRAG_TARGET_STRING }
};
static int uid_drag_target_count = 1;

/* This prevents access from cmd line if the window is open.
 * We must make sure foreign files are not deleted from command
 * line (using dbus) while they are being updated using the window 
 */
static gboolean interface_lock = FALSE;

static void refresh_foreign_files(intf_win *intf_w, const gboolean first);

gboolean orage_external_update_check (G_GNUC_UNUSED gpointer user_data)
{
    struct stat s;
    OrageApplication *app;
    gint i;
    gboolean external_changes_present = FALSE;

    /* check main Orage file */
    if (g_stat(g_par.orage_file, &s) < 0) {
        g_warning ("stat of %s failed: %s", g_par.orage_file,
                   g_strerror (errno));
    }
    else {
        if (s.st_mtime > g_par.latest_file_change) {
            g_par.latest_file_change = s.st_mtime;
            g_message ("Found external update on file %s", g_par.orage_file);
            external_changes_present = TRUE;
        }
    }

    /* check also foreign files */
    for (i = 0; i < g_par.foreign_count; i++) {
        if (g_stat(g_par.foreign_data[i].file, &s) < 0) {
            g_warning ("stat of %s failed: %s", g_par.foreign_data[i].file,
                       g_strerror (errno));
        }
        else {
            if (s.st_mtime > g_par.foreign_data[i].latest_file_change) {
                g_par.foreign_data[i].latest_file_change = s.st_mtime;
                g_message ("Found external update on file %s",
                           g_par.foreign_data[i].file);
                external_changes_present = TRUE;
            }
        }
    }

    if (external_changes_present) {
        g_message ("Refreshing alarms and calendar due to external file update");
        xfical_file_close_force();
        xfical_alarm_build_list(FALSE);
        app = ORAGE_APPLICATION (g_application_get_default ());
        orage_window_classic_mark_appointments (ORAGE_WINDOW_CLASSIC (orage_application_get_window (app)));
    }

    return(TRUE); /* keep running */
}

static void orage_file_entry_changed (G_GNUC_UNUSED GtkWidget *dialog,
                                      gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    const gchar *s;

    s = gtk_entry_get_text(GTK_ENTRY(intf_w->orage_file_entry));
    if (strcmp(g_par.orage_file, s) == 0) {
        gtk_widget_set_sensitive(intf_w->orage_file_save_button, FALSE);
    }
    else {
        gtk_widget_set_sensitive(intf_w->orage_file_save_button, TRUE);
    }
}

static void orage_file_save_button_clicked (G_GNUC_UNUSED GtkButton *button
        , gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    gchar *s;
    gboolean ok = TRUE;

    s = g_strdup(gtk_entry_get_text(GTK_ENTRY(intf_w->orage_file_entry)));
    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->orage_file_rename_rb))) {
        if (!g_file_test(s, G_FILE_TEST_EXISTS)) {
            g_warning("New file %s does not exist. Rename not done", s);
            ok = FALSE;
        }
        if (!xfical_file_check(s)) {
            g_warning("New file %s is not valid ical calendar file. Rename not done", s);
            ok = FALSE;
        }
    }
    else if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->orage_file_copy_rb))) {
        ok = orage_copy_file(g_par.orage_file, s);
    }
    else if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->orage_file_move_rb))) {
        if (g_rename(g_par.orage_file, s)) { /* failed */
            g_warning("rename failed. trying manual copy");
            ok = orage_copy_file(g_par.orage_file, s);
            if (ok) { /* this is move. so let's remove the orig */
               if (g_remove(g_par.orage_file))
                   g_warning("file remove failed %s", g_par.orage_file);
            }
        }
    }
    else {
        g_warning("illegal file save toggle button status");
        ok = FALSE;
    }

    /* finally orage internal file name change */
    if (ok) {
        if (g_par.orage_file)
            g_free(g_par.orage_file);
        g_par.orage_file = s;
        gtk_widget_set_sensitive(intf_w->orage_file_save_button, FALSE);
        write_parameters(); /* store file name */
        xfical_file_close_force(); /* close it so that we open new file */
    }
    else {
        g_free(s);
    }
}

#ifdef HAVE_ARCHIVE
static void archive_file_entry_changed (G_GNUC_UNUSED GtkWidget *dialog,
                                        gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    const gchar *s;

    s = gtk_entry_get_text(GTK_ENTRY(intf_w->archive_file_entry));
    if (strcmp(g_par.archive_file, s) == 0) { /* same file */
        gtk_widget_set_sensitive(intf_w->archive_file_save_button, FALSE);
    }
    else {
        gtk_widget_set_sensitive(intf_w->archive_file_save_button, TRUE);
    }
}

static void archive_file_save_button_clicked (G_GNUC_UNUSED GtkButton *button
        , gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    gchar *s;
    gboolean ok = TRUE;

    s = g_strdup(gtk_entry_get_text(GTK_ENTRY(intf_w->archive_file_entry)));
    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->archive_file_rename_rb))) {
        if (!g_file_test(s, G_FILE_TEST_EXISTS)) {
            g_warning("New file %s does not exist. Rename not done", s);
            ok = FALSE;
        }
        if (!xfical_file_check(s)) {
            g_warning("New file %s is not valid ical calendar file. Rename not done", s);
            ok = FALSE;
        }
    }
    else if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->archive_file_copy_rb))) {
        ok = orage_copy_file(g_par.archive_file, s);
    }
    else if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->archive_file_move_rb))) {
        if (g_rename(g_par.archive_file, s)) { /* failed */
            g_warning("rename failed. trying manual copy");
            ok = orage_copy_file(g_par.archive_file, s);
            if (ok) { /* this is move. so let's remove the orig */
               if (g_remove(g_par.archive_file))
                   g_warning("file remove failed %s", g_par.archive_file);
            }
        }
    }
    else {
        g_warning("illegal file save toggle button status");
        ok = FALSE;
    }

    /* finally archive internal file name change */
    if (ok) {
        if (g_par.archive_file)
            g_free(g_par.archive_file);
        g_par.archive_file = s;
        gtk_widget_set_sensitive(intf_w->archive_file_save_button, FALSE);
        write_parameters(); /* store file name */
    }
    else {
        g_free(s);
    }
}
#endif

static GtkWidget *orage_file_chooser(GtkWidget *parent_window
        , gboolean save, gchar *cur_file, gchar *cur_folder, gchar *def_name)
{
    GtkFileChooser *f_chooser;
    GtkFileFilter *filter;

    /* Create file chooser */
    f_chooser = GTK_FILE_CHOOSER(gtk_file_chooser_dialog_new(
            _("Select a file..."), GTK_WINDOW(parent_window)
            , save ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN
            , "_Cancel", GTK_RESPONSE_CANCEL
            , "_OK", GTK_RESPONSE_ACCEPT
            , NULL));
    /* Add filters */
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Calendar files"));
    gtk_file_filter_add_pattern(filter, "*.ics");
    gtk_file_filter_add_pattern(filter, "*.vcs");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(f_chooser), filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("All Files"));
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(f_chooser, filter);

    if (ORAGE_STR_EXISTS(cur_folder)) 
        gtk_file_chooser_add_shortcut_folder(f_chooser, cur_folder, NULL);

    gtk_file_chooser_set_show_hidden(f_chooser, TRUE);

    if (ORAGE_STR_EXISTS(cur_file)) {
        if (!gtk_file_chooser_set_filename(f_chooser, cur_file)) {
            if (ORAGE_STR_EXISTS(cur_folder)) 
                gtk_file_chooser_set_current_folder(f_chooser, cur_folder);
            if (ORAGE_STR_EXISTS(def_name)) 
                gtk_file_chooser_set_current_name(f_chooser, def_name);
        }
    }
    else {
        if (ORAGE_STR_EXISTS(cur_folder)) 
            gtk_file_chooser_set_current_folder(f_chooser, cur_folder);
        if (ORAGE_STR_EXISTS(def_name)) 
            gtk_file_chooser_set_current_name(f_chooser, def_name);
    }
    return(GTK_WIDGET(f_chooser));
}

static void orage_file_open_button_clicked (G_GNUC_UNUSED GtkButton *button
        , gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *rcfile, *rcdir;
    gchar *s;

    rcdir = g_path_get_dirname (g_par.orage_file);
    rcfile = g_path_get_basename (g_par.orage_file);
    f_chooser = orage_file_chooser(intf_w->main_window, TRUE
            , g_par.orage_file, rcdir, rcfile);
    g_free(rcdir);
    g_free(rcfile);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        s = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (s) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->orage_file_entry), s);
            gtk_widget_grab_focus(intf_w->orage_file_entry);
            gtk_editable_set_position(
                    GTK_EDITABLE(intf_w->orage_file_entry), -1);
            g_free(s);
        }
    }
    gtk_widget_destroy(f_chooser);
}

#ifdef HAVE_ARCHIVE
static void archive_file_open_button_clicked (G_GNUC_UNUSED GtkButton *button
        , gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *rcfile, *rcdir;
    gchar *s;

    rcdir = g_path_get_dirname((const gchar *)g_par.archive_file);
    rcfile = g_path_get_basename((const gchar *)g_par.archive_file);
    f_chooser = orage_file_chooser(intf_w->main_window, TRUE
            , g_par.archive_file, rcdir, rcfile);
    g_free(rcdir);
    g_free(rcfile);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        s = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (s) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->archive_file_entry), s);
            gtk_widget_grab_focus(intf_w->archive_file_entry);
            gtk_editable_set_position(
                    GTK_EDITABLE(intf_w->archive_file_entry), -1);
            g_free(s);
        }
    }
    gtk_widget_destroy(f_chooser);
}
#endif

static void exp_open_button_clicked (G_GNUC_UNUSED GtkButton *button,
                                     gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *entry_filename, *file_path=NULL, *file_name=NULL;
    gchar *cal_file;

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_exp_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        file_path = g_path_get_dirname(entry_filename);
        file_name = g_path_get_basename(entry_filename);
    }
    f_chooser = orage_file_chooser(intf_w->main_window, TRUE
            , entry_filename, file_path, file_name);
    g_free(file_path);
    g_free(file_name);
    g_free(entry_filename);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        cal_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (cal_file) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->iea_exp_entry), cal_file);
            gtk_widget_grab_focus(intf_w->iea_exp_entry);
            gtk_editable_set_position(GTK_EDITABLE(intf_w->iea_exp_entry), -1);
            g_free(cal_file);
        }
    }

    gtk_widget_destroy(f_chooser);
}

static void imp_open_button_clicked (G_GNUC_UNUSED GtkButton *button,
                                     gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *entry_filename, *file_path=NULL;
    gchar *cal_file;

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_imp_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        file_path = g_path_get_dirname(entry_filename);
    }
    f_chooser = orage_file_chooser(intf_w->main_window, FALSE
            , entry_filename, file_path, NULL);
    g_free(file_path);
    g_free(entry_filename);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        cal_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (cal_file) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->iea_imp_entry), cal_file);
            gtk_widget_grab_focus(intf_w->iea_imp_entry);
            gtk_editable_set_position(GTK_EDITABLE(intf_w->iea_imp_entry), -1);
            g_free(cal_file);
        }
    }

    gtk_widget_destroy(f_chooser);
}

#ifdef HAVE_ARCHIVE
static void on_archive_button_clicked_cb (G_GNUC_UNUSED GtkButton *button,
                                          G_GNUC_UNUSED gpointer user_data)
{
    xfical_archive();
}

static void on_unarchive_button_clicked_cb (G_GNUC_UNUSED GtkButton *button,
                                            G_GNUC_UNUSED gpointer user_data)
{
    xfical_unarchive();
}
#endif

gboolean orage_import_file (const gchar *entry_filename)
{
    OrageApplication *app;
    if (xfical_import_file(entry_filename)) {
        app = ORAGE_APPLICATION (g_application_get_default ());
        orage_window_classic_mark_appointments (ORAGE_WINDOW_CLASSIC (
            orage_application_get_window (app)));
        xfical_alarm_build_list(FALSE);
        return(TRUE);
    }
    else
        return(FALSE);
}

static void imp_save_button_clicked (G_GNUC_UNUSED GtkButton *button,
                                     gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    gchar *entry_filename, *filename, *filename_end;
    gboolean more_files;

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_imp_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        more_files = TRUE;
        for (filename = entry_filename; more_files; ) {
            filename_end = g_strstr_len((const gchar *)filename
                    , strlen(filename), ",");
            if (filename_end != NULL)
                *filename_end = 0; /* filename ends here */
            /* FIXME: proper messages to screen */
            if (orage_import_file(filename))
                g_message ("Import done %s", filename);
            else
                g_warning ("import failed file=%s", filename);
            if (filename_end != NULL) { /* we have more files */
                filename = filename_end+1; /* next file starts here */
                more_files = TRUE;
            }
            else {
                more_files = FALSE;
            }
        }
    }
    else
        g_warning ("%s: filename MISSING", G_STRFUNC);
    g_free(entry_filename);
}

static void exp_save_button_clicked (G_GNUC_UNUSED GtkButton *button,
                                     gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    gchar *entry_filename, *entry_uids;
    gint app_count = 0; /* 0 = all, 1 = one */

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_exp_entry));
    entry_uids = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->iea_exp_id_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        /* FIXME: proper messages to screen */
        if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(intf_w->iea_exp_add_all_rb))) {
            app_count = 0;
        }
        else if (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(intf_w->iea_exp_add_id_rb))) {
            app_count = 1;
        }
        else {
            g_warning ("UNKNOWN select appointment");
        }

        if (xfical_export_file (entry_filename, app_count, entry_uids))
            g_message ("Export done %s", entry_filename);
        else
            g_warning ("export failed file=%s", entry_filename);
    }
    else
        g_warning("save_button_clicked: filename MISSING");
    g_free(entry_filename);
    g_free(entry_uids);
}

static void for_open_button_clicked (G_GNUC_UNUSED GtkButton *button,
                                     gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    GtkWidget *f_chooser;
    gchar *entry_filename, *file_path=NULL;
    gchar *cal_file, *cal_name;

    entry_filename = g_strdup(gtk_entry_get_text(
            (GtkEntry *)intf_w->for_new_entry));
    if (ORAGE_STR_EXISTS(entry_filename)) {
        file_path = g_path_get_dirname(entry_filename);
    }
    f_chooser = orage_file_chooser(intf_w->main_window, FALSE
            , entry_filename, file_path, NULL);
    g_free(file_path);
    g_free(entry_filename);

    if (gtk_dialog_run(GTK_DIALOG(f_chooser)) == GTK_RESPONSE_ACCEPT) {
        cal_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(f_chooser));
        if (cal_file) {
            gtk_entry_set_text(GTK_ENTRY(intf_w->for_new_entry), cal_file);
            cal_name = g_path_get_basename(cal_file);
            gtk_entry_set_text(GTK_ENTRY(intf_w->for_new_name_entry), cal_name);
            gtk_widget_grab_focus(intf_w->for_new_name_entry);
            gtk_editable_set_position(GTK_EDITABLE(intf_w->for_new_name_entry)
                    , -1);
            g_free(cal_name);
            g_free(cal_file);
        }
    }

    gtk_widget_destroy(f_chooser);
}

static void orage_foreign_file_remove_line(gint del_line)
{
    int i;
    OrageApplication *app;

    g_free(g_par.foreign_data[del_line].file);
    g_free(g_par.foreign_data[del_line].name);
    g_par.foreign_count--;
    for (i = del_line; i < g_par.foreign_count; i++) {
        g_par.foreign_data[i].file = g_par.foreign_data[i+1].file;
        g_par.foreign_data[i].read_only = g_par.foreign_data[i+1].read_only;
        g_par.foreign_data[i].latest_file_change = g_par.foreign_data[i+1].latest_file_change;
        g_par.foreign_data[i].name = g_par.foreign_data[i+1].name;
    }
    g_par.foreign_data[i].file = NULL;
    g_par.foreign_data[i].name = NULL;

    write_parameters();
    app = ORAGE_APPLICATION (g_application_get_default ());
    orage_window_classic_mark_appointments (ORAGE_WINDOW_CLASSIC (
        orage_application_get_window (app)));
    xfical_alarm_build_list(FALSE);
}

gboolean orage_foreign_file_remove (const gchar *filename)
{
    gint i;
    gboolean found = FALSE;

    if (interface_lock) {
        g_warning ("Exchange window active, can't remove files from cmd line");
        return(FALSE);
    }
    if (!ORAGE_STR_EXISTS(filename)) {
        g_warning("File '%s' is empty. Not removed.", filename);
        return(FALSE);
    }
    for (i = 0; i < g_par.foreign_count && !found; i++) {
        g_warning("file '%s'", g_par.foreign_data[i].file);
        g_warning("name '%s'", g_par.foreign_data[i].name);
        if (strcmp(g_par.foreign_data[i].file, filename) == 0 ||
            strcmp(g_par.foreign_data[i].name, filename) == 0) {
            found = TRUE;
        }
    }
    if (!found) {
        g_warning("File '%s' not found. Not removed.", filename);
        return(FALSE);
    }

    orage_foreign_file_remove_line(i);
    return(TRUE);
}

static void for_remove_button_clicked (G_GNUC_UNUSED GtkButton *button,
                                       gpointer user_data)
{
    gint del_line = GPOINTER_TO_INT(user_data);

    g_message ("%s: Removing foreign file %s (%s).", G_STRFUNC,
               g_par.foreign_data[del_line].name,
               g_par.foreign_data[del_line].file);

    orage_foreign_file_remove_line(del_line);

    g_message ("%s: Foreign file removed and Orage alarms refreshed.",
               G_STRFUNC);
}

static void for_remove_button_clicked2 (G_GNUC_UNUSED GtkButton *button,
                                        gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;

    refresh_foreign_files(intf_w, FALSE);
}

static void refresh_foreign_files(intf_win *intf_w, const gboolean first)
{
    GtkWidget *label, *hbox, *vbox, *button;
    gchar num[3];
    gint i;

    if (!first)
        gtk_widget_destroy(intf_w->for_cur_frame);

    vbox = gtk_grid_new ();
    intf_w->for_cur_frame = orage_create_framebox_with_content(
            _("Current foreign files"), GTK_SHADOW_NONE, vbox);
    g_object_set (intf_w->for_cur_frame, "margin-top", 15,
                                         NULL);
    gtk_grid_attach_next_to (GTK_GRID (intf_w->for_tab_main_vbox),
                             intf_w->for_cur_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    intf_w->for_cur_table = orage_table_new (5);
    if (g_par.foreign_count) {
        for (i = 0; i < g_par.foreign_count; i++) {
            hbox = gtk_grid_new ();
            if (g_par.foreign_data[i].name) {
                label = gtk_label_new(g_par.foreign_data[i].name);
                gtk_widget_set_tooltip_text(label, g_par.foreign_data[i].file);
            }
            else
                label = gtk_label_new(g_par.foreign_data[i].file);

            g_object_set (label, "xalign", 0.0, "yalign", 0.5,
                                 "hexpand", TRUE, "halign", GTK_ALIGN_FILL,
                                 NULL);
            gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL,
                                 GTK_POS_RIGHT, 1, 1);
            if (g_par.foreign_data[i].read_only)
                label = gtk_label_new(_("READ ONLY"));
            else
                label = gtk_label_new(_("READ WRITE"));

            g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
            gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL,
                                     GTK_POS_RIGHT, 1, 1);
            button = orage_util_image_button ("list-remove", _("_Remove"));
            g_object_set (button, "margin-left", 5, "margin-right", 5, NULL);
            gtk_grid_attach_next_to (GTK_GRID (hbox), button, NULL,
                                     GTK_POS_RIGHT, 1, 1);
            g_snprintf (num, sizeof (num), "%02d", i + 1);
            label = gtk_label_new(num);
            orage_table_add_row (intf_w->for_cur_table, label, hbox, i,
                                 OTBL_EXPAND | OTBL_FILL, 0);

            g_signal_connect((gpointer)button, "clicked"
                    , G_CALLBACK(for_remove_button_clicked),GINT_TO_POINTER(i));
            g_signal_connect_after((gpointer)button, "clicked"
                    , G_CALLBACK(for_remove_button_clicked2), intf_w);
        }

        gtk_grid_attach_next_to (GTK_GRID (vbox), intf_w->for_cur_table, NULL,
                                 GTK_POS_BOTTOM, 1, 1);
    }
    else {
        label = gtk_label_new(_("***** No foreign files *****"));
        g_object_set (vbox, "halign", GTK_ALIGN_CENTER, NULL);
        gtk_grid_attach_next_to (GTK_GRID (vbox), label, NULL,
                                 GTK_POS_BOTTOM, 1, 1);
    }
    gtk_widget_show_all(intf_w->for_cur_frame);
}

static gboolean orage_foreign_file_add_internal (const gchar *filename,
                                                 const gchar *name,
                                                 const gboolean read_only,
                                                 GtkWidget *main_window)
{
    gint i = 0;
    OrageApplication *app;
    const gchar *add_failed = _("Foreign file add failed");

    if (g_par.foreign_count > 9) {
        g_warning ("%s: Orage can only handle 10 foreign files. Limit reached. "
                   "New file not added.", G_STRFUNC);
        if (main_window)
        {
            orage_error_dialog (GTK_WINDOW (main_window)
                    , add_failed
                    , _("Orage can only handle 10 foreign files. Limit reached."));
        }
        return(FALSE);
    }
    if (!ORAGE_STR_EXISTS(filename)) {
        g_warning ("%s: File is empty. New file not added.", G_STRFUNC);
        if (main_window)
        {
            orage_error_dialog (GTK_WINDOW (main_window)
                    , add_failed
                    , _("Filename is empty."));
        }
        return(FALSE);
    }
    if (!ORAGE_STR_EXISTS(name)) {
        g_warning ("%s: Name is empty. New file not added.", G_STRFUNC);
        if (main_window)
        {
            orage_error_dialog (GTK_WINDOW (main_window)
                    , add_failed
                    , _("Name is empty."));
        }
        return(FALSE);
    }
    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        g_warning ("%s: New file %s does not exist. New file not added.",
                   G_STRFUNC, filename);
        if (main_window)
        {
            orage_error_dialog (GTK_WINDOW (main_window)
                    , add_failed
                    , _("File does not exist."));
        }
        return(FALSE);
    }
    for (i = 0; i < g_par.foreign_count; i++) {
        if (strcmp(g_par.foreign_data[i].file, filename) == 0) {
            g_warning ("%s: Foreign file already exists. New file not added",
                       G_STRFUNC);
            if (main_window)
            {
                orage_error_dialog (GTK_WINDOW (main_window)
                        , add_failed
                        , _("Same filename already exists in Orage."));
            }
            return(FALSE);
        }
        if (strcmp(g_par.foreign_data[i].name, name) == 0) {
            g_warning ("%s: Foreign file name already exists. "
                       "New file not added", G_STRFUNC);
            if (main_window)
            {
                orage_error_dialog (GTK_WINDOW (main_window)
                        , add_failed
                        , _("Same name already exists in Orage."));
            }
            return(FALSE);
        }
    }

    g_par.foreign_data[g_par.foreign_count].file = g_strdup(filename);
    g_par.foreign_data[g_par.foreign_count].name = g_strdup(name);
    g_par.foreign_data[g_par.foreign_count].read_only = read_only;
    g_par.foreign_data[g_par.foreign_count].latest_file_change = (time_t)0;
    g_par.foreign_count++;

    write_parameters();
    app = ORAGE_APPLICATION (g_application_get_default ());
    orage_window_classic_mark_appointments (ORAGE_WINDOW_CLASSIC (
        orage_application_get_window (app)));
    xfical_alarm_build_list(FALSE);
    return(TRUE);
}

/* this is used from command line */
gboolean orage_foreign_file_add(const gchar *filename, const gboolean read_only
        , const gchar *name)
{
    if (interface_lock) {
        g_warning ("Exchange window active, can't add files from cmd line");
        return(FALSE);
    }
    return(orage_foreign_file_add_internal(filename, name, read_only, NULL));
}

static void for_add_button_clicked (G_GNUC_UNUSED GtkButton *button,
                                    gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;
    const gchar *entry_filename;
    const gchar *entry_name;
    gboolean read_only;

    entry_filename = gtk_entry_get_text((GtkEntry *)intf_w->for_new_entry);
    entry_name = gtk_entry_get_text((GtkEntry *)intf_w->for_new_name_entry);
    read_only = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(intf_w->for_new_read_only));
    if (orage_foreign_file_add_internal (entry_filename, entry_name, read_only,
                                         intf_w->main_window))
    {
        refresh_foreign_files(intf_w, FALSE);
        g_message ("%s: New foreign file %s (%s) added.", G_STRFUNC, entry_name
                , entry_filename);
    }
}

static void close_intf_w(gpointer user_data)
{
    intf_win *intf_w = (intf_win *)user_data;

    gtk_widget_destroy(intf_w->main_window);
    g_free(intf_w);
    interface_lock = FALSE;
}

static void close_button_clicked (G_GNUC_UNUSED GtkButton *button,
                                  gpointer user_data)
{
    close_intf_w(user_data);
}

static void filemenu_close_activated (G_GNUC_UNUSED GtkMenuItem *menuitem,
                                      gpointer user_data)
{
    close_intf_w(user_data);
}

static gboolean on_Window_delete_event (G_GNUC_UNUSED GtkWidget *w,
                                        G_GNUC_UNUSED GdkEvent *e,
                                        gpointer user_data)
{
    close_intf_w(user_data);

    return(FALSE);
}

static void create_menu(intf_win *intf_w)
{
    /* Menu bar */
    intf_w->menubar = gtk_menu_bar_new();
    gtk_grid_attach_next_to (GTK_GRID (intf_w->main_vbox), intf_w->menubar,
                             NULL, GTK_POS_BOTTOM, 1, 1);

    /* File menu */
    intf_w->filemenu = orage_menu_new(_("_File"), intf_w->menubar);

    intf_w->filemenu_close = orage_image_menu_item_for_parent_new_from_stock (
        "gtk-close", intf_w->filemenu, intf_w->accelgroup);

    g_signal_connect((gpointer)intf_w->filemenu_close, "activate"
            , G_CALLBACK(filemenu_close_activated), intf_w);
}

static void create_toolbar(intf_win *intf_w)
{
    /* Toolbar */
    intf_w->toolbar = gtk_toolbar_new();
    gtk_grid_attach_next_to (GTK_GRID (intf_w->main_vbox), intf_w->toolbar,
                             NULL, GTK_POS_BOTTOM, 1, 1);

    /* Buttons */
    intf_w->close_button = orage_toolbar_append_button(intf_w->toolbar
            , "window-close", _("Close"), 0);

    g_signal_connect((gpointer)intf_w->close_button, "clicked"
            , G_CALLBACK(close_button_clicked), intf_w);
}

static void handle_file_drag_data(GtkWidget *widget, GdkDragContext *context
        , GtkSelectionData *data, guint d_time, gboolean imp)
{
    gchar **file_list;
    gchar *file;
    gint i, pos;
    GError *error = NULL;

    if (gtk_selection_data_get_length (data) < 0) {
        g_warning("File drag failed");
        gtk_drag_finish(context, FALSE, FALSE, d_time);
        return;
    }
    file_list = g_uri_list_extract_uris (
            (const gchar *)gtk_selection_data_get_data (data));
    for (i = 0; file_list[i] != NULL; i++) {
        if ((file = g_filename_from_uri(file_list[i], NULL, &error)) == NULL) {
            g_warning("Dragging g_filename_from_uri %s failed %s"
                    , file_list[i], error->message);
            g_error_free(error);
            return;
        }
        if (i == 0) { /* first file (often the only file */
            gtk_entry_set_text(GTK_ENTRY(widget), file);
            gtk_editable_set_position(GTK_EDITABLE(widget), -1);
            pos = gtk_editable_get_position(GTK_EDITABLE(widget));
        }
        else {
            if (imp) { /* quite ok to have more files */
                gtk_editable_insert_text(GTK_EDITABLE(widget), ",", 1, &pos);
                gtk_editable_insert_text(GTK_EDITABLE(widget), file
                        , strlen(file), &pos);
            }
            else { /* export to only 1 file, ignoring the rest */
                g_warning ("Exporting only to one file, "
                           "ignoring drag file %d (%s)", i+1, file_list[i]);
            }
        }
    }
    gtk_drag_finish(context, TRUE, FALSE, d_time);
}

static void imp_file_drag_data_received (GtkWidget *widget,
                                         GdkDragContext *context,
                                         G_GNUC_UNUSED gint x,
                                         G_GNUC_UNUSED gint y,
                                         GtkSelectionData *data,
                                         G_GNUC_UNUSED guint info,
                                         guint d_time)
{
    handle_file_drag_data(widget, context, data, d_time, TRUE);
}

static void exp_file_drag_data_received (GtkWidget *widget,
                                         GdkDragContext *context,
                                         G_GNUC_UNUSED gint x,
                                         G_GNUC_UNUSED gint y,
                                         GtkSelectionData *data,
                                         G_GNUC_UNUSED guint info,
                                         guint d_time)
{
    handle_file_drag_data(widget, context, data, d_time, FALSE);
}

static void uid_drag_data_received (G_GNUC_UNUSED GtkWidget *widget,
                                    GdkDragContext *context,
                                    G_GNUC_UNUSED gint x, G_GNUC_UNUSED gint y,
                                    GtkSelectionData *data,
                                    G_GNUC_UNUSED guint info, guint d_time)
{
    if (gtk_selection_data_get_length (data) < 0) {
        g_warning("UID drag failed");
        gtk_drag_finish(context, FALSE, FALSE, d_time);
        return;
    }
    gtk_drag_finish(context, TRUE, FALSE, d_time);
}

static gboolean drag_drop(GtkWidget *widget, GdkDragContext *context
        , G_GNUC_UNUSED gint x, G_GNUC_UNUSED gint y, guint d_time)
{
    gtk_drag_get_data(widget, context,
             GDK_POINTER_TO_ATOM (gdk_drag_context_list_targets (context)->data)
            , d_time);

    return(TRUE);
}

static void drag_and_drop_init(intf_win *intf_w)
{
    gtk_drag_dest_set(intf_w->iea_imp_entry
            , GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT
            , file_drag_targets, file_drag_target_count, GDK_ACTION_COPY);
    g_signal_connect(intf_w->iea_imp_entry, "drag_drop"
            , G_CALLBACK(drag_drop), NULL);
    g_signal_connect(intf_w->iea_imp_entry, "drag_data_received"
            , G_CALLBACK(imp_file_drag_data_received), NULL);

    gtk_drag_dest_set(intf_w->iea_exp_entry
            , GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT
            , file_drag_targets, file_drag_target_count, GDK_ACTION_COPY);
    g_signal_connect(intf_w->iea_exp_entry, "drag_drop"
            , G_CALLBACK(drag_drop), NULL);
    g_signal_connect(intf_w->iea_exp_entry, "drag_data_received"
            , G_CALLBACK(exp_file_drag_data_received), NULL);

    gtk_drag_dest_set(intf_w->iea_exp_id_entry
            , GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT
            , uid_drag_targets, uid_drag_target_count, GDK_ACTION_COPY);
    g_signal_connect(intf_w->iea_exp_id_entry, "drag_drop"
            , G_CALLBACK(drag_drop), NULL);
    g_signal_connect(intf_w->iea_exp_id_entry, "drag_data_received"
            , G_CALLBACK(uid_drag_data_received), NULL);
}

static void exp_add_all_rb_clicked (G_GNUC_UNUSED GtkWidget *button,
                                    gpointer *user_data)
{
    intf_win *intf_w = (intf_win *)user_data;

    gtk_widget_set_sensitive(intf_w->iea_exp_id_entry, FALSE);
}

static void exp_add_id_rb_clicked (G_GNUC_UNUSED GtkWidget *button,
                                   gpointer *user_data)
{
    intf_win *intf_w = (intf_win *)user_data;

    gtk_widget_set_sensitive(intf_w->iea_exp_id_entry, TRUE);
}

static void create_import_export_tab(intf_win *intf_w)
{
    GtkWidget *label, *hbox, *vbox, *m_vbox;
    gchar *file;
    gchar *str;

    m_vbox = gtk_grid_new ();
    intf_w->iea_notebook_page = orage_create_framebox_with_content(
            NULL, GTK_SHADOW_NONE, m_vbox);
    intf_w->iea_tab_label = gtk_label_new(_("Import/export"));
    gtk_notebook_append_page(GTK_NOTEBOOK(intf_w->notebook)
            , intf_w->iea_notebook_page, intf_w->iea_tab_label);

    /***** import *****/
    vbox = gtk_grid_new ();
    intf_w->iea_imp_frame = orage_create_framebox_with_content(
            _("Import"), GTK_SHADOW_NONE, vbox);
    g_object_set (intf_w->iea_imp_frame, "margin-top", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (m_vbox),
                             intf_w->iea_imp_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Read from file:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->iea_imp_entry = gtk_entry_new();
    g_object_set (intf_w->iea_imp_entry, "hexpand", TRUE,
                                         "halign", GTK_ALIGN_FILL,
                                         NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_imp_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);
    intf_w->iea_imp_open_button = orage_util_image_button ("document-open",
                                                           _("_Open"));
    g_object_set (intf_w->iea_imp_open_button, "margin-left", 5,
                                               "margin-right", 5,
                                               NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_imp_open_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    intf_w->iea_imp_save_button = orage_util_image_button ("document-save",
                                                         _("_Save"));
    g_object_set (intf_w->iea_imp_save_button, "margin-left", 5,
                                               "margin-right", 5,
                                               NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_imp_save_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect((gpointer)intf_w->iea_imp_open_button, "clicked"
            , G_CALLBACK(imp_open_button_clicked), intf_w);
    g_signal_connect((gpointer)intf_w->iea_imp_save_button, "clicked"
            , G_CALLBACK(imp_save_button_clicked), intf_w);
    gtk_widget_set_tooltip_text(intf_w->iea_imp_entry 
            , _("Separate filenames with comma(,).\n NOTE: comma is not valid character in filenames for Orage."));

    /***** export *****/
    vbox = gtk_grid_new ();
    intf_w->iea_exp_frame = orage_create_framebox_with_content(
            _("Export"), GTK_SHADOW_NONE, vbox);
    g_object_set (intf_w->iea_exp_frame, "margin-top", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (m_vbox),
                             intf_w->iea_exp_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Write to file:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->iea_exp_entry = gtk_entry_new();
    g_object_set (intf_w->iea_exp_entry, "hexpand", TRUE,
                                         "halign", GTK_ALIGN_FILL,
                                         NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_exp_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);
    file = g_build_filename(g_get_home_dir(), "orage_export.ics", NULL);
    gtk_entry_set_text(GTK_ENTRY(intf_w->iea_exp_entry), file);
    g_free(file);
    intf_w->iea_exp_open_button = orage_util_image_button ("document-open",
                                                           _("_Open"));
    g_object_set (intf_w->iea_exp_open_button, "margin-left", 5,
                                               "margin-right", 5,
                                               NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_exp_open_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    intf_w->iea_exp_save_button = orage_util_image_button ("document-save",
                                                           _("_Save"));
    g_object_set (intf_w->iea_exp_save_button, "margin-left", 5,
                                               "margin-right", 5,
                                               NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_exp_save_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect((gpointer)intf_w->iea_exp_open_button, "clicked"
            , G_CALLBACK(exp_open_button_clicked), intf_w);
    g_signal_connect((gpointer)intf_w->iea_exp_save_button, "clicked"
            , G_CALLBACK(exp_save_button_clicked), intf_w);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Select"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    intf_w->iea_exp_add_all_rb = gtk_radio_button_new_with_label(NULL
            , _("All appointments"));
    g_object_set (intf_w->iea_exp_add_all_rb, "margin-left", 5,
                                              "margin-right", 5,
                                              NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_exp_add_all_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect((gpointer)intf_w->iea_exp_add_all_rb, "clicked"
            , G_CALLBACK(exp_add_all_rb_clicked), intf_w);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Select"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL,
                             GTK_POS_BOTTOM, 1, 1);
    intf_w->iea_exp_add_id_rb = gtk_radio_button_new_with_mnemonic_from_widget(
            GTK_RADIO_BUTTON(intf_w->iea_exp_add_all_rb)
            , _("Named appointments:"));
    g_object_set (intf_w->iea_exp_add_id_rb, "margin-left", 5,
                                             "margin-right", 5,
                                              NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_exp_add_id_rb, NULL,
                             GTK_POS_RIGHT, 1, 1);
    intf_w->iea_exp_id_entry = gtk_entry_new ();
    g_object_set (intf_w->iea_exp_id_entry, "hexpand", TRUE,
                                            "halign", GTK_ALIGN_FILL,
                                            NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_exp_id_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_sensitive(intf_w->iea_exp_id_entry, FALSE);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect((gpointer)intf_w->iea_exp_add_id_rb, "clicked"
            , G_CALLBACK(exp_add_id_rb_clicked), intf_w);
#ifdef HAVE_ARCHIVE
    gtk_widget_set_tooltip_text(intf_w->iea_exp_add_all_rb
            , _("Note that only main file appointments are read.\nArchived and Foreign events are not exported."));
#else
    gtk_widget_set_tooltip_text(intf_w->iea_exp_add_all_rb
            , _("Note that only main file appointments are read.\nForeign events are not exported."));
#endif
    gtk_widget_set_tooltip_text(intf_w->iea_exp_add_id_rb
            , _("You can easily drag these from event-list window."));
    gtk_widget_set_tooltip_text(intf_w->iea_exp_id_entry
            , _("Orage appointment UIDs separated by commas."));

#ifdef HAVE_ARCHIVE
    /***** archive *****/
    vbox = gtk_grid_new ();
    intf_w->iea_arc_frame = orage_create_framebox_with_content(
            _("Archive"), GTK_SHADOW_NONE, vbox);
    g_object_set (intf_w->iea_arc_frame, "margin-top", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (m_vbox),
                             intf_w->iea_arc_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    intf_w->iea_arc_button1 = orage_util_image_button ("system-run",
                                                       _("_Execute"));
    g_object_set (intf_w->iea_arc_button1, "margin-left", 5,
                                           "margin-right", 5,
                                           NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_arc_button1, NULL,
                             GTK_POS_RIGHT, 1, 1);
    str = g_strdup_printf(_("Archive now (threshold: %d months)")
            , g_par.archive_limit);
    label = gtk_label_new(str);
    g_free(str);
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect((gpointer)intf_w->iea_arc_button1, "clicked"
            , G_CALLBACK(on_archive_button_clicked_cb), intf_w);
    gtk_widget_set_tooltip_text(intf_w->iea_arc_button1
            , _("You can change archive threshold in parameters"));

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    intf_w->iea_arc_button2 = orage_util_image_button ("system-run",
                                                       _("_Execute"));
    g_object_set (intf_w->iea_arc_button2, "margin-left", 5,
                                           "margin-right", 5,
                                           NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->iea_arc_button2, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);
    label = gtk_label_new(_("Revert archive now"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);

    g_signal_connect((gpointer)intf_w->iea_arc_button2, "clicked"
            , G_CALLBACK(on_unarchive_button_clicked_cb), intf_w);
    gtk_widget_set_tooltip_text(intf_w->iea_arc_button2
            , _("Return all archived events into main orage file and remove arch file.\nThis is useful for example when doing export and moving orage\nappointments to another system."));
#endif
}

static void create_orage_file_tab(intf_win *intf_w)
{
    GtkWidget *label, *hbox, *vbox, *m_vbox;

    m_vbox = gtk_grid_new ();
    /* FIXME: this could be simpler than framebox */
    intf_w->fil_notebook_page = orage_create_framebox_with_content (
            NULL, GTK_SHADOW_NONE, m_vbox);
    intf_w->fil_tab_label = gtk_label_new(_("Orage files"));
    gtk_notebook_append_page(GTK_NOTEBOOK(intf_w->notebook)
            , intf_w->fil_notebook_page, intf_w->fil_tab_label);

    /***** main file *****/
    vbox = gtk_grid_new ();
    intf_w->orage_file_frame = orage_create_framebox_with_content(
            _("Orage main calendar file"), GTK_SHADOW_NONE, vbox);
    g_object_set (intf_w->orage_file_frame, "margin-top", 5,
                                            "margin-bottom", 5,
                                            NULL);
    gtk_grid_attach_next_to (GTK_GRID (m_vbox), intf_w->orage_file_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Current file:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    label = gtk_label_new (g_par.orage_file);
    g_object_set (label, "margin-right", 5,
                         "hexpand", TRUE, "halign", GTK_ALIGN_START,
                         NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("New file:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->orage_file_entry = gtk_entry_new();
    gtk_entry_set_text (GTK_ENTRY(intf_w->orage_file_entry), g_par.orage_file);
    g_object_set (intf_w->orage_file_entry,
                  "hexpand", TRUE, "halign", GTK_ALIGN_FILL, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->orage_file_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);

    intf_w->orage_file_open_button = orage_util_image_button ("document-open",
                                                              _("_Open"));
    g_object_set (intf_w->orage_file_open_button,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->orage_file_open_button,
                             NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->orage_file_save_button = orage_util_image_button ("document-save",
                                                              _("_Save"));
    gtk_widget_set_sensitive(intf_w->orage_file_save_button, FALSE);
    g_object_set (intf_w->orage_file_save_button,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->orage_file_save_button,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Action options:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->orage_file_rename_rb = 
            gtk_radio_button_new_with_label(NULL, _("Rename"));
    g_object_set (intf_w->orage_file_rename_rb,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->orage_file_rename_rb,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(intf_w->orage_file_rename_rb
            , _("Orage internal file rename only.\nDoes not touch external filesystem at all.\nNew file must exist."));

    intf_w->orage_file_copy_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                    GTK_RADIO_BUTTON(intf_w->orage_file_rename_rb), _("Copy"));
    g_object_set (intf_w->orage_file_copy_rb,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->orage_file_copy_rb,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(intf_w->orage_file_copy_rb
            , _("Current file is copied and stays unmodified in the old place."));

    intf_w->orage_file_move_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                    GTK_RADIO_BUTTON(intf_w->orage_file_rename_rb), _("Move"));
    g_object_set (intf_w->orage_file_move_rb,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->orage_file_move_rb,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_widget_set_tooltip_text(intf_w->orage_file_move_rb
            , _("Current file is moved and vanishes from the old place."));
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect(G_OBJECT(intf_w->orage_file_open_button), "clicked"
            , G_CALLBACK(orage_file_open_button_clicked), intf_w);
    g_signal_connect(G_OBJECT(intf_w->orage_file_entry), "changed"
            , G_CALLBACK(orage_file_entry_changed), intf_w);
    g_signal_connect(G_OBJECT(intf_w->orage_file_save_button), "clicked"
            , G_CALLBACK(orage_file_save_button_clicked), intf_w);

#ifdef HAVE_ARCHIVE
    /***** archive file *****/
    vbox = gtk_grid_new ();
    intf_w->archive_file_frame = orage_create_framebox_with_content(
            _("Archive file"), GTK_SHADOW_NONE, vbox);
    g_object_set (intf_w->archive_file_frame, "margin-top", 5,
                                              "margin-bottom", 5,
                                              NULL);
    gtk_grid_attach_next_to (GTK_GRID (m_vbox), intf_w->archive_file_frame,
                             NULL, GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Current file:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    label = gtk_label_new((const gchar *)g_par.archive_file);
    g_object_set (label, "margin-right", 5,
                         "hexpand", TRUE, "halign", GTK_ALIGN_START,
                         NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("New file:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->archive_file_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(intf_w->archive_file_entry)
            , (const gchar *)g_par.archive_file);
    g_object_set (intf_w->archive_file_entry,
                  "hexpand", TRUE, "halign", GTK_ALIGN_FILL, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->archive_file_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);
    intf_w->archive_file_open_button = orage_util_image_button ("document-open",
                                                                _("_Open"));
    g_object_set (intf_w->archive_file_open_button,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->archive_file_open_button,
                             NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->archive_file_save_button = orage_util_image_button ("document-save",
                                                                _("_Save"));
    gtk_widget_set_sensitive(intf_w->archive_file_save_button, FALSE);
    g_object_set (intf_w->archive_file_save_button,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->archive_file_save_button,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Action options:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->archive_file_rename_rb = 
            gtk_radio_button_new_with_label(NULL, _("Rename"));
    g_object_set (intf_w->archive_file_rename_rb,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->archive_file_rename_rb,
                             NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->archive_file_copy_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                GTK_RADIO_BUTTON(intf_w->archive_file_rename_rb), _("Copy"));
    g_object_set (intf_w->archive_file_copy_rb,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->archive_file_copy_rb,
                             NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->archive_file_move_rb = 
            gtk_radio_button_new_with_mnemonic_from_widget(
                GTK_RADIO_BUTTON(intf_w->archive_file_rename_rb), _("Move"));
    g_object_set (intf_w->archive_file_move_rb,
                  "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->archive_file_move_rb,
                             NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect(G_OBJECT(intf_w->archive_file_open_button), "clicked"
            , G_CALLBACK(archive_file_open_button_clicked), intf_w);
    g_signal_connect(G_OBJECT(intf_w->archive_file_entry), "changed"
            , G_CALLBACK(archive_file_entry_changed), intf_w);
    g_signal_connect(G_OBJECT(intf_w->archive_file_save_button), "clicked"
            , G_CALLBACK(archive_file_save_button_clicked), intf_w);
#endif
}

static void create_foreign_file_tab(intf_win *intf_w)
{
    GtkWidget *label, *hbox, *vbox;

    intf_w->for_tab_main_vbox = gtk_grid_new ();
    /* FIXME: this could be simpler than framebox */
    intf_w->for_notebook_page =
            orage_create_framebox_with_content (NULL, GTK_SHADOW_NONE,
                                                intf_w->for_tab_main_vbox);
    intf_w->for_tab_label = gtk_label_new(_("Foreign files"));
    gtk_notebook_append_page(GTK_NOTEBOOK(intf_w->notebook)
            , intf_w->for_notebook_page, intf_w->for_tab_label);

    /***** Add new file *****/
    vbox = gtk_grid_new ();
    intf_w->for_new_frame = orage_create_framebox_with_content(
            _("Add new foreign file"), GTK_SHADOW_NONE, vbox);

    gtk_grid_attach_next_to (GTK_GRID (intf_w->for_tab_main_vbox),
                             intf_w->for_new_frame, NULL,
                             GTK_POS_BOTTOM, 1, 1);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Foreign file:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->for_new_entry = gtk_entry_new();
    g_object_set (intf_w->for_new_entry,
                  "hexpand", TRUE, "halign", GTK_ALIGN_FILL, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->for_new_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);
    intf_w->for_new_open_button = orage_util_image_button ("document-open",
                                                           _("_Open"));
    g_object_set (intf_w->for_new_open_button, "margin-left", 5,
                                               "margin-right", 5,
                                               NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->for_new_open_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    intf_w->for_new_save_button = orage_util_image_button ("list-add",
                                                           _("_Add"));
    g_object_set (intf_w->for_new_save_button, "margin-left", 5,
                                               "margin-right", 5,
                                               NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->for_new_save_button, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);

    g_signal_connect((gpointer)intf_w->for_new_open_button, "clicked"
            , G_CALLBACK(for_open_button_clicked), intf_w);
    g_signal_connect((gpointer)intf_w->for_new_save_button, "clicked"
            , G_CALLBACK(for_add_button_clicked), intf_w);

    hbox = gtk_grid_new ();
    g_object_set (hbox, "margin-top", 5, "margin-bottom", 5, NULL);
    label = gtk_label_new(_("Visible name:"));
    g_object_set (label, "margin-left", 5, "margin-right", 5, NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), label, NULL, GTK_POS_RIGHT, 1, 1);
    intf_w->for_new_name_entry = gtk_entry_new();
    g_object_set (intf_w->for_new_name_entry, "hexpand", TRUE,
                                              "halign", GTK_ALIGN_FILL,
                                              NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->for_new_name_entry, NULL,
                             GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);
    intf_w->for_new_read_only = gtk_check_button_new_with_label(_("Read only"));
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(intf_w->for_new_read_only), TRUE);
    g_object_set (intf_w->for_new_read_only, "margin-left", 5,
                                             "margin-right", 5,
                                             NULL);
    gtk_grid_attach_next_to (GTK_GRID (hbox), intf_w->for_new_read_only, NULL,
                             GTK_POS_RIGHT, 1, 1);

    gtk_widget_set_tooltip_text(intf_w->for_new_read_only
            , _("Set this if you want to make sure that this file is never modified by Orage.\nNote that modifying foreign files may make them incompatible with the original tool, where they came from!"));
    gtk_widget_set_tooltip_text(intf_w->for_new_name_entry
            , _("This internal name is displayed to user instead of file name."));

    /***** Current files *****/
    refresh_foreign_files(intf_w, TRUE);
}

void orage_external_interface (void)
{
    intf_win *intf_w = g_new(intf_win, 1);

    interface_lock = TRUE;
     /* main window creation and base elements */
    intf_w->main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(intf_w->main_window)
            , _("Exchange data - Orage"));
    gtk_window_set_default_size(GTK_WINDOW(intf_w->main_window), 300, 200);

    intf_w->accelgroup = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(intf_w->main_window)
            , intf_w->accelgroup);

    intf_w->main_vbox = gtk_grid_new ();
    gtk_container_add(GTK_CONTAINER(intf_w->main_window), intf_w->main_vbox);

    create_menu(intf_w);
    create_toolbar(intf_w);

     /* create tabs */
    intf_w->notebook = gtk_notebook_new();
    gtk_grid_attach_next_to (GTK_GRID (intf_w->main_vbox), intf_w->notebook,
                             NULL, GTK_POS_BOTTOM, 1, 1);
    gtk_container_set_border_width(GTK_CONTAINER(intf_w->notebook), 5);

    create_import_export_tab(intf_w);
    create_orage_file_tab(intf_w);
    create_foreign_file_tab(intf_w);

    g_signal_connect((gpointer)intf_w->main_window, "delete_event",
            G_CALLBACK(on_Window_delete_event), intf_w);

    gtk_widget_show_all(intf_w->main_window);
    drag_and_drop_init(intf_w);
}
