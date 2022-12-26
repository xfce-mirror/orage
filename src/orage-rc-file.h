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

#ifndef ORAGE_RC_FILE_H
#define ORAGE_RC_FILE_H 1

#include <gdk/gdk.h>
#include <glib.h>

typedef struct _OrageRc OrageRc;

OrageRc *orage_rc_file_open (const gchar *fpath, gboolean read_only);
void orage_rc_file_close (OrageRc *orc);

gchar **orage_rc_get_groups (OrageRc *orc);
void orage_rc_set_group (OrageRc *orc, const gchar *grp);
void orage_rc_del_group (OrageRc *orc, const gchar *grp);
gchar *orage_rc_get_group (OrageRc *orc);

gboolean orage_rc_exists_item (OrageRc *orc, const gchar *key);
void orage_rc_del_item (OrageRc *orc, const gchar *key);

gchar *orage_rc_get_str (OrageRc *orc, const gchar *key, const gchar *def);
void orage_rc_put_str (OrageRc *orc, const gchar *key, const gchar *val);

gint orage_rc_get_int (OrageRc *orc, const gchar *key, gint def);
void orage_rc_put_int (OrageRc *orc, const gchar *key, gint val);

gboolean orage_rc_get_bool (OrageRc *orc, const gchar *key, gboolean def);
void orage_rc_put_bool (OrageRc *orc, const gchar *key, gboolean val);

GDateTime *orage_rc_get_gdatetime (OrageRc *orc, const gchar *key,
                                   GDateTime *def);

void orage_rc_put_gdatetime (OrageRc *orc, const gchar *key, GDateTime *gdt);

/** Read RGBA colour from configuration file.
 *  described in orage rc, then if colour is not
 *  @param orc Orage RC file refernce
 *  @param key key for colour
 *  @param rgba output colour.
 *  @param def default colour. This colour is used only when colour is not
 *         present or not pareseable in given rc file.
 *  @retrun true when output rgba is updated, false if output not updated.
 */
gboolean orage_rc_get_color (OrageRc *orc, const gchar *key,
                             GdkRGBA *rgba, const gchar *def);

#endif
