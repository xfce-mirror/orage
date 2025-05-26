#include "orage-month-view.h"
#include "orage-month-cell.h"
#include "orage-time-util.h"

struct _OrageMonthView
{
    GtkBox parent;

    GtkWidget *main_box;

    GtkWidget *header_buttons;
    GtkWidget *button_next;
    GtkWidget *button_prev;

    GtkWidget *month_grid;

    GtkWidget *label_weekday[7];

    GtkWidget *month_cell[6][7];

    GtkWidget *header;

    GDateTime *date;
    gint days_delay;
    gint first_weekday;
};

gchar *day_name[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                     "Thursday", "Friday", "Saturday"};

static void update_month_cells (OrageMonthView *self);

static void update_days_delay (OrageMonthView *self, gint month_different);

G_DEFINE_TYPE (OrageMonthView, orage_month_view, GTK_TYPE_BOX)

static void
update_days_delay (OrageMonthView *self, gint month_different)
{
    self->days_delay
            = (time_day_of_week (
                                 1, g_date_time_get_month (self->date) + month_different,
                                 g_date_time_get_year (self->date))
               - self->first_weekday + 7)
            % 7;
}

static void
next_button_clicked (OrageMonthView *self, GtkWidget *widget)
{
    update_days_delay (self, 1);

    self->date = g_date_time_add_months (self->date, 1);
    update_month_cells (self);
}

static void
prev_button_clicked (OrageMonthView *self, GtkWidget *widget)
{
    update_days_delay (self, -1);
    self->date = g_date_time_add_months (self->date, -1);
    update_month_cells (self);
}

static void
setup_month_grid (OrageMonthView *self)
{
    guint row, col;

    for (row = 0; row < 6; row++)
    {
        for (col = 0; col < 7; col++)
        {
            GtkWidget *cell;

            cell = ocal_month_cell_new ();

            self->month_cell[row][col] = cell;
            gtk_grid_attach (GTK_GRID (self->month_grid), cell, col, (row + 1),
                             1, 1);
        }
    }
}

static void
update_month_cells (OrageMonthView *self)
{
    guint row, col;
    g_autoptr (GDateTime) dt = NULL;

    dt = g_date_time_new_local (g_date_time_get_year (self->date),
                                g_date_time_get_month (self->date), 1, 0, 0, 0);

    for (row = 0; row < 6; row++)
    {

        for (col = 0; col < 7; col++)
        {
            g_autoptr (GDateTime) cell_date = NULL;
            OcalMonthCell *cell;

            cell = OCAL_MONTH_CELL (self->month_cell[row][col]);

            cell_date
                    = g_date_time_add_days (dt, row * 7 + col - self->days_delay);
            ocal_month_cell_set_date (cell, cell_date);
        }
    }
}

static void
orage_month_view_init (OrageMonthView *self)
{

    self->date = g_date_time_new_now_utc ();
    self->first_weekday = 0;
    update_days_delay (self, -1);

    self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

    self->header_buttons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

    self->button_prev = gtk_button_new_with_label ("Prev");
    g_signal_connect_swapped (self->button_prev, "clicked",
                              G_CALLBACK (prev_button_clicked), self);

    gtk_box_pack_start (GTK_BOX (self->header_buttons), self->button_prev, FALSE,
                        FALSE, 0);

    self->button_next = gtk_button_new_with_label ("Next");
    g_signal_connect_swapped (self->button_next, "clicked",
                              G_CALLBACK (next_button_clicked), self);

    gtk_box_pack_start (GTK_BOX (self->header_buttons), self->button_next, FALSE,
                        FALSE, 0);

    gtk_box_pack_start (GTK_BOX (self->main_box), self->header_buttons, FALSE,
                        FALSE, 0);
    self->month_grid = gtk_grid_new ();

    for (int i = 0; i < 7; i++)
    {
        self->label_weekday[i] = gtk_label_new_with_mnemonic (day_name[i]);
        gtk_grid_attach (GTK_GRID (self->month_grid), self->label_weekday[i], i,
                         0, 1, 1);
    }

    setup_month_grid (self);
    update_month_cells (self);

    gtk_box_pack_start (GTK_BOX (self->main_box), self->month_grid, FALSE, FALSE,
                        0);

    gtk_box_pack_start (GTK_BOX (&self->parent), self->main_box, FALSE, FALSE,
                        0);
}

static void
orage_month_view_class_init (OrageMonthViewClass *klass)
{
    GObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = G_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);
}

GtkWidget *orage_month_view_new (void)
{
    return g_object_new (ORAGE_MONTH_VIEW_TYPE, NULL);
}
