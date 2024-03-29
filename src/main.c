/* xfcalendar
 *
 * Copyright (C) 2002 Mickael Graf (korbinus@linux.se)
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.  You
 * should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */

/*
 * Initial main.c file generated by Glade. Edit as required.
 * Glade will not overwrite this file.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <gtk/gtk.h>

#include "interface.h"
#include "support.h"

void
createRCDir(void)
{
	gchar *calpath;

	calpath = xfce_get_userfile("xfcalendar", NULL);

	if (!g_file_test(calpath, G_FILE_TEST_IS_DIR)) {
		if (mkdir(calpath, 0755) < 0) {
			g_error("Unable to create directory %s: %s",
					calpath, g_strerror(errno));
		}
	}

	g_free(calpath);
}

int
main(int argc, char *argv[])
{
	GtkWidget *window1;

	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	gtk_set_locale();
	gtk_init(&argc, &argv);

	add_pixmap_directory(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE
			G_DIR_SEPARATOR_S "pixmaps");

	/*
	 * The following code was added by Glade to create one of each
	 * component (except popup menus), just so that you see something
	 * after building the project. Delete any components that you don't
	 * want shown initially.
	 */
	window1 = create_XFCalendar();
	gtk_widget_show(window1);

	/*
	 * Now it's serious, the application is running, so we create the RC
	 * directory
	 */
	createRCDir();

	gtk_main();

	return(EXIT_SUCCESS);
}

