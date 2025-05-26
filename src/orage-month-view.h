#ifndef __OCAL_MONTH_VIEW__
#define __OCAL_MONTH_VIEW__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ORAGE_MONTH_VIEW_TYPE (orage_month_view_get_type ())

G_DECLARE_FINAL_TYPE (OrageMonthView, orage_month_view, ORAGE, MONTH_VIEW, GtkBox)

GtkWidget *orage_month_view_new ();

G_END_DECLS

#endif
