/*
 * Copyright (c) 2021-2023 Erkki Moorits
 * Copyright (c) 2005-2011 Juha Kautto   (juha at xfce.org)
 * Copyright (c) 2004-2006 Mickael Graf  (korbinus at xfce.org)
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

#include <gtk/gtk.h>
#include <glib.h>

#include "orage-i18n.h"
#include "about-xfcalendar.h"
#include "functions.h"

void create_wAbout (G_GNUC_UNUSED GtkWidget *widget,
                    G_GNUC_UNUSED gpointer user_data)
{
  GtkWidget *dialog;
  GtkAboutDialog *about;
  const gchar *authors[] = {"Erkki Moorits <erkki.moorits@mail.ee>",
                            "Juha Kautto <juha@xfce.org>",
                            "Benedikt Meurer <benny@xfce.org>",
                            "Mickael Graf <korbinus@xfce.org>",
                            NULL};

  dialog = gtk_about_dialog_new();
  about = (GtkAboutDialog *) dialog;
  gtk_about_dialog_set_program_name(about, "Orage");
  gtk_about_dialog_set_version(about, VERSION);
  gtk_about_dialog_set_copyright(about, "Copyright © 2003-2023 Orage Team");
  gtk_about_dialog_set_comments(about, _("Orage is a time-managing application for the Xfce desktop environment"));
  gtk_about_dialog_set_website(about, "https://docs.xfce.org/apps/orage/start");
  gtk_about_dialog_set_authors(about, authors);
  gtk_about_dialog_set_license_type(about, GTK_LICENSE_GPL_2_0);
  gtk_about_dialog_set_translator_credits(about, _("translator-credits"));
  gtk_about_dialog_set_logo_icon_name(about, ORAGE_APP_ID);

  gtk_window_set_default_size(GTK_WINDOW(dialog), 520, 440);

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}
