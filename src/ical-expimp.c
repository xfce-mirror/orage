/*
 * Copyright (c) 2021-2025 Erkki Moorits
 * Copyright (c) 2005-2011 Juha Kautto  (juha at xfce.org)
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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <unistd.h>
#include <time.h>
#include <math.h>

#include <gdk/gdk.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include <libical/ical.h>
#include <libical/icalss.h>

#include "event-list.h"
#include "functions.h"
#include "ical-code.h"
#include "ical-expimp.h"
#include "ical-internal.h"
#include "interface.h"
#include "orage-i18n.h"
#include "parameters.h"
#include "reminder.h"

static gboolean export_selected (const gchar *file_name, const gchar *uids);
static gboolean export_all (const gchar *file_name);

static gchar *find_calendar_id (const gchar *calendar_name,
                                gboolean *ical_opened)
{
    gchar *calendar_id;
    gboolean found;
    gint i;

    if (xfce_str_is_empty (calendar_name))
        calendar_id = g_strdup ("O00.");
    else
    {
        calendar_id = NULL;
        if (xfical_file_open (TRUE))
        {
            *ical_opened = TRUE;
            found = FALSE;
            for (i = 0; (i < g_par.foreign_count) && (found == FALSE); i++)
            {
                if (strcmp (g_par.foreign_data[i].file, calendar_name) == 0 ||
                    strcmp (g_par.foreign_data[i].name, calendar_name) == 0)
                {
                    found = TRUE;
                }
            }

            if (found)
                calendar_id = g_strdup_printf ("F%02d.", i - 1);
            else
            {
                g_warning ("%s: matching foreign file not found: '%s'",
                           G_STRFUNC, calendar_name);
            }
        }
        else
            g_warning ("%s: calendar file open failed", G_STRFUNC);
    }

    return calendar_id;
}

static void add_event (icalcomponent *c, gboolean *ical_opened)
{
    icalcomponent *ca;
    char *uid;

    ca = icalcomponent_new_clone(c);
    if (icalcomponent_get_uid(ca) == NULL) {
        uid = ic_generate_uid();
        icalcomponent_add_property(ca,  icalproperty_new_uid(uid));
        g_message ("Generated UID %s", uid);
        g_free(uid);
    }

    if (*ical_opened == FALSE)
    {
        if (xfical_file_open (FALSE) == FALSE)
        {
            g_critical ("%s: ical file open failed", G_STRFUNC);
            return;
        }

        *ical_opened = TRUE;
    }

    icalcomponent_add_component(ic_ical, ca);
    ic_file_modified = TRUE;
    icalset_mark(ic_fical);
    icalset_commit(ic_fical);
}

static void add_event_with_id (icalcomponent *c, gboolean *ical_opened,
                               const gchar *id)
{
    icalcomponent *ca;
    gchar *tmp;
    gchar *uid;
    gint i;

    ca = icalcomponent_new_clone (c);
    if (icalcomponent_get_uid (ca) == NULL)
    {
        tmp = ic_generate_uid ();
        uid = g_strconcat (id, tmp, NULL);
        g_free (tmp);
        icalcomponent_add_property (ca,  icalproperty_new_uid (uid));
        g_message ("generated UID '%s'", uid);
        g_free (uid);
    }

    if (*ical_opened == FALSE)
    {
        if (xfical_file_open (FALSE) == FALSE)
        {
            g_critical ("%s: ical file open failed", G_STRFUNC);
            return;
        }

        *ical_opened = TRUE;
    }

    if (id[0] == 'O')
    {
        icalcomponent_add_component (ic_ical, ca);
        icalset_mark (ic_fical);
    }
    else if (id[0] == 'F')
    {
        sscanf (id, "F%02d", &i);
        if (i < g_par.foreign_count && ic_f_ical[i].ical != NULL)
        {
            icalcomponent_add_component (ic_f_ical[i].ical, ca);
            icalset_mark (ic_f_ical[i].fical);
        }
        else
        {
            g_critical ("%s: unknown foreign file number %s", G_STRFUNC, id);
            return;
        }
    }
    else
    {
        /* Note: we never update/add Archive file */
        g_critical ("%s: unknown file type '%s'", G_STRFUNC, id);
        return;
    }

    ic_file_modified = TRUE;
    icalset_commit (ic_fical);
}

/* pre process the file to rule out some features, which orage does not
 * support so that we can do better conversion 
 */
static gboolean pre_format(const gchar *file_name_in,
                           const gchar *file_name_out)
{
    gchar *text, *tmp, *tmp2, *tmp3;
    gsize text_len;
    GError *error = NULL;

    g_debug ("starting import file preprocessing");

    if (g_file_get_contents (file_name_in, &text, &text_len, &error) == FALSE)
    {
        g_critical ("%s: could not open ical file (%s) error: %s", G_STRFUNC,
                    file_name_in, error->message);
        g_error_free (error);
        return FALSE;
    }

    /***** Check utf8 conformability *****/
    if (!g_utf8_validate(text, -1, NULL)) {
        g_critical ("%s: is not in utf8 format. Conversion needed. "
                    "(Use iconv and convert it into UTF-8 and import it "
                    "again.)", G_STRFUNC);
        return FALSE;
    }

    /***** 1: change DCREATED to CREATED *****/
    for (tmp = g_strstr_len(text, text_len, "DCREATED:"); 
         tmp != NULL;
         tmp = g_strstr_len(tmp, strlen(tmp), "DCREATED:")) {
        tmp2 = tmp+strlen("DCREATED:yyyymmddThhmmss");
        if (*tmp2 == 'Z') {
            /* it is already in UTC, so just fix the parameter name */
            *tmp = ' ';
        }
        else {
            /* needs to be converted to UTC also */
            for (tmp3 = tmp; tmp3 < tmp2; tmp3++) {
                *tmp3 = *(tmp3+1); 
            }
            *(tmp3-1) = 'Z'; /* this is 'bad'...but who cares...it is fast */
        }
        g_message ("Patched DCREATED to be CREATED");
    }

    /***** 2: change absolute timezones into libical format *****/
    /* At least evolution uses absolute timezones.
     * We assume format has /xxx/xxx/timezone and we should remove the
     * extra /xxx/xxx/ from it */
    for (tmp = g_strstr_len(text, text_len, ";TZID=/"); 
         tmp != NULL;
         tmp = g_strstr_len(tmp, strlen(tmp), ";TZID=/")) {
        /* tmp = original TZID start
         * tmp2 = end of line 
         * tmp3 = current search and eventually the real tzid */
        tmp = tmp+6; /* 6 = skip ";TZID=" */
        if (!(tmp2 = g_strstr_len(tmp, 100, "\n"))) { /* no end of line */
            g_warning ("%s: timezone patch failed 1. no end-of-line found: %s",
                       G_STRFUNC, tmp);
            continue;
        }
        tmp3 = tmp;

        tmp3++; /* skip '/' */
        if (!(tmp3 = g_strstr_len(tmp3, tmp2-tmp3, "/"))) { /* no more '/' */
            g_warning ("%s: timezone patch failed 2. no / found: %s", G_STRFUNC,
                       tmp);
            continue;
        }
        tmp3++; /* skip '/' */
        if (!(tmp3 = g_strstr_len(tmp3, tmp2-tmp3, "/"))) { /* no more '/' */
            g_warning ("%s: timezone patch failed 3. no / found: %s", G_STRFUNC,
                       tmp);
            continue;
        }

        /* we found the real tzid since we came here */
        tmp3++; /* skip '/' */
        /* move the real tzid (=tmp3) to the beginning (=tmp) */
        for (; tmp3 < tmp2; tmp3++, tmp++)
            *tmp = *tmp3;
        /* fill the end of the line with spaces */
        for (; tmp < tmp2; tmp++)
            *tmp = ' ';
        g_message ("Patched timezone to Orage format");
    }

    /***** All done: write file *****/
    if (!g_file_set_contents(file_name_out, text, -1, NULL)) {
        g_critical ("%s: Could not write ical file (%s)",
                    G_STRFUNC, file_name_out);
        return(FALSE);
    }
    g_free(text);
    g_message ("Import file preprocessing done");
    return(TRUE);
}

/* pre process the file to rule out some features, which orage does not
 * support so that we can do better conversion
 */
static gboolean pre_format_gfile_to_string (GFile *file_in,
                                            GString **out,
                                            GError  **error)
{
    gchar *text, *tmp, *tmp2, *tmp3;
    gsize text_len;

    g_debug ("starting import file preprocessing");

    if (g_file_load_contents (file_in, NULL, &text, &text_len, NULL, error) == FALSE)
        return FALSE;

    if (g_utf8_validate (text, -1, NULL) == FALSE)
    {
        g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                     "%s: input file is not valid UTF-8", G_STRFUNC);
        g_free (text);
        return FALSE;
    }

    /***** 1: change DCREATED to CREATED *****/
    for (tmp = g_strstr_len (text, text_len, "DCREATED:");
         tmp != NULL;
         tmp = g_strstr_len (tmp, strlen (tmp), "DCREATED:"))
    {
        tmp2 = tmp + strlen ("DCREATED:yyyymmddThhmmss");
        if (*tmp2 == 'Z')
        {
            /* it is already in UTC, so just fix the parameter name */
            *tmp = ' ';
        }
        else
        {
            /* needs to be converted to UTC also */
            for (tmp3 = tmp; tmp3 < tmp2; tmp3++)
                *tmp3 = *(tmp3 + 1);

            *(tmp3 - 1) = 'Z'; /* this is 'bad'...but who cares...it is fast */
        }

        g_message ("patched DCREATED to be CREATED");
    }

    /***** 2: change absolute timezones into libical format *****/
    /* At least evolution uses absolute timezones.
     * We assume format has /xxx/xxx/timezone and we should remove the
     * extra /xxx/xxx/ from it */
    for (tmp = g_strstr_len (text, text_len, ";TZID=/");
         tmp != NULL;
         tmp = g_strstr_len (tmp, strlen (tmp), ";TZID=/"))
    {
        /* tmp = original TZID start
         * tmp2 = end of line
         * tmp3 = current search and eventually the real tzid */
        tmp = tmp + 6; /* 6 = skip ";TZID=" */
        if (!(tmp2 = g_strstr_len (tmp, 100, "\n")))
        { /* no end of line */
            g_warning ("%s: timezone patch failed 1. no end-of-line found: %s",
                       G_STRFUNC, tmp);
            continue;
        }
        tmp3 = tmp;

        tmp3++; /* skip '/' */
        if (!(tmp3 = g_strstr_len (tmp3, tmp2 - tmp3, "/")))
        {
            g_warning ("%s: timezone patch failed 2. no / found: %s", G_STRFUNC,
                       tmp);
            continue;
        }
        tmp3++; /* skip '/' */
        if (!(tmp3 = g_strstr_len (tmp3, tmp2 - tmp3, "/")))
        {
            g_warning ("%s: timezone patch failed 3. no / found: %s", G_STRFUNC,
                       tmp);
            continue;
        }

        tmp3++;
        for (; tmp3 < tmp2; tmp3++, tmp++)
            *tmp = *tmp3;

        for (; tmp < tmp2; tmp++)
            *tmp = ' ';

        g_message ("patched timezone to Orage format");
    }

    /***** All done: write file *****/
    *out = g_string_new (text);
    g_free (text);
    g_message ("import file preprocessing done");

    return TRUE;
}

static gboolean xfical_import_component (icalcomponent *root,
                                         gboolean *ical_opened,
                                         const gchar *id)
{
    icalcomponent *comp;

    for (comp = icalcomponent_get_first_component (root, ICAL_ANY_COMPONENT);
         comp != NULL;
         comp = icalcomponent_get_next_component (root, ICAL_ANY_COMPONENT))
    {
        switch (icalcomponent_isa (comp))
        {
            case ICAL_VEVENT_COMPONENT:
            case ICAL_VTODO_COMPONENT:
            case ICAL_VJOURNAL_COMPONENT:
                add_event_with_id (comp, ical_opened, id);
                return TRUE;

            case ICAL_VTIMEZONE_COMPONENT:
                /* We ignore TIMEZONE component; Orage only uses internal
                 * timezones from libical.
                 */
                return FALSE;

            default:
                g_warning ("%s: unknown component %s", G_STRFUNC,
                           icalcomponent_kind_to_string (icalcomponent_isa (comp)));
                return FALSE;
        }
    }

    return FALSE;
}

gboolean xfical_import_by_path (const gchar *file_name)
{
    icalset *file_ical;
    gchar *ical_file_name;
    icalcomponent *c1, *c2;
    gint cnt1 = 0, cnt2 = 0;
    gboolean ical_opened = FALSE;

    ical_file_name = g_strdup_printf("%s.orage", file_name);
    if (!pre_format(file_name, ical_file_name)) {
        g_free(ical_file_name);
        return(FALSE);
    }
    if ((file_ical = icalset_new_file(ical_file_name)) == NULL) {
        g_critical ("%s: Could not open ical file (%s) %s", G_STRFUNC,
                    ical_file_name, icalerror_strerror(icalerrno));
        g_free(ical_file_name);
        return(FALSE);
    }
    for (c1 = icalset_get_first_component(file_ical);
         c1 != 0;
         c1 = icalset_get_next_component(file_ical)) {
        if (icalcomponent_isa(c1) == ICAL_VCALENDAR_COMPONENT) {
            cnt1++;
            for (c2=icalcomponent_get_first_component(c1, ICAL_ANY_COMPONENT);
                 c2 != 0;
                 c2=icalcomponent_get_next_component(c1, ICAL_ANY_COMPONENT)){
                if ((icalcomponent_isa(c2) == ICAL_VEVENT_COMPONENT)
                ||  (icalcomponent_isa(c2) == ICAL_VTODO_COMPONENT)
                ||  (icalcomponent_isa(c2) == ICAL_VJOURNAL_COMPONENT)) {
                    cnt2++;
                    add_event (c2, &ical_opened);
                }
                /* we ignore TIMEZONE component; Orage only uses internal
                 * timezones from libical */
                else if (icalcomponent_isa(c2) != ICAL_VTIMEZONE_COMPONENT)
                    g_warning ("%s: unknown component %s %s", G_STRFUNC
                            , icalcomponent_kind_to_string(
                                    icalcomponent_isa(c2))
                            , ical_file_name);
            }

        }
        else
            g_warning ("%s: unknown icalset component %s in %s", G_STRFUNC
                    , icalcomponent_kind_to_string(icalcomponent_isa(c1))
                    , ical_file_name);
    }

    if (ical_opened)
        xfical_file_close (FALSE);

    if (cnt1 == 0) {
        g_warning ("%s: No valid icalset components found", G_STRFUNC);
        g_free(ical_file_name);
        return(FALSE);
    }
    if (cnt2 == 0) {
        g_warning ("%s: No valid ical components found", G_STRFUNC);
        g_free(ical_file_name);
        return(FALSE);
    }

    g_free(ical_file_name);
    return(TRUE);
}

gboolean orage_calendar_import_file (GFile *file, const gchar *dest)
{
    gchar *calendar_id = NULL;
    icalcomponent *vcal;
    icalcomponent *root;
    gchar *file_name;
    gboolean vcalendar_found;
    gboolean imported;
    gboolean result = TRUE;
    gboolean ical_opened = FALSE;
    GString *ical_text = NULL;
    GError *error = NULL;

    if (pre_format_gfile_to_string (file, &ical_text, &error) == FALSE)
    {
        file_name = g_file_get_path (file);
        g_critical ("%s: could not open ical file (%s): %s", G_STRFUNC,
                    file_name, error ? error->message : "unknown error");
        g_free (file_name);
        g_error_free (error);
        result = FALSE;
    }

    if (result)
    {
        root = icalparser_parse_string (ical_text->str);
        g_string_free (ical_text, TRUE);
        if (root == NULL)
        {
            g_critical ("%s: failed to parse iCal data: %s", G_STRFUNC,
                        icalerror_strerror (icalerrno));
            result = FALSE;
        }
    }

    if (result)
    {
        calendar_id = find_calendar_id (dest, &ical_opened);
        if (calendar_id == NULL)
        {
            g_warning ("%s: matching calendar file not found: '%s'",
                       G_STRFUNC, dest);
            result = FALSE;
        }
    }

    if (result)
    {
        vcalendar_found = FALSE;
        imported = FALSE;

        if (icalcomponent_isa (root) == ICAL_VCALENDAR_COMPONENT)
        {
            vcalendar_found = TRUE;
            imported = xfical_import_component (root, &ical_opened, calendar_id);
        }
        else
        {
            g_warning ("%s: multiple VCALENDAR components found, importing all",
                       G_STRFUNC);

            for (vcal = icalcomponent_get_first_component (root, ICAL_VCALENDAR_COMPONENT);
                 vcal != NULL;
                 vcal = icalcomponent_get_next_component (root, ICAL_VCALENDAR_COMPONENT))
            {
                vcalendar_found = TRUE;
                imported |= xfical_import_component (root, &ical_opened, calendar_id);
            }
        }

        icalcomponent_free (root);

        if (vcalendar_found == FALSE)
        {
            g_warning ("%s: no valid ICAL_VCALENDAR_COMPONENT components found",
                       G_STRFUNC);
            result = FALSE;
        }

        if (result)
        {
            if (imported == FALSE)
            {
                g_warning ("%s: no valid ical components found", G_STRFUNC);
                result = FALSE;
            }
        }
    }

    if (calendar_id)
        g_free (calendar_id);

    if (ical_opened)
        xfical_file_close (TRUE);

    return result;
}

gboolean xfical_export_file (GFile *file, const gchar *uids)
{
    gboolean result;
    gchar *name;

    name = g_file_get_path (file);

    if (uids)
    {
        /* Copy only selected appointments. */
        result = export_selected (name, uids);
    }
    else
    {
        /* Copy the whole file */
        result = export_all (name);
    }

    g_free (name);

    return result;
}

static gboolean export_prepare_write_file(const gchar *file_name)
{
    gchar *tmp;

    if (strcmp(file_name, g_par.orage_file) == 0) {
        g_warning ("%s: You do not want to overwrite Orage ical file! (%s)"
                , G_STRFUNC, file_name);
        return(FALSE);
    }
    tmp = g_path_get_dirname(file_name);
    if (g_mkdir_with_parents(tmp, 0755)) { /* octal */
        g_critical ("%s: Could not create directories (%s)",
                    G_STRFUNC, file_name);
        return(FALSE);
    }
    g_free(tmp);
    if (g_file_test(file_name, G_FILE_TEST_EXISTS)) {
	if (g_remove(file_name) == -1) {
            g_warning ("%s: Failed to remove export file %s",
                       G_STRFUNC, file_name);
        }
    }
    return(TRUE);
}

static gboolean export_all (const gchar *file_name)
{
    gchar *text;
    gsize text_len;

    if (!export_prepare_write_file(file_name))
        return(FALSE);
    /* read Orage's ical file */
    if (!g_file_get_contents(g_par.orage_file, &text, &text_len, NULL)) {
        g_critical ("%s: Could not open Orage ical file (%s)", G_STRFUNC
                , g_par.orage_file);
        return(FALSE);
    }
    if (!g_file_set_contents(file_name, text, -1, NULL)) {
        g_warning ("%s: Could not write file (%s)", G_STRFUNC
                , file_name);
        g_free(text);
        return(FALSE);
    }
    g_free(text);
    return(TRUE);
}

static gboolean export_selected_uid (icalcomponent *base, const gchar *uid,
                                     icalcomponent *x_ical)
{
    icalcomponent *c, *d;
    gboolean key_found = FALSE;
    const gchar *uid_ical;

    for (c = icalcomponent_get_first_component (base, ICAL_ANY_COMPONENT);
         c != 0 && !key_found;
         c = icalcomponent_get_next_component (base, ICAL_ANY_COMPONENT))
    {
        uid_ical = icalcomponent_get_uid (c);
        if (uid_ical == NULL)
            g_warning ("%s: component missing UID, skipping", G_STRFUNC);
        else if (strcmp (uid, uid_ical) == 0)
        {
            d = icalcomponent_new_clone (c);
            icalcomponent_add_component (x_ical, d);
            key_found = TRUE;
        }
    }

    if (key_found == FALSE)
        g_warning ("%s: not found '%s' from Orage", G_STRFUNC, uid);

    return key_found;
}

static gboolean export_selected (const gchar *file_name, const gchar *uids)
{
    icalcomponent *x_ical = NULL;
    icalset *x_fical = NULL;
    gchar *uid_end;
    const gchar *uid;
    gboolean more_uids;
    gboolean result;
    gint i;

    if (export_prepare_write_file (file_name) == FALSE)
        return FALSE;

    if (xfce_str_is_empty (uids))
    {
        g_warning ("%s: UID list is empty", G_STRFUNC);
        return FALSE;
    }

    if (ic_internal_file_open (&x_ical, &x_fical, file_name, FALSE, FALSE) == FALSE)
    {
        g_warning ("%s: failed to create export file %s", G_STRFUNC, file_name);
        return FALSE;
    }

    if (xfical_file_open (TRUE) == FALSE)
        return FALSE;

    /* checks done, let's start the real work */
    more_uids = TRUE;
    result = TRUE;
    for (uid = uids; more_uids;)
    {
        if (strlen (uid) < 5)
        {
            g_warning ("%s: unknown appointment name %s", G_STRFUNC, uid);
            return FALSE;
        }

        uid_end = g_strstr_len (uid, strlen (uid), ",");
        if (uid_end != NULL)
            *uid_end = 0; /* uid ends here */
        /* FIXME: proper messages to screen */
        if (uid[0] == 'O')
            result &= export_selected_uid (ic_ical, uid, x_ical);
        else if (uid[0] == 'F')
        {
            sscanf (uid, "F%02d", &i);
            if (i < g_par.foreign_count && ic_f_ical[i].ical != NULL)
                result &= export_selected_uid (ic_f_ical[i].ical, uid, x_ical);
            else
            {
                g_warning ("%s: unknown foreign file number %d, %s", G_STRFUNC,
                           i, uid);
                return FALSE;
            }
        }
        else
            g_warning ("%s: unknown uid type (%s)", G_STRFUNC, uid);

        if (uid_end != NULL) /* we have more uids */
            uid = uid_end + 1; /* next uid starts here */
        else
            more_uids = FALSE;
    }

    xfical_file_close (TRUE);
    icalset_mark (x_fical);
    icalset_commit (x_fical);
    icalset_free (x_fical);
    return result;
}

gboolean xfical_export_by_path (const gchar *file_name,
                                const gint type,
                                const gchar *uids)
{
    if (type == 0) { /* copy the whole file */
        return(export_all(file_name));
    }
    else if (type == 1) { /* copy only selected appointments */
        return(export_selected(file_name, uids));
    }
    else {
        g_critical ("%s: unknown app count %d", G_STRFUNC, type);
        return(FALSE);
    }
}
