#ifndef __OCAL_MONTH_CELL_H__
#define __OCAL_MONTH_CELL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OCAL_MONTH_CELL_TYPE (ocal_month_cell_get_type ())

G_DECLARE_FINAL_TYPE (OcalMonthCell, ocal_month_cell, OCAL, MONTH_CELL, GtkBox)

GtkWidget *ocal_month_cell_new ();

void ocal_month_cell_set_date (OcalMonthCell *self, GDateTime *date);

G_END_DECLS

#endif // __OCAL_MONTH_CELL_H__
