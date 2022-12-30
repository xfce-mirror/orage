/*      Orage - Calendar and alarm handler
 *
 * Copyright (c) 2022 Erkki Moorits
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
       Free Software Foundation
       51 Franklin Street, 5th Floor
       Boston, MA 02110-1301 USA

 */

#include "orage-application.h"
#include "functions.h"
#include <gtk/gtk.h>

struct _OrageApplication
{
    GtkApplication parent;
#if 0
    GtkWidget *window;
#endif
};

G_DEFINE_TYPE (OrageApplication, orage_application, GTK_TYPE_APPLICATION);

static void orage_application_class_init (OrageApplicationClass *klass)
{
    GObjectClass *object_class;
    GApplicationClass *application_class;

    object_class = G_OBJECT_CLASS(klass);
    
    application_class = G_APPLICATION_CLASS(klass);
#if 0
    application_class->activate = ocal_application_activate;
    application_class->open = ocal_application_open;
    application_class->command_line = ocal_application_command_line;
#endif
}

static void orage_application_init (OrageApplication *self)
{
}

OrageApplication *orage_application_new (void)
{
    return g_object_new (ORAGE_APPLICATION_TYPE,
                         "application-id", ORAGE_APP_ID,
                         "flags", G_APPLICATION_HANDLES_COMMAND_LINE |
                                  G_APPLICATION_HANDLES_OPEN,
                         NULL);
}
