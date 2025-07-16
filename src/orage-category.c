/*
 * Copyright (c) 2021-2023 Erkki Moorits
 * Copyright (c) 2005-2013 Juha Kautto  (juha at xfce.org)
 * Copyright (c) 2004-2005 Mickael Graf (korbinus at xfce.org)
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

#include "orage-category.h"

#include "functions.h"
#include "orage-rc-file.h"

#include <gdk/gdk.h>
#include <glib.h>

#define ORAGE_RC_COLOUR "Color"

static GList *orage_category_list = NULL;

static void orage_category_free (gpointer gcat, G_GNUC_UNUSED gpointer dummy)
{
    orage_category_struct *cat = (orage_category_struct *)gcat;

    g_free (cat->category);
    g_free (cat);
}

static void orage_category_free_list (void)
{
    g_list_foreach (orage_category_list, orage_category_free, NULL);
    g_list_free (orage_category_list);
    orage_category_list = NULL;
}

OrageRc *orage_category_file_open (const gboolean read_only)
{
    gchar *fpath;
    OrageRc *orc;

    fpath = orage_data_file_location (ORAGE_CATEGORIES_DIR_FILE);
    orc = orage_rc_file_open (fpath, read_only);
    if (orc == NULL)
        g_warning ("%s: category file open failed.", G_STRFUNC);

    g_free (fpath);

    return orc;
}

GList *orage_category_get_list (void)
{
    GdkRGBA rgba;
    OrageRc *orc;
    gchar **cat_groups;
    gint i;
    orage_category_struct *cat;

    if (orage_category_list != NULL)
        orage_category_free_list ();

    orc = orage_category_file_open (TRUE);
    cat_groups = orage_rc_get_groups (orc);
    for (i = 0; cat_groups[i] != NULL; i++)
    {
        orage_rc_set_group (orc, cat_groups[i]);
        if (orage_rc_get_color (orc, ORAGE_RC_COLOUR, &rgba, NULL))
        {
            cat = g_new (orage_category_struct, 1);
            cat->category = g_strdup (cat_groups[i]);
            cat->color = rgba;
            orage_category_list = g_list_prepend (orage_category_list, cat);
        }
    }
    g_strfreev (cat_groups);
    orage_rc_file_close (orc);

    return orage_category_list;
}

GdkRGBA *orage_category_list_contains (const gchar *categories)
{
    GList *cat_l;
    orage_category_struct *cat;

    if (categories == NULL)
        return NULL;

    for (cat_l = g_list_first (orage_category_list);
         cat_l != NULL;
         cat_l = g_list_next(cat_l))
    {
        cat = (orage_category_struct *)cat_l->data;
        if (g_str_has_suffix (categories, cat->category))
            return &cat->color;
    }

    /* Not found. */
    return NULL;
}

void orage_category_write_entry (const gchar *category, const GdkRGBA *color)
{
    OrageRc *orc;
    gchar *color_str;

    if (!ORAGE_STR_EXISTS(category))
    {
        g_message ("%s: empty category. Not written", G_STRFUNC);
        return;
    }

    color_str = gdk_rgba_to_string (color);
    orc = orage_category_file_open (FALSE);
    orage_rc_set_group (orc, category);
    orage_rc_put_str (orc, ORAGE_RC_COLOUR, color_str);
    g_free (color_str);
    orage_rc_file_close (orc);
}

void orage_category_remove_entry (const gchar *category)
{
    OrageRc *orc;

    if (!ORAGE_STR_EXISTS(category))
    {
        g_message ("%s: empty category. Not removed", G_STRFUNC);
        return;
    }
    orc = orage_category_file_open (FALSE);
    orage_rc_del_group (orc, category);
    orage_rc_file_close (orc);
}
