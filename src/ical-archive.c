/*
 * Copyright (c) 2021-2023 Erkki Moorits
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

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include <libical/ical.h>
#include <libical/icalss.h>

#include "orage-i18n.h"
#include "orage-window.h"
#include "functions.h"
#include "reminder.h"
#include "ical-code.h"
#include "ical-internal.h"
#include "event-list.h"
#include "orage-appointment-window.h"
#include "parameters.h"
#include "interface.h"

#ifdef HAVE_ARCHIVE
gboolean xfical_archive_open(void)
{
    if (!g_par.archive_limit)
        return(FALSE);
    if (!ORAGE_STR_EXISTS(g_par.archive_file))
        return(FALSE);

    return(ic_internal_file_open(&ic_aical, &ic_afical, g_par.archive_file
            , FALSE, FALSE));
}

void xfical_archive_close(void)
{
    if (!ORAGE_STR_EXISTS(g_par.archive_file))
        return;

    if (ic_afical == NULL)
        g_warning ("%s: afical is NULL", G_STRFUNC);
    icalset_free(ic_afical);
    ic_afical = NULL;
}
#endif

static icalproperty *replace_repeating(icalcomponent *c, icalproperty *p
        , icalproperty_kind k)
{
    icalproperty *s, *n;
    const char *text;
    const gint x_len = strlen("X-ORAGE-ORIG-");

    text = g_strdup(icalproperty_as_ical_string(p));
    n = icalproperty_new_from_string(text + x_len);
    g_free((gchar *)text);
    s = icalcomponent_get_first_property(c, k);
    /* remove X-ORAGE-ORIG...*/
    icalcomponent_remove_property(c, p);
    /* remove old k (=either DTSTART or DTEND) */
    icalcomponent_remove_property(c, s);
    /* add new DTSTART or DTEND */
    icalcomponent_add_property(c, n);
    /* we need to start again from the first since we messed the order,
     * but there are not so many X- propoerties that this is worth worring */
    return(icalcomponent_get_first_property(c, ICAL_X_PROPERTY));
}

#ifdef HAVE_ARCHIVE
static void xfical_icalcomponent_archive_normal(icalcomponent *e) 
{
    icalcomponent *d;

    /* Add to the archive file */
    d = icalcomponent_new_clone(e);
    icalcomponent_add_component(ic_aical, d);

    /* Remove from the main file */
    icalcomponent_remove_component(ic_ical, e);
}

static void xfical_icalcomponent_archive_recurrent (icalcomponent *e,
                                                    const gint threshold_months)
{
    struct icaltimetype sdate, edate, nsdate, nedate;
    struct icalrecurrencetype rrule;
    struct icaldurationtype duration;
    icalrecur_iterator* ri;
    gchar *stz_loc = NULL, *etz_loc = NULL;
    const char *text;
    char *text2;
    icalproperty *p, *pdtstart, *pdtend;
    icalproperty *p_orig, *p_origdtstart = NULL, *p_origdtend = NULL;
    gboolean upd_edate = FALSE;
    gboolean has_orig_dtstart = FALSE, has_orig_dtend = FALSE;

    /* We must not remove recurrent events, but just modify start- and
     * enddates and actually only the date parts since time will stay.
     * Note that we may need to remove limited recurrency events. We only
     * add X-ORAGE-ORIG... dates if those are not there already.
     */
    sdate = icalcomponent_get_dtstart(e);
    pdtstart = icalcomponent_get_first_property(e, ICAL_DTSTART_PROPERTY);
    stz_loc = ic_get_char_timezone(pdtstart);
    sdate = ic_convert_to_timezone(sdate, pdtstart);

    edate = icalcomponent_get_dtend(e);
    if (icaltime_is_null_time(edate)) {
        edate = sdate;
    }
    pdtend = icalcomponent_get_first_property(e, ICAL_DTEND_PROPERTY);
    if (pdtend) { /* we have DTEND, so we need to adjust it. */
        etz_loc = ic_get_char_timezone(pdtend);
        edate = ic_convert_to_timezone(edate, pdtend);
        duration = icaltime_subtract(edate, sdate);
        upd_edate = TRUE;
    }
    else {
        p = icalcomponent_get_first_property(e, ICAL_DURATION_PROPERTY);
        if (p) /* we have DURATION, which does not need changes */
            duration = icalproperty_get_duration(p);
        else  /* neither duration, nor dtend, assume dtend=dtstart */
            duration = icaltime_subtract(edate, sdate);
    }
    p_orig = icalcomponent_get_first_property(e, ICAL_X_PROPERTY);
    while (p_orig) {
        text = icalproperty_get_x_name(p_orig);
        if (g_str_has_prefix(text, "X-ORAGE-ORIG-DTSTART")) {
            if (has_orig_dtstart) {
                /* This fixes bug which existed prior to 4.5.9.7:
                 * It was possible that multiple entries were generated.
                 * They are in order: oldest first.
                 * And we only need the oldest, so delete the rest */
                g_warning ("%s: Corrupted X-ORAGE-ORIG-DTSTART setting. Fixing",
                           G_STRFUNC);
                icalcomponent_remove_property(e, p_orig);
                /* we need to start from scratch since counting may go wrong
                 * bcause delete moves the pointer. */
                has_orig_dtstart = FALSE;
                has_orig_dtend = FALSE;
                p_orig = icalcomponent_get_first_property(e, ICAL_X_PROPERTY);
            }
            else {
                has_orig_dtstart = TRUE;
                p_origdtstart = p_orig;
                p_orig = icalcomponent_get_next_property(e, ICAL_X_PROPERTY);
            }
        }
        else if (g_str_has_prefix(text, "X-ORAGE-ORIG-DTEND")) {
            if (has_orig_dtend) {
                g_warning ("%s: Corrupted X-ORAGE-ORIG-DTEND setting. Fixing",
                           G_STRFUNC);
                icalcomponent_remove_property(e, p_orig);
                has_orig_dtstart = FALSE;
                has_orig_dtend = FALSE;
                p_orig = icalcomponent_get_first_property(e, ICAL_X_PROPERTY);
            }
            else {
                has_orig_dtend = TRUE;
                p_origdtend = p_orig;
                p_orig = icalcomponent_get_next_property(e, ICAL_X_PROPERTY);
            }
        }
        else /* it was not our X-PROPERTY */
            p_orig = icalcomponent_get_next_property(e, ICAL_X_PROPERTY);
    }

    p = icalcomponent_get_first_property(e, ICAL_RRULE_PROPERTY);
    rrule = icalproperty_get_rrule(p);
    ri = icalrecur_iterator_new(rrule, sdate);

    /* We must do a loop for finding the first occurence after the threshold */
    for (nsdate = icalrecur_iterator_next(ri),
            nedate = icaltime_add(nsdate, duration);
         !icaltime_is_null_time(nsdate)
         && (nedate.year*12 + nedate.month) < threshold_months;
         nsdate = icalrecur_iterator_next(ri),
            nedate = icaltime_add(nsdate, duration)){
    }
    icalrecur_iterator_free(ri);

    if (icaltime_is_null_time(nsdate)) { /* remove since it has ended */
        g_message ("Recur ended, moving to archive file");
        if (has_orig_dtstart)
            replace_repeating(e, p_origdtstart, ICAL_DTSTART_PROPERTY);
        if (has_orig_dtend)
            replace_repeating(e, p_origdtend, ICAL_DTEND_PROPERTY);
        xfical_icalcomponent_archive_normal(e);
    }
    else { /* modify times*/
        if (!has_orig_dtstart) {
            text = g_strdup(icalproperty_as_ical_string(pdtstart));
            text2 = g_strjoin(NULL, "X-ORAGE-ORIG-", text, NULL);
            p = icalproperty_new_from_string(text2);
            g_free((gchar *)text2);
            g_free((gchar *)text);
            icalcomponent_add_property(e, p);
        }
        icalcomponent_remove_property(e, pdtstart);
        if (stz_loc == NULL)
            icalcomponent_add_property(e, icalproperty_new_dtstart(nsdate));
        else
            icalcomponent_add_property(e
                    , icalproperty_vanew_dtstart(nsdate
                            , icalparameter_new_tzid(stz_loc)
                            , 0));
        if (upd_edate) {
            if (!has_orig_dtend) {
                text = g_strdup(icalproperty_as_ical_string(pdtend));
                text2 = g_strjoin(NULL, "X-ORAGE-ORIG-", text, NULL);
                p = icalproperty_new_from_string(text2);
                g_free((gchar *)text2);
                g_free((gchar *)text);
                icalcomponent_add_property(e, p);
            }
            icalcomponent_remove_property(e, pdtend);
            if (etz_loc == NULL)
                icalcomponent_add_property(e, icalproperty_new_dtend(nedate));
            else
                icalcomponent_add_property(e
                        , icalproperty_vanew_dtend(nedate
                                , icalparameter_new_tzid(etz_loc)
                                , 0));
        }
    }
}

gboolean xfical_archive(void)
{
    GDateTime *gdt;
    GDateTime *threshold;
    xfical_period per;
    icalcomponent *c, *c2;
    icalproperty *p;
    const char *uid;
    gint year;
    gint month;
    gint day;
    gint threshold_months;
    gint ical_months;

    if (g_par.archive_limit == 0) {
        g_message ("Archiving not enabled. Exiting");
        return(TRUE);
    }
    if (!xfical_file_open(FALSE) || !xfical_archive_open()) {
        g_critical ("%s: file open error", G_STRFUNC);
        return(FALSE);
    }

    gdt = g_date_time_new_now_local ();
    threshold = g_date_time_add_months (gdt, -g_par.archive_limit);
    g_date_time_unref (gdt);
    g_date_time_get_ymd (threshold, &year, &month, &day);

    g_message ("Archiving threshold: %d month(s)", g_par.archive_limit);
    /* yy mon day */
    g_message ("Archiving events, which are older than: %04d-%02d-%02d",
               year, month, 1);

    /* Check appointment file for items older than the threshold */
    /* Note: remove moves the "c" pointer to next item, so we need to store it
     *       first to process all of them or we end up skipping entries */
    threshold_months = year * 12 + month;
    for (c = icalcomponent_get_first_component(ic_ical, ICAL_ANY_COMPONENT);
         c != 0;
         c = c2) {
        c2 = icalcomponent_get_next_component(ic_ical, ICAL_ANY_COMPONENT);
#if 0
        sdate = icalcomponent_get_dtstart(c);
        edate = icalcomponent_get_dtend(c);
        if (icaltime_is_null_time(edate)) {
            edate = sdate;
        }
#endif
        per =  ic_get_period(c, TRUE);
        /* Items with endate before threshold => archived.
         * Recurring events are marked in the main file by adding special
         * X-ORAGE_ORIG-DTSTART/X-ORAGE_ORIG-DTEND to save the original
         * start/end dates. Then start_date is changed. These are NOT
         * written in archive file (unless of course they really have ended).
         */
        ical_months = per.etime.year * 12 + per.etime.month;
        if (ical_months < threshold_months)
        {
            uid = icalcomponent_get_uid(c);
            g_message ("Archiving uid: %s", uid);
            /* FIXME: check VTODO completed before archiving it */
            if (per.ikind == ICAL_VTODO_COMPONENT
                && ((per.ctime.year*12 + per.ctime.month)
                    < (per.stime.year*12 + per.stime.month))) {
                /* VTODO not completed, do not archive */
                g_message ("VTODO not complete; not archived");
            }
            else {
                p = icalcomponent_get_first_property(c, ICAL_RRULE_PROPERTY);
                if (p) {  /*  it is recurrent event */
                    g_message ("Recurring. End year: %04d, "
                               "month: %02d, day: %02d", per.etime.year,
                               per.etime.month, per.etime.day);
                    xfical_icalcomponent_archive_recurrent (c, threshold_months);
                }
                else
                    xfical_icalcomponent_archive_normal(c);
            }
        }
    }

    g_date_time_unref (threshold);
    ic_file_modified = TRUE;
    icalset_mark(ic_afical);
    icalset_commit(ic_afical);
    xfical_archive_close();
    icalset_mark(ic_fical);
    icalset_commit(ic_fical);
    xfical_file_close(FALSE);
    g_message ("Archiving done");
    return(TRUE);
}

gboolean xfical_unarchive(void)
{
    icalcomponent *c, *d;
    icalproperty *p;
    const char *text;

    /* PHASE 1: go through base orage file and remove "repeat" shortcuts */
    g_message ("Starting archive removal.");
    g_message ("PHASE 1: reset recurring appointments");
    if (!xfical_file_open(FALSE)) {
        g_critical ("%s: file open error", G_STRFUNC);
        return(FALSE);
    }

    for (c = icalcomponent_get_first_component(ic_ical, ICAL_VEVENT_COMPONENT);
         c != 0;
         c = icalcomponent_get_next_component(ic_ical, ICAL_VEVENT_COMPONENT)) {
         p = icalcomponent_get_first_property(c, ICAL_X_PROPERTY);
         while (p) {
            text = icalproperty_get_x_name(p);
            if (g_str_has_prefix(text, "X-ORAGE-ORIG-DTSTART"))
                p = replace_repeating(c, p, ICAL_DTSTART_PROPERTY);
            else if (g_str_has_prefix(text, "X-ORAGE-ORIG-DTEND"))
                p = replace_repeating(c, p, ICAL_DTEND_PROPERTY);
            else /* it was not our X-PROPERTY */
                p = icalcomponent_get_next_property(c, ICAL_X_PROPERTY);
        }
    }
    /* PHASE 2: go through archive file and add everything back to base orage.
     * After that delete the whole arch file */
    g_message ("PHASE 2: return archived appointments");
    if (!xfical_archive_open()) {
        /* we have risk to delete the data permanently, let's stop here */
        g_error ("%s: archive file open error", G_STRFUNC);
        /*
        icalset_mark(ic_fical);
        icalset_commit(ic_fical);
        */
        xfical_file_close(FALSE);
        return(FALSE);
    }
    for (c = icalcomponent_get_first_component(ic_aical, ICAL_ANY_COMPONENT);
         c != 0;
         c = icalcomponent_get_next_component(ic_aical, ICAL_ANY_COMPONENT)) {
    /* Add to the base file */
        d = icalcomponent_new_clone(c);
        icalcomponent_add_component(ic_ical, d);
    }
    xfical_archive_close();
    if (g_remove(g_par.archive_file) == -1) {
        g_warning ("%s: Failed to remove archive file %s",
                   G_STRFUNC, g_par.archive_file);
    }
    ic_file_modified = TRUE;
    icalset_mark(ic_fical);
    icalset_commit(ic_fical);
    xfical_file_close(FALSE);
    g_message ("Archive removal done");
    return(TRUE);
}

gboolean xfical_unarchive_uid (const gchar *uid)
{
    icalcomponent *c, *d;
    gboolean key_found = FALSE;
    const char *text;
    const gchar *ical_uid;

    ical_uid = uid+4; /* skip file id (which is A00. now)*/
    if (!xfical_file_open(FALSE) || !xfical_archive_open()) {
        g_critical ("%s: file open error", G_STRFUNC);
        return(FALSE);
    } 
    for (c = icalcomponent_get_first_component(ic_aical, ICAL_ANY_COMPONENT);
         c != 0 && !key_found;
         c = icalcomponent_get_next_component(ic_aical, ICAL_ANY_COMPONENT)) {
        text = icalcomponent_get_uid(c);
        if (strcmp(text, ical_uid) == 0) { /* we found our uid (=event) */
            /* Add to the base file */
            d = icalcomponent_new_clone(c);
            icalcomponent_add_component(ic_ical, d);
            /* Remove from the archive file */
            icalcomponent_remove_component(ic_aical, c);
            key_found = TRUE;
            ic_file_modified = TRUE;
        }
    }
    icalset_mark(ic_afical);
    icalset_commit(ic_afical);
    xfical_archive_close();
    icalset_mark(ic_fical);
    icalset_commit(ic_fical);
    xfical_file_close(FALSE);

    return(TRUE);
}
#endif
