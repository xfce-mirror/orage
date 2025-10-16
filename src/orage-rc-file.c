/*
 * Copyright (c) 2022-2023 Erkki Moorits
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


/*******************************************************
 * rc file interface
 *******************************************************/

#include "orage-rc-file.h"
#include "functions.h"
#include "orage-time-utils.h"
#include <gdk/gdk.h>
#include <glib.h>

struct _OrageRc
{
    GKeyFile *rc;
    gboolean read_only;
    gchar *file_name;
    gchar *cur_group;
};

static OrageRc *orage_rc_new (GKeyFile *grc,
                              const gchar *fpath,
                              const gboolean read_only)
{
    OrageRc *orc;

    orc = g_new (OrageRc, 1);
    orc->rc = grc;
    orc->read_only = read_only;
    orc->file_name = g_strdup (fpath);
    orc->cur_group = NULL;

    return orc;
}

static void orage_rc_free (OrageRc *orc)
{
    g_key_file_free (orc->rc);
    g_free (orc->file_name);
    g_free (orc->cur_group);
    g_free (orc);
}

OrageRc *orage_rc_file_open (const gchar *fpath, const gboolean read_only)
{
    OrageRc *orc;
    GKeyFile *grc;
    GError *error = NULL;

    g_debug ("%s: fpath='%s', %s", G_STRFUNC, fpath, read_only ? "RO" : "RW");

    grc = g_key_file_new ();
    if (g_key_file_load_from_file (grc, fpath, G_KEY_FILE_KEEP_COMMENTS, &error))
        orc = orage_rc_new (grc, fpath, read_only);
    else
    {
        g_warning ("Unable to open RC file (%s). Creating it. (%s)", fpath,
                   error->message);
        g_clear_error (&error);
        g_key_file_free (grc);
        orc = orage_rc_file_new (fpath);
    }

    return orc;
}

OrageRc *orage_rc_file_new (const gchar *fpath)
{
    OrageRc *orc = NULL;
    GKeyFile *grc;
    GError *error = NULL;

    g_debug ("%s: fpath='%s'", G_STRFUNC, fpath);

    grc = g_key_file_new ();
    if (g_file_set_contents (fpath, "#Created by Orage", -1, &error))
        orc = orage_rc_new (grc, fpath, FALSE);
    else
    {
        g_warning ("Unable to create RC file (%s). (%s)", fpath,
                   error->message);
        g_key_file_free (grc);
        g_error_free (error);
    }

    return orc;
}

/* FIXME: check if file contents have been changed and only write when needed or
 * build separate save function */
void orage_rc_file_close (OrageRc *orc)
{
    GError *error = NULL;

    if (orc == NULL)
    {
        g_debug ("%s: closing empty file.", G_STRFUNC);
        return;
    }

    g_debug ("%s: close='%s'", G_STRFUNC, orc->file_name);

    if (orc->read_only == FALSE)
    {
        g_debug ("%s: saving content='%s'", G_STRFUNC, orc->file_name);
        if (g_key_file_save_to_file (orc->rc, orc->file_name, &error) == FALSE)
        {
            g_warning ("%s: File save failed. RC file (%s). (%s)",
                       G_STRFUNC, orc->file_name, error->message);
            g_error_free (error);
        }
    }

    orage_rc_free (orc);
}

gchar **orage_rc_get_groups (OrageRc *orc)
{
    return g_key_file_get_groups (orc->rc, NULL);
}

void orage_rc_set_group (OrageRc *orc, const gchar *grp)
{
    g_free (orc->cur_group);
    orc->cur_group = g_strdup (grp);
}

void orage_rc_del_group (OrageRc *orc, const gchar *grp)
{
    GError *error = NULL;

    if (!g_key_file_remove_group (orc->rc, grp, &error))
    {
        g_debug ("%s: Group remove failed. RC file (%s). group (%s) (%s)",
                 G_STRFUNC, orc->file_name, grp, error->message);
        g_error_free (error);
    }
}

gchar *orage_rc_get_group (OrageRc *orc)
{
    return g_strdup (orc->cur_group);
}

gboolean orage_rc_exists_item (OrageRc *orc, const gchar *key)
{
    return g_key_file_has_key (orc->rc, orc->cur_group, key, NULL);
}

void orage_rc_del_item (OrageRc *orc, const gchar *key)
{
    g_key_file_remove_key (orc->rc, orc->cur_group, key, NULL);
}

gchar *orage_rc_get_str (OrageRc *orc, const gchar *key, const gchar *def)
{
    GError *error = NULL;
    gchar *ret;

    ret = g_key_file_get_string (orc->rc, orc->cur_group, key, &error);
    if (!ret && error)
    {
        ret = g_strdup (def);
        g_debug ("%s: str (%s) group (%s) in RC file (%s) not found, "
                 "using default (%s)", G_STRFUNC, key, orc->cur_group,
                 orc->file_name, ret);
        
        g_error_free (error);
    }
    return ret;
}

void orage_rc_put_str (OrageRc *orc, const gchar *key, const gchar *val)
{
    if (ORAGE_STR_EXISTS (val))
        g_key_file_set_string (orc->rc, orc->cur_group, key, val);
}

gint orage_rc_get_int (OrageRc *orc, const gchar *key, const gint def)
{
    GError *error = NULL;
    gint ret;

    ret = g_key_file_get_integer (orc->rc, orc->cur_group, key, &error);
    if (!ret && error)
    {
        ret = def;
        g_debug ("%s: str (%s) group (%s) in RC file (%s) not found, "
                 "using default (%d)", G_STRFUNC, key, orc->cur_group,
                 orc->file_name, ret);
        g_error_free (error);
    }

    return ret;
}

void orage_rc_put_int (OrageRc *orc, const gchar *key, const gint val)
{
    g_key_file_set_integer (orc->rc, orc->cur_group ,key, val);
}

gboolean orage_rc_get_bool (OrageRc *orc, const gchar *key, const gboolean def)
{
    GError *error = NULL;
    gboolean ret;

    ret = g_key_file_get_boolean (orc->rc, orc->cur_group, key, &error);
    if (!ret && error)
    {
        ret = def;
        g_debug ("%s: str (%s) group (%s) in RC file (%s) not found, "
                 "using default (%s)", G_STRFUNC, key, orc->cur_group,
                 orc->file_name, ret ? "TRUE" : "FALSE");
        g_error_free (error);
    }

    return ret;
}

void orage_rc_put_bool (OrageRc *orc, const gchar *key, const gboolean val)
{
    g_key_file_set_boolean (orc->rc, orc->cur_group, key, val);
}

GDateTime *orage_rc_get_gdatetime (OrageRc *orc, const gchar *key,
                                   GDateTime *def)
{
    GError *error = NULL;
    gchar *ret;
    GDateTime *gdt;

    ret = g_key_file_get_string (orc->rc, orc->cur_group, key, &error);
    if ((ret == NULL) && error)
    {
        gdt = def ? g_date_time_ref (def) : NULL;
        g_debug ("%s: str (%s) group (%s) in RC file (%s) not found, "
                 "using default", G_STRFUNC, key, orc->cur_group,
                 orc->file_name);
        g_error_free (error);
    }
    else
        gdt = orage_icaltime_to_gdatetime (ret);

    return gdt;
}

void orage_rc_put_gdatetime (OrageRc *orc, const gchar *key, GDateTime *gdt)
{
    gchar *icaltime = orage_gdatetime_to_icaltime (gdt, FALSE);

    g_key_file_set_string (orc->rc, orc->cur_group, key, icaltime);
    g_free (icaltime);
}

gboolean orage_rc_get_color (OrageRc *orc, const gchar *key,
                             GdkRGBA *rgba, const gchar *def)
{
    gboolean result;
    gchar *color_str;

    color_str = orage_rc_get_str (orc, key, NULL);
    if (color_str)
    {
        result = orage_str_to_rgba (color_str, rgba, def);
        g_free (color_str);
    }
    else if (def)
    {
        g_warning ("unable to read colour from rc file, using default");
        result = gdk_rgba_parse (rgba, def);
    }
    else
        result = FALSE;

    return result;
}
