/*
 * Copyright (c) 2021-2025 Erkki Moorits
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
#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>

#include "orage-i18n.h"
#include "orage-about.h"
#include "functions.h"

void orage_show_about (GtkWindow *parent)
{
    static const gchar *authors[] = {"Erkki Moorits <erkki.moorits@mail.ee>",
                                     "Juha Kautto <juha@xfce.org>",
                                     "Benedikt Meurer <benny@xfce.org>",
                                     "Mickael Graf <korbinus@xfce.org>",
                                     NULL};

    gtk_show_about_dialog (parent,
                           "authors", authors,
                           "comments", _("Orage is a time-managing application for the Xfce desktop environment"),
                           "copyright", "Copyright \xc2\xa9 2003-2025 Orage Team",
                           "destroy-with-parent", TRUE,
                           "license-type", GTK_LICENSE_GPL_2_0,
                           "logo-icon-name", ORAGE_APP_ID,
                           "program-name", "Orage",
                           "version", VERSION_FULL,
                           "translator-credits", _("translator-credits"),
                           "website", ORAGE_DOC_ADDRESS,
                           NULL);
}
