/*
 * Copyright (c) 2021-2023 Erkki Moorits
 * Copyright (c) 2008-2011 Juha Kautto  (juha at xfce.org)
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

#ifndef __TIMEZONE_SELECTION_H__
#define __TIMEZONE_SELECTION_H__

gboolean orage_timezone_button_clicked(GtkButton *button, GtkWindow *parent
        , gchar **tz, gboolean check_ical, char *local_tz);

#endif /* !__TIMEZONE_SELECTION_H__ */
