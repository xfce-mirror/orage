#include "orage-month-cell.h"
#include "orage-time-util.h"

struct _OcalMonthCell
{
    /* Add GtkOverlay as a main container for this widgets */
    GtkBox parent;

    GtkWidget *main_box;

    GtkWidget *day_label;

    GDateTime *date;
};

G_DEFINE_TYPE (OcalMonthCell, ocal_month_cell, GTK_TYPE_BOX)

void
ocal_month_cell_set_date (OcalMonthCell *self, GDateTime *date)
{
    g_autofree gchar *text = NULL;

    if (self->date && date
        && ocal_date_time_compare_date (self->date, date) == 0)
        return;

    ocal_clear_date_time (&self->date);
    self->date = g_date_time_ref (date);

    text = g_strdup_printf ("%d", g_date_time_get_day_of_month (date));

    gtk_label_set_text (GTK_LABEL (self->day_label), text);
}

static void
ocal_month_cell_init (OcalMonthCell *self)
{
    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    self->day_label = gtk_label_new ("x");

    gtk_box_pack_start (GTK_BOX (self->main_box), self->day_label, FALSE, FALSE,
                        0);

    gtk_box_pack_start (GTK_BOX (&self->parent), self->main_box, FALSE, FALSE,
                        0);
}

static void
ocal_month_cell_class_init (OcalMonthCellClass *klass)
{
}

GtkWidget *
ocal_month_cell_new ()
{
    return g_object_new (OCAL_MONTH_CELL_TYPE, NULL);
}
