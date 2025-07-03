/*
 * Copyright (c) 2025 Erkki Moorits
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

#include "orage-month-view.h"
#include "orage-month-cell.h"
#include "orage-time-util.h"
#include "orage-i18n.h"
#include "functions.h"

#include <gtk/gtk.h>

#define MONTH_VIEW_FIRST_DAY_OF_WEEK_PROPERTY "fist-day-of-week"

struct _OrageMonthView
{
    GtkBox parent;

    GtkWidget *main_box;
    GtkWidget *month_grid;
    GtkWidget *month_label;
    GtkWidget *label_weekday[7];
    GtkWidget *label_week_nr[6];
    GtkWidget *month_cell[6][7];
    GtkWidget *header;

    GDateTime *date;
    FirstDayOfWeek first_weekday;
    FirstDayOfWeek first_weekday2;
};

G_DEFINE_TYPE (OrageMonthView, orage_month_view, GTK_TYPE_BOX)

enum
{
    PROP_0,
    MONTH_VIEW_FIRST_DAY_OF_WEEK,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = {NULL,};

static void load_css (void)
{
    GtkCssProvider *provider = gtk_css_provider_new ();
    GdkDisplay *display = gdk_display_get_default ();
    GdkScreen *screen = gdk_display_get_default_screen (display);

    gtk_style_context_add_provider_for_screen (screen,
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_USER);

    gtk_css_provider_load_from_data (provider,
                                    ".cell {\n"
                                    "  border: 1px solid #888;\n"
                                    "  background-color: #f0f0f0;\n"
                                    "  padding: 6px;\n"
                                    "}\n"
                                    ".cell:hover {\n"
                                    "  background-color: #e0e0ff;\n"
                                    "  border-color: #333388;\n"
                                    "}\n"
                                    ".month-label {\n"
                                    "  font-weight: bold;\n"
                                    "}\n",
                                    -1, NULL);

    g_object_unref (provider);
}

static FirstDayOfWeek to_first_weekday (const FirstDayOfWeek first_day)
{
    gint first_weekday;

    if (first_day != BY_LOCALE)
        return first_day;

    first_weekday = orage_get_first_weekday ();

    return (first_weekday == 0) ? SUNDAY : MONDAY;
}

static GDateTime *orage_month_view_get_first_day_of_month (OrageMonthView *self)
{
    GDateTime *gdt = self->date;

    return g_date_time_new_local (g_date_time_get_year (gdt),
                                  g_date_time_get_month (gdt), 1, 0, 0, 0);
}

static void orage_month_view_update_month_label (OrageMonthView *self)
{
    const gchar *month_list[12] =
    {
        _("January"), _("February"), _("March"), _("April"), _("May"),
        _("June"), _("July"), _("August"), _("September"), _("October"),
        _("November"), _("December")
    };

    gint month;
    gchar *text;
    GDateTime *gdt = self->date;

    /* We could use the following code:
     * "text = g_date_time_format(self->date, "%B")"
     * However, in some languages the month name starts with a lowercase letter,
     * so we need to ensure that the first letter is uppercase. In this case,
     * it's simpler to just use translated month names directly. Also, GLib's
     * locale handling differs slightly from Orage's.
     */

    month = g_date_time_get_month (gdt);

    /* TRANSLATORS: "%1$s %2$d" is used to display the month and year in the
     * calendar month view page, in "month year" format (for example,
     * "January 2025"). To use the "year month" format (e.g., "2025 January"),
     * use "%2$d %1$s" instead.
     */
    text = g_strdup_printf (_("%1$s %2$d"), month_list[month - 1],
                                            g_date_time_get_year (gdt));
    gtk_label_set_text (GTK_LABEL (self->month_label), text);
    g_free (text);
}

static void orage_month_view_update_month_cells (OrageMonthView *self)
{
    OrageMonthCell *cell;
    GDateTime *cell_date;
    GDateTime *gdt;
    guint row, col;
    gint offset;

    gdt = orage_month_view_get_first_day_of_month (self);
    offset = g_date_time_get_day_of_week (gdt)
           - (self->first_weekday2 == MONDAY ? 1 : 0);

    for (row = 0; row < 6; row++)
    {
        for (col = 0; col < 7; col++)
        {
            cell = ORAGE_MONTH_CELL (self->month_cell[row][col]);

            cell_date = g_date_time_add_days (gdt,
                                              row * 7 + col - offset);
            orage_month_cell_set_date (cell, cell_date);
            g_date_time_unref (cell_date);
        }
    }

    g_date_time_unref (gdt);
}

static void orage_month_view_update_week_nr_cells (OrageMonthView *self)
{
    gchar *cell_text;
    guint row;
    GDateTime *gdt;
    GDateTime *cell_date;
    GtkLabel *cell;

    gdt = orage_month_view_get_first_day_of_month (self);

    for (row = 0; row < 6; row++)
    {
        cell = GTK_LABEL (self->label_week_nr[row]);
        cell_date = g_date_time_add_weeks (gdt, row);
        cell_text = g_date_time_format (cell_date, "%V");
        gtk_label_set_text (cell, cell_text);
        g_free (cell_text);
        g_date_time_unref (cell_date);
    }

    g_date_time_unref (gdt);
}

static void orage_month_view_update_day_names (OrageMonthView *self)
{
    static const gchar *day_name[] =
    {
        _("Monday"), _("Tuesday"), _("Wednesday"), _("Thursday"), _("Friday"),
        _("Saturday"), _("Sunday")
    };

    guint col;

    if (self->first_weekday2 == SUNDAY)
    {
        gtk_label_set_text (GTK_LABEL (self->label_weekday[0]), day_name[6]);

        for (col = 0; col < 6; col++)
            gtk_label_set_text (GTK_LABEL (self->label_weekday[col + 1]), day_name[col]);
    }
    else
    {
        for (col = 0; col < 7; col++)
            gtk_label_set_text (GTK_LABEL (self->label_weekday[col]), day_name[col]);
    }
}

static void orage_month_view_update_variable_fields (OrageMonthView *self)
{
    orage_month_view_update_month_label (self);
    orage_month_view_update_month_cells (self);
    orage_month_view_update_week_nr_cells (self);
}

static void orage_month_view_get_property (GObject *object,
                                           const guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
    const OrageMonthView *self = ORAGE_MONTH_VIEW (object);

    switch (prop_id)
    {
        case MONTH_VIEW_FIRST_DAY_OF_WEEK:
            g_value_set_int (value, (int)self->first_weekday);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void orage_month_view_set_property (GObject *object,
                                           const guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
    OrageMonthView *self = ORAGE_MONTH_VIEW (object);

    switch (prop_id)
    {
        case MONTH_VIEW_FIRST_DAY_OF_WEEK:
            self->first_weekday= (FirstDayOfWeek)g_value_get_int (value);
            self->first_weekday2= to_first_weekday (self->first_weekday);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void orage_month_view_constructed (GObject *object)
{
    OrageMonthView *self = (OrageMonthView *)object;

    orage_month_view_update_day_names (self);
    orage_month_view_update_variable_fields (self);

    G_OBJECT_CLASS (orage_month_view_parent_class)->constructed (object);
}

static void orage_month_view_class_init (OrageMonthViewClass *klass)
{
    GObjectClass *object_class;

    load_css ();

    object_class = G_OBJECT_CLASS (klass);
    object_class->constructed = orage_month_view_constructed;
    object_class->get_property = orage_month_view_get_property;
    object_class->set_property = orage_month_view_set_property;

    properties[MONTH_VIEW_FIRST_DAY_OF_WEEK] =
            g_param_spec_int (MONTH_VIEW_FIRST_DAY_OF_WEEK_PROPERTY,
                              MONTH_VIEW_FIRST_DAY_OF_WEEK_PROPERTY,
                              "First day of week",
                              MONDAY,
                              BY_LOCALE,
                              MONDAY,
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void orage_month_view_init (OrageMonthView *self)
{
    GtkSizeGroup *date_group;
    GtkWidget *cell;
    guint row;
    guint col;

    self->date = g_date_time_new_now_utc ();
    self->first_weekday = MONDAY;
    self->first_weekday2 = self->first_weekday;

    date_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

    self->month_label = gtk_label_new (NULL);
    gtk_style_context_add_class (
        gtk_widget_get_style_context (self->month_label), "month-label");
    gtk_box_pack_start (GTK_BOX (self->main_box), self->month_label, FALSE,
                        TRUE, 0);

    self->month_grid = gtk_grid_new ();
    g_object_set (self->month_grid, "hexpand", TRUE,
                                    "vexpand", TRUE,
                                    "halign", GTK_ALIGN_FILL,
                                    "valign", GTK_ALIGN_FILL,
                                    NULL);

    /* Setup title row. */
    cell = gtk_label_new ("Week");
    gtk_grid_attach (GTK_GRID (self->month_grid), cell, 0, 0, 1, 1);
    gtk_style_context_add_class (gtk_widget_get_style_context (cell), "cell");

    for (col = 0; col < 7; col++)
    {
        cell = gtk_label_new (NULL);
        g_object_set (cell, "hexpand", TRUE, "halign", GTK_ALIGN_FILL, NULL);
        gtk_grid_attach (GTK_GRID (self->month_grid), cell, col + 1, 0, 1, 1);
        gtk_style_context_add_class (gtk_widget_get_style_context (cell), "cell");
        gtk_size_group_add_widget (date_group, cell);
        self->label_weekday[col] = cell;
    }

    /* Setup date rows. */
    for (row = 0; row < 6; row++)
    {
        /* Setup week numbers. */
        cell = gtk_label_new (NULL);
        g_object_set (cell, "vexpand", TRUE, "valign", GTK_ALIGN_FILL, NULL);
        gtk_style_context_add_class (gtk_widget_get_style_context (cell), "cell");
        gtk_grid_attach (GTK_GRID (self->month_grid), cell, 0, row + 1, 1, 1);
        self->label_week_nr[row] = cell;

        for (col = 1; col < 8; col++)
        {
            cell = orage_month_cell_new ();
            g_object_set (cell, "hexpand", TRUE,
                                "vexpand", TRUE,
                                "halign", GTK_ALIGN_FILL,
                                "valign", GTK_ALIGN_FILL,
                                NULL);
            gtk_style_context_add_class (gtk_widget_get_style_context (cell), "cell");
            gtk_size_group_add_widget (date_group, cell);
            gtk_grid_attach (GTK_GRID (self->month_grid), cell, col, row + 1,
                             1, 1);
            self->month_cell[row][col - 1] = cell;
        }
    }

    gtk_box_pack_start (GTK_BOX (self->main_box), self->month_grid, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (&self->parent), self->main_box, TRUE, TRUE, 0);
}

GtkWidget *orage_month_view_new (const FirstDayOfWeek first_day)
{
    return g_object_new (ORAGE_MONTH_VIEW_TYPE,
                         MONTH_VIEW_FIRST_DAY_OF_WEEK_PROPERTY, first_day,
                         NULL);
}

void orage_month_view_select_month (OrageMonthView *month_view, GDateTime *gdt)
{
    GDateTime *date_old;

    date_old = month_view->date;
    month_view->date = g_date_time_ref (gdt);
    g_date_time_unref (date_old);

    orage_month_view_update_month_label (month_view);
    orage_month_view_update_week_nr_cells (month_view);
    orage_month_view_update_month_cells (month_view);
}
