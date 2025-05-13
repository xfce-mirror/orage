/*
 * Copyright (c) 2021-2024 Erkki Moorits
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
    /* errno */

#include <stdlib.h>
    /* malloc, atoi, free, setenv */

#include <stdio.h>
#include <glib/gstdio.h>
    /* printf, fopen, fread, fclose, perror, rename */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
    /* stat, mkdir */

#include <time.h>
    /* localtime, gmtime, asctime */

#include <string.h>
    /* strncmp, strcmp, strlen, strncat, strncpy, strdup, strstr */

#include <glib.h>
#include "tz_zoneinfo_read.h"

/* This define is needed to get nftw instead if ftw.
 * Documentation says the define is _XOPEN_SOURCE, but it
 * does not work. __USE_XOPEN_EXTENDED works 
 * Same with _GNU_SOURCE and __USE_GNU */
/* #define _XOPEN_SOURCE 500 */
#define __USE_XOPEN_EXTENDED 1
#define _GNU_SOURCE 1
#define __USE_GNU 1
#include <ftw.h>
    /* nftw */
#ifndef FTW_ACTIONRETVAL
/* BSD systems lack FTW_ACTIONRETVAL, so we need to define needed
 * things like FTW_CONTINUE locally */
#define FTW_CONTINUE 0
#endif

#define DEFAULT_OS_ZONEINFO_DIRECTORY  "/usr/share/zoneinfo"
#define ZONETAB_FILE        "zone.tab"
#define COUNTRY_FILE        "iso3166.tab"


/** This is the toplevel directory where the timezone data is installed in. */
#define ORAGE_ZONEINFO_DIRECTORY PACKAGE_DATADIR "/orage/zoneinfo/"

/** This is the filename of the file containing tz_convert parameters
 * This file contains the location of the os zoneinfo data.
 * the same than the above DEFAULT_OS_ZONEINFO_DIRECTORY */
#define TZ_CONVERT_PAR_FILENAME  "tz_convert.par"
#define TZ_CONVERT_PAR_FILE_LOC  ORAGE_ZONEINFO_DIRECTORY TZ_CONVERT_PAR_FILENAME

/* this contains all timezone data */
orage_timezone_array tz_array= {0};

static char *zone_tab_buf = NULL, *country_buf = NULL, *zones_tab_buf = NULL;

/** Number of processed files. */
static guint number_of_proccessed_files = 0;

static unsigned char *in_buf, *in_head, *in_tail;
static int in_file_base_offset = 0;

static int details;     /* show extra data (country and next change time) */
static int check_ical;  /* check that we have also the ical timezone data */

static char *in_file = NULL, *out_file = NULL;
static gint excl_dir_cnt = -1;
static char **excl_dir = NULL;

/* in_timezone_name is the real timezone name from the infile 
 * we are processing.
 * timezone_name is the timezone we are writing. Usually it is the same
 * than in_timezone_name. 
 * timezone_name is for example Europe/Helsinki */
/* FIXME: we do not need both timezone_name and in_timezone_name here.
 * Remove one */
static char *timezone_name = NULL;  
static char *in_timezone_name = NULL;

/* Ignore rules which are older or equal than this */
static int ignore_older = 1970; 

/* time change table starts here */
static unsigned char *begin_timechanges;           

/* time change type index table starts here */
static unsigned char *begin_timechangetypeindexes; 

/* time change type table starts here */
static unsigned char *begin_timechangetypes;       

/* timezone name table */
static unsigned char *begin_timezonenames;         

static unsigned long gmtcnt;
static unsigned long stdcnt;
static unsigned long leapcnt;
static unsigned long timecnt;  /* points when time changes */
static unsigned long typecnt;  /* table of different time changes = types */
static unsigned long charcnt;  /* length of timezone name table */

/* silence -Wformat-y2k warning when using %c or %x (see man strftime.3) */
static inline size_t orage_strftime (char *s, size_t max,
                                     const char *fmt, const struct tm *tm)
{
    return strftime (s, max, fmt, tm);
}

static gchar *get_zoneinfo_directory (void)
{
    const gchar *env_tzdir;
    gchar *tzdir;

    env_tzdir = g_getenv ("TZDIR");
    tzdir = g_strdup (env_tzdir ? env_tzdir : DEFAULT_OS_ZONEINFO_DIRECTORY);

    g_debug ("tzdir: '%s'", tzdir);

    return tzdir;
}

static void read_file(const char *file_name, const struct stat *file_stat)
{
    FILE *file;

    g_debug ("size of file %s is %d bytes", file_name, (int)file_stat->st_size);

    in_buf = g_new0(unsigned char, file_stat->st_size);
    in_head = in_buf;
    in_tail = in_buf + file_stat->st_size - 1;
    file = g_fopen (file_name, "r");
    if (!file) {
        g_warning ("file open error (%s): %s", file_name, g_strerror (errno));
        return;
    }
    if ((off_t)fread(in_buf, 1, file_stat->st_size, file) < file_stat->st_size)
        if (ferror(file)) {
            g_warning ("file read failed (%s): %s", file_name,
                       g_strerror (errno));
            fclose(file);
            return;
        }
    fclose(file);
    number_of_proccessed_files++;
}

static int get_long(void)
{
    int tmp;

    tmp = (((int)in_head[0]<<24)
         + ((int)in_head[1]<<16)
         + ((int)in_head[2]<<8)
         +  (int)in_head[3]);
    in_head += 4;
    return(tmp);
}

static int process_header(void)
{
    if (strncmp((char *)in_head, "TZif", 4)) { /* we accept version 1 and 2 */
        return(1);
    }
    /* header */
    in_head += 4; /* type */
    in_head += 16; /* reserved */

    gmtcnt  = get_long();
    stdcnt  = get_long();
    leapcnt = get_long();
    timecnt = get_long();
    typecnt = get_long();
    charcnt = get_long();

    return(0);
}

static void process_local_time_table(void)
{
    /* points when time changes */

    begin_timechanges = in_head;

    /* Skip over table entries. */
    in_head += (4 * timecnt);
}

static void process_local_time_type_table(void)
{
    /* pointers to table, which explain how time changes */

    begin_timechangetypeindexes = in_head;

    /* Skip over table entries. */
    in_head += timecnt;
}

static void process_ttinfo_table(void)
{
    /* table of different time changes = types */

    begin_timechangetypes = in_head;

    /* Skip over table entries. */
    in_head += (6 * typecnt);
}

static void process_abbr_table(void)
{
    begin_timezonenames = in_head;

    /* Skip over table entries. */
    in_head += charcnt;
}

static void process_leap_table(void)
{
    /* Skip over table entries. */
    in_head += (8 * leapcnt);
}

static void process_std_table(void)
{
    /* Skip over table entries. */
    in_head += stdcnt;
}

static void process_gmt_table(void)
{
    /* Skip over table entries. */
    in_head += gmtcnt;
}

/* go through the contents of the file and find the positions of 
 * needed data. Uses global pointer: in_head */
static int process_file(const char *file_name)
{
    if (process_header()) {
        g_message ("File (%s) does not look like tz file. Skipping it.",
                   file_name);
        return(1);
    }
    process_local_time_table();
    process_local_time_type_table();
    process_ttinfo_table();
    process_abbr_table();
    process_leap_table();
    process_std_table();
    process_gmt_table();

    return(0); /* ok */
}

static void get_country(void)
{ /* tz_array.city[tz_array.count] contains the city name.
     We will find corresponding country and fill it to the table */
    char *str, *str_nl, cc[4];

    if (!(str = strstr(zone_tab_buf, tz_array.city[tz_array.count])))
        return; /* not found */
    /* we will find corresponding country code (2 char) 
     * by going to the beginning of that line. */
    for (str_nl = str; str_nl > zone_tab_buf && str_nl[0] != '\n'; str_nl--)
        ;
    /* now at the end of the previous line. 
     * There are some comments in that file, but let's play it safe and check */
    if (str_nl < zone_tab_buf)
        return; /* not found */
    /* now step one step forward and we are pointing to the country code */
    tz_array.cc[tz_array.count] = g_new(char, 2 + 1);
    strncpy(tz_array.cc[tz_array.count], ++str_nl, 2);
    tz_array.cc[tz_array.count][2] = '\0';

    /********** then search the country **********/
    /* Need to search line, which starts with country code.
     * Note that it is not enough to search any country code, but it really
     * needs to be the first two chars in the line */
    cc[0] = '\n';
    cc[1] = tz_array.cc[tz_array.count][0];
    cc[2] = tz_array.cc[tz_array.count][1];
    cc[3] = '\0';
    if (!country_buf || !(str = strstr(country_buf, cc)))
        return; /* not found */
    /* country name is after the country code and a single tab */
    str += 4;
    /* but we still need to find how long it is.
     * It ends in the line end. 
     * (There is a line end at the end of the file also.) */
    for (str_nl = str; str_nl[0] != '\n'; str_nl++)
        ;
    tz_array.country[tz_array.count] = g_new(char, (str_nl - str) + 1);
    strncpy(tz_array.country[tz_array.count], str, (str_nl - str));
    tz_array.country[tz_array.count][(str_nl - str)] = '\0';
}

static int timezone_exists_in_ical(void)
{ /* in_timezone_name contains the timezone name.
     We will search if it exists also in the ical zones.tab file */
  /* new libical checks os zone.tab file, so we need to use that if using
     that library instead of our own private libical */
    if (strchr(in_timezone_name, '/')
        && strstr(zone_tab_buf, in_timezone_name))
        return(1); /* yes, it is there */
    else
        return(0); /* not found */
}

/* FIXME: need to check that if OUTFILE is given as a parameter,
 * INFILE is not a directory (or make outfile to act like directory also ? */
static int write_ical_file(void)
{
    int i;
    unsigned int tct_i, abbr_i;
    struct tm cur_gm_time;
    time_t tt_now = time(NULL);
    long tc_time = 0, prev_tc_time = 0; /* TimeChange times */
    char s_next[101], s_prev[101];

    tz_array.city[tz_array.count] = g_strdup(in_timezone_name);

    tz_array.cc[tz_array.count] = NULL;
    tz_array.country[tz_array.count] = NULL;
    if (details)
        get_country();

    in_head = begin_timechanges;
    for (i = 0; (i < (int)timecnt) && (tc_time <= tt_now); i++) {
        /* search for current time setting.
         * timecnt tells how many changes we have in the tz file.
         * i points to the next value to read. */
        prev_tc_time = tc_time;
        tc_time = get_long(); /* start time of this timechange */
    }
    /* i points to the next value to be read, so need to -- */
    if (--i < 0 && typecnt == 0) { 
        /* we failed to find any timechanges that have happened earlier than
         * now and there are no changes defined, so use default UTC=GMT */
        tz_array.utc_offset[tz_array.count] = 0;
        tz_array.dst[tz_array.count] = 0;
        tz_array.tz[tz_array.count] = "UTC";
        tz_array.prev[tz_array.count] = NULL;
        tz_array.next[tz_array.count] = NULL;
        tz_array.next_utc_offset[tz_array.count] = 0;
        tz_array.count++;
        return(1); /* done */
    }
    if (tc_time > tt_now) {
        /* we found previous and next value */
        /* tc_time has the next change time */
        if (details) {
            /* NOTE: If the time change happens for example at 04:00
             * and goes one hour backward, the new time is 03:00 and this
             * is what localtime_r reports. In real life we want to show
             * here 04:00, so let's subtract 1 sec to get close to that.
             * This is a bit similar than 24:00 or 00:00. Summary:
             * 04:00 is returned as 03:00 (change happened already) but
             * 03:59 is returned as 03:59 (change did not yet happen) */
            prev_tc_time -= 1;
            localtime_r((const time_t *)&prev_tc_time, &cur_gm_time);
            orage_strftime(s_prev, 100, "%c", &cur_gm_time);
            tz_array.prev[tz_array.count] = g_strdup(s_prev);

            tc_time -= 1;
            localtime_r((const time_t *)&tc_time, &cur_gm_time);
            orage_strftime(s_next, 100, "%c", &cur_gm_time);
            tz_array.next[tz_array.count] = g_strdup(s_next);
            /* get timechange type index */
            if (timecnt) {
                in_head = begin_timechangetypeindexes;
                tct_i = (unsigned int)in_head[i];
            }
            else
                tct_i = 0;

            /* get timechange type */
            in_head = begin_timechangetypes;
            in_head += 6*tct_i;
            tz_array.next_utc_offset[tz_array.count] = (int)get_long();
        }
        else {
            tz_array.prev[tz_array.count] = NULL;
            tz_array.next[tz_array.count] = NULL;
        }
        i--; /* we need to take the previous value */
    }
    else { /* no next value, but previous may exist */
        tz_array.next[tz_array.count] = NULL;
        if (details && prev_tc_time) {
            prev_tc_time -= 1;
            localtime_r((const time_t *)&prev_tc_time, &cur_gm_time);
            orage_strftime(s_prev, 100, "%c", &cur_gm_time);
            tz_array.prev[tz_array.count] = g_strdup(s_prev);
        }
        else
            tz_array.prev[tz_array.count] = NULL;
    }

    /* i now points to latest time change and shows current time.
     * So we found our result and can start collecting real data: */

    /* get timechange type index */
    if (timecnt) {
        in_head = begin_timechangetypeindexes;
        tct_i = (unsigned int)in_head[i];
    }
    else 
        tct_i = 0;

    /* get timechange type */
    in_head = begin_timechangetypes;
    in_head += 6*tct_i;
    tz_array.utc_offset[tz_array.count] = (int)get_long();
    tz_array.dst[tz_array.count] = in_head[0];
    abbr_i =  in_head[1];

     /* get timezone name */
    in_head = begin_timezonenames;
    tz_array.tz[tz_array.count] = g_strdup((char *)in_head + abbr_i);

    tz_array.count++;

    return(0);
}

static int file_call_process_file(const char *file_name
        , const struct stat *sb, int flags)
{
    struct stat file_stat;

    if (flags == FTW_SL)
        g_debug ("processing symbolic link=(%s)", file_name);
    else
        g_debug ("processing file=(%s)", file_name);

    in_timezone_name = g_strdup(&file_name[in_file_base_offset
            + strlen("zoneinfo/")]);
    timezone_name = g_strdup(in_timezone_name);
    if (check_ical && !timezone_exists_in_ical()) {
        g_debug ("skipping file=(%s) as it does not exist in libical",
                 file_name);
        g_free(in_timezone_name);
        g_free(timezone_name);
        return(1);
    }
    if (flags == FTW_SL) {
        read_file(file_name, sb);
    /* we know it is symbolic link, so we actually need stat instead of lstat
    which nftw gives us!
    (lstat = information from the real file istead of the link) */ 
        if (g_stat (file_name, &file_stat)) {
            g_warning ("file open error (%s): %s", file_name,
                       g_strerror (errno));
            g_free(in_timezone_name);
            g_free(timezone_name);
            return(1);
        }
        read_file(file_name, &file_stat);
    }
    else
        read_file(file_name, sb);
    if (process_file(file_name)) { /* we skipped this file */
        g_free(in_timezone_name);
        g_free(timezone_name);
        g_free(in_buf);
        return(1);
    }
    write_ical_file ();

    g_free(in_buf);
    g_free(out_file);
    out_file = NULL;
    g_free(in_timezone_name);
    g_free(timezone_name);
    return(0);
}

/* The main code. This is called once per each file found */
static int file_call(const char *file_name, const struct stat *sb, int flags
        , struct FTW *f)
{
#ifdef FTW_ACTIONRETVAL
    int i;
#endif

    /* we are only interested about files and directories we can access */
    if (flags == FTW_F || flags == FTW_SL) { /* we got file */
        if (file_call_process_file(file_name, sb, flags))
            return(FTW_CONTINUE); /* skip this file */
    }
    else if (flags == FTW_D) { /* this is directory */
        g_debug ("processing directory=(%s)", file_name);
#ifdef FTW_ACTIONRETVAL
        /* need to check if we have excluded directory */
        for (i = 0; (i <= excl_dir_cnt) && excl_dir[i]; i++) {
            if (strcmp(excl_dir[i],  file_name+f->base) == 0) {
                g_message ("skipping excluded directory (%s)",
                           file_name+f->base);
                return(FTW_SKIP_SUBTREE);
            }
        }
#else
        (void)f;
        /* FIXME: this directory should be skipped.
         * Not easy to do that in BSD, where we do not have FTW_ACTIONRETVAL
         * features. It can be done by checking differently.
         */
#endif
    }
    else {
        g_message ("skipping inaccessible file=(%s)", file_name);
    }

    return(FTW_CONTINUE);
}

/* check the parameters and use defaults when possible */
static gboolean check_parameters (void)
{
    char *s_tz, *last_tz = NULL, tz[]="/zoneinfo", tz2[]="zoneinfo/";
    int tz_len;
    gint i;
    struct stat in_stat;
    FILE *par_file;
    struct stat par_file_stat;
    gboolean in_file_is_dir;

    in_file = NULL;
    par_file = g_fopen (TZ_CONVERT_PAR_FILE_LOC, "r");
    if (par_file != NULL) { /* does exist and no error */
        if (g_stat(TZ_CONVERT_PAR_FILE_LOC, &par_file_stat) == -1) {
            /* error reading the parameter file */
            g_warning ("in_file name not found from (%s)",
                       TZ_CONVERT_PAR_FILE_LOC);
        }
        else { /* no errors */
            in_file = g_new(char, par_file_stat.st_size+1);
            if (((off_t)fread(in_file, 1, par_file_stat.st_size, par_file)
                        < par_file_stat.st_size)
            && (ferror(par_file))) {
                g_warning ("error reading (%s)", TZ_CONVERT_PAR_FILE_LOC);
                g_free(in_file);
                in_file = NULL;
            }
            else { 
                /* terminate with nul */
                if (in_file[par_file_stat.st_size-1] == '\n')
                    in_file[par_file_stat.st_size-1] = '\0';
                else
                    in_file[par_file_stat.st_size] = '\0';
                /* test that it is fine */
                if (g_stat (in_file, &par_file_stat) == -1) { /* error */
                    g_warning ("error reading (%s) (from %s)", in_file,
                               TZ_CONVERT_PAR_FILE_LOC);
                    g_free(in_file);
                    in_file = NULL;
                }
            }
        }
        fclose(par_file);
    }
    if (in_file == NULL) /* in file not found */
        in_file = get_zoneinfo_directory ();

    if (in_file[0] != '/') {
        g_warning ("in_file name (%s) is not absolute file name. Ending",
                   in_file);
        return FALSE;
    }

    if (g_stat (in_file, &in_stat) == -1) { /* error */
        g_warning ("file error '%s': %s", in_file, g_strerror (errno));
        return FALSE;
    }

    if (S_ISDIR(in_stat.st_mode)) {
        in_file_is_dir = TRUE;
        if (timezone_name) {
            g_warning ("when infile (%s) is directory, you can not specify "
                       "timezone name (%s), but it is copied from each in "
                       "file. Ending", in_file, timezone_name);
            return FALSE;
        }
        if (out_file) {
            g_warning ("when infile (%s) is directory, you can not specify "
                       "outfile name (%s), but it is copied from each in file. "
                       "Ending", in_file, out_file);
            return FALSE;
        }
    }
    else {
        in_file_is_dir = FALSE;
        if (!S_ISREG(in_stat.st_mode)) {
            g_warning ("in_file (%s) is not directory nor normal file. Ending",
                       in_file);
            return FALSE;
        }
    }

    /* find last "/zoneinfo" from the infile (directory) name. 
     * Normally there is only one. 
     * It needs to be at the end of the string or be followed by '/' */
    tz_len = strlen(tz);
    s_tz = in_file;
    for (s_tz = strstr(s_tz, tz); s_tz != NULL; s_tz = strstr(s_tz, tz)) {
        if (s_tz[tz_len] == '\0' || s_tz[tz_len] == '/')
            last_tz = s_tz;
        s_tz++;
    }
    if (last_tz == NULL) {
        g_warning ("in_file name (%s) does not contain (%s). Ending", in_file,
                   tz);
        return FALSE;
    }

    in_file_base_offset = last_tz - in_file + 1; /* skip '/' */

    if (in_file_is_dir == FALSE) {
        in_timezone_name = g_strdup(&in_file[in_file_base_offset + strlen(tz2)]);
        if (timezone_name == NULL)
            timezone_name = g_strdup(in_timezone_name);
    }

    if (excl_dir == NULL) { /* use default */
        excl_dir_cnt = 5; /* just in case it was changed by parameter */
        excl_dir = g_new0(char *, 3);
        excl_dir[0] = g_strdup("posix");
        excl_dir[1] = g_strdup("right");
    }

    g_message ("infile: '%s' is %s", in_file,
               in_file_is_dir ? "directory" : "normal file");
    g_debug ("year limit: %d", ignore_older);
    g_debug ("infile timezone: '%s'", in_timezone_name);
    g_debug ("outfile: '%s'", out_file);
    g_debug ("outfile timezone: '%s'", timezone_name);
    g_debug ("maximum exclude directory count: %d", excl_dir_cnt);

    for (i = 0; (i <= excl_dir_cnt) && excl_dir[i]; i++)
        g_debug ("exclude directory %d: '%s'" , i, excl_dir[i]);

    return TRUE;
}

static gboolean read_os_timezones (void)
{
    char *tz_dir, *zone_tab_file_name;
    int zoneinfo_len=strlen("zoneinfo/");
    FILE *zone_tab_file;
    struct stat zone_tab_file_stat;
    size_t len;

    /****** zone.tab file ******/
    if (zone_tab_buf) {
        return TRUE;
    }

    len = in_file_base_offset + zoneinfo_len + 1;
    tz_dir = g_new(char, in_file_base_offset + zoneinfo_len + 1); /* '\0' */
    strncpy(tz_dir, in_file, in_file_base_offset);
    tz_dir[in_file_base_offset] = '\0'; 
    g_strlcat (tz_dir, "zoneinfo/", len); /* now we have the base directory */

    len = strlen(tz_dir) + strlen(ZONETAB_FILE) + 1;
    zone_tab_file_name = g_new (char, len);
    g_strlcpy (zone_tab_file_name, tz_dir, len);
    g_strlcat (zone_tab_file_name, ZONETAB_FILE, len);

    g_free(tz_dir);

    if (!(zone_tab_file = g_fopen (zone_tab_file_name, "r"))) {
        g_warning ("zone.tab file open failed (%s): %s", zone_tab_file_name,
                   g_strerror (errno));
        g_free(zone_tab_file_name);
        return FALSE;
    }
    if (g_stat (zone_tab_file_name, &zone_tab_file_stat) == -1) {
        g_warning ("zone.tab file stat failed (%s): %s", zone_tab_file_name,
                   g_strerror (errno));
        g_free(zone_tab_file_name);
        fclose(zone_tab_file);
        return FALSE;
    }
    zone_tab_buf = g_new(char, zone_tab_file_stat.st_size+1);
    if (((off_t)fread(zone_tab_buf, 1, zone_tab_file_stat.st_size, zone_tab_file) < zone_tab_file_stat.st_size)
    && (ferror(zone_tab_file))) {
        g_warning ("zone.tab file read failed (%s): %s", zone_tab_file_name,
                   g_strerror (errno));
        g_free(zone_tab_file_name);
        fclose(zone_tab_file);
        return FALSE;
    }
    zone_tab_buf[zone_tab_file_stat.st_size] = '\0';
    g_free(zone_tab_file_name);
    fclose(zone_tab_file);

    return TRUE;
}

static gboolean read_countries (void)
{
    char *tz_dir, *country_file_name;
    int zoneinfo_len=strlen("zoneinfo/");
    FILE *country_file;
    struct stat country_file_stat;
    size_t len;

    /****** country=iso3166.tab file ******/
    if (country_buf) { /* we have read it already */
        return TRUE;
    }

    len = in_file_base_offset + zoneinfo_len + 1;
    tz_dir = g_new (char, len); /* '\0' */
    strncpy(tz_dir, in_file, in_file_base_offset);
    tz_dir[in_file_base_offset] = '\0'; 
#ifdef __OpenBSD__ 
    g_strlcat (tz_dir, "misc/", len); /* this is shorter than "zoneinfo" so it is safe */
#else
    g_strlcat (tz_dir, "zoneinfo/", len); /* now we have the base directory */
#endif

    len = strlen(tz_dir) + strlen(COUNTRY_FILE) + 1;
    country_file_name = g_new (char, len);
    g_strlcpy (country_file_name, tz_dir, len);
    g_strlcat (country_file_name, COUNTRY_FILE, len);
    g_free(tz_dir);

    if (!(country_file = g_fopen (country_file_name, "r"))) {
        g_warning ("iso3166.tab file open failed (%s): %s", country_file_name,
                   g_strerror (errno));
        g_free(country_file_name);
        return FALSE;
    }
    if (g_stat (country_file_name, &country_file_stat) == -1) {
        g_warning ("iso3166.tab file stat failed (%s): %s", country_file_name,
                   g_strerror (errno));
        g_free(country_file_name);
        fclose(country_file);
        return FALSE;
    }
    country_buf = g_new(char, country_file_stat.st_size+1);
    if (((off_t)fread(country_buf, 1, country_file_stat.st_size, country_file) < country_file_stat.st_size)
    && (ferror(country_file))) {
        g_warning ("iso3166.tab file read failed (%s): %s", country_file_name,
                   g_strerror (errno));
        g_free(country_file_name);
        fclose(country_file);
        return FALSE;
    }
    country_buf[country_file_stat.st_size] = '\0';
    g_free(country_file_name);
    fclose(country_file);
    return TRUE;
}

orage_timezone_array get_orage_timezones(int show_details, int ical)
{
#ifdef FTW_ACTIONRETVAL
    const guint tz_array_size = 1000; /* FIXME: this needs to be counted */
    const int nftw_flags = FTW_PHYS | FTW_ACTIONRETVAL;
#else
    const guint tz_array_size = 2000; /* BSD can not skip unneeded directories */
    const int nftw_flags = FTW_PHYS;
#endif

    details = show_details;
    check_ical = ical;

    if (tz_array.count)
        return tz_array;

    tz_array.city = g_new (char *, tz_array_size+2);
    tz_array.utc_offset = g_new (int, tz_array_size+2);
    tz_array.dst = g_new (int, tz_array_size+2);
    tz_array.tz = g_new (char *, tz_array_size+2);
    tz_array.prev = g_new (char *, tz_array_size+2);
    tz_array.next = g_new (char *, tz_array_size+2);
    tz_array.next_utc_offset = g_new (int, tz_array_size+2);
    tz_array.country = g_new (char *, tz_array_size+2);
    tz_array.cc = g_new (char *, tz_array_size+2);

    if (check_parameters () == FALSE)
        goto out;

    g_debug ("processing files from '%s'", in_file);

    if (details)
    {
        if (read_os_timezones () == FALSE)
            goto out;

        if (read_countries () == FALSE)
            goto out;
    }

    if (check_ical)
    {
        if (read_os_timezones () == FALSE)
            goto out;
    }

    /* nftw goes through the whole file structure and calls "file_call" with
     * each file. It returns 0 when everything has been done and -1 if it run
     * into an error.
     */
    if (nftw (in_file, file_call, 10, nftw_flags) == -1)
    {
        g_critical ("nftw error in file handling: %s", g_strerror (errno));
        exit (EXIT_FAILURE);
    }

    g_message ("processed %d timezone file(s) from '%s'",
               number_of_proccessed_files, in_file);

out:
    g_debug ("adding default timezone entries");
    g_free (in_file);

    tz_array.utc_offset[tz_array.count] = 0;
    tz_array.dst[tz_array.count] = 0;
    tz_array.tz[tz_array.count] = g_strdup ("UTC");
    tz_array.prev[tz_array.count] = NULL;
    tz_array.next[tz_array.count] = NULL;
    tz_array.next_utc_offset[tz_array.count] = 0;
    tz_array.country[tz_array.count] = NULL;
    tz_array.cc[tz_array.count] = NULL;
    tz_array.city[tz_array.count] = g_strdup ("UTC");
    tz_array.count++;

    tz_array.utc_offset[tz_array.count] = 0;
    tz_array.dst[tz_array.count] = 0;
    tz_array.tz[tz_array.count] = NULL;
    tz_array.prev[tz_array.count] = NULL;
    tz_array.next[tz_array.count] = NULL;
    tz_array.next_utc_offset[tz_array.count] = 0;
    tz_array.country[tz_array.count] = NULL;
    tz_array.cc[tz_array.count] = NULL;
    tz_array.city[tz_array.count] = g_strdup ("floating");
    tz_array.count++;

    return tz_array;
}

void free_orage_timezones (void)
{
    int i;

    for (i = 0 ; i < tz_array.count; i++) {
        if (tz_array.city[i])
            g_free(tz_array.city[i]);
        if (tz_array.tz[i])
            g_free(tz_array.tz[i]);
        if (tz_array.prev[i])
            g_free(tz_array.prev[i]);
        if (tz_array.next[i])
            g_free(tz_array.next[i]);
        if (tz_array.country[i])
            g_free(tz_array.country[i]);
        if (tz_array.cc[i])
            g_free(tz_array.cc[i]);
    }
    g_free(tz_array.city);
    g_free(tz_array.utc_offset);
    g_free(tz_array.dst);
    g_free(tz_array.tz);
    g_free(tz_array.prev);
    g_free(tz_array.next);
    g_free(tz_array.next_utc_offset);
    g_free(tz_array.country);
    g_free(tz_array.cc);
    tz_array.count = 0;
    timezone_name = NULL;
    if (zone_tab_buf) {
        g_free(zone_tab_buf);
        zone_tab_buf = NULL;
    }
    if (country_buf) {
        g_free(country_buf);
        country_buf = NULL;
    }
    if (zones_tab_buf) {
        g_free(zones_tab_buf);
        zones_tab_buf = NULL;
    }
    number_of_proccessed_files = 0;
}
