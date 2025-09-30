bin_PROGRAMS = orage

orage_SOURCES =							\
	event-list.c						\
	event-list.h						\
	functions.c						\
	functions.h						\
	ical-archive.c						\
	ical-code.c						\
	ical-code.h						\
	ical-internal.h						\
	ical-expimp.c						\
	ical-expimp.h						\
	interface.c						\
	interface.h						\
	main.c							\
	orage-about.c						\
	orage-about.h						\
	orage-alarm-structure.c					\
	orage-alarm-structure.h					\
	orage-application.c					\
	orage-application.h					\
	orage-appointment-window.c				\
	orage-appointment-window.h				\
	orage-category.c					\
	orage-category.h					\
	orage-css.c						\
	orage-css.h						\
	orage-event.c						\
	orage-event.h						\
	orage-import.c						\
	orage-import.h						\
	orage-month-cell.c					\
	orage-month-cell.h					\
	orage-month-view.c					\
	orage-month-view.h					\
	orage-i18n.h						\
	orage-rc-file.c						\
	orage-rc-file.h						\
	orage-sleep-monitor.c					\
	orage-sleep-monitor.h					\
	orage-sync-edit-dialog.c				\
	orage-sync-edit-dialog.h				\
	orage-sync-ext-command.c				\
	orage-sync-ext-command.h				\
	orage-task-runner.c					\
	orage-task-runner.h    					\
	orage-time-utils.c					\
	orage-time-utils.h					\
	orage-week-window.c					\
	orage-week-window.h					\
	orage-window-classic.c					\
	orage-window-classic.h					\
	orage-window-next.c					\
	orage-window-next.h					\
	orage-window.c						\
	orage-window.h						\
	parameters.c						\
	parameters.h						\
	parameters_internal.h					\
	reminder.c						\
	reminder.h						\
	timezone_names.c					\
	timezone_names.h					\
	timezone_selection.c					\
	timezone_selection.h					\
	tz_zoneinfo_read.c					\
	tz_zoneinfo_read.h					\
	xfical_exception.c					\
	xfical_exception.h

if HAVE_X11_TRAY_ICON
orage_SOURCES +=						\
	tray_icon.c						\
	tray_icon.h
endif
	
orage_CPPFLAGS =						\
	-DPACKAGE_DATADIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"			\
	-DG_LOG_DOMAIN=\"orage\"

orage_CFLAGS =							\
	$(LIBGTK_CFLAGS)					\
	$(LIBX11_CFLAGS)					\
	$(LIBXFCE4UTIL_CFLAGS)					\
	$(LIBXFCE4UI_CFLAGS)					\
	$(LIBICALGLIB_CFLAGS)					\
	$(GIO_CFLAGS)

orage_LDADD =							\
	$(LIBGTK_LIBS)						\
	$(LIBX11_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)					\
	$(LIBXFCE4UI_LIBS)					\
	$(LIBICALGLIB_LIBS)					\
	$(GIO_LIBS)						\
	-lm							\
	$(INTLLIBS)

if HAVE_NOTIFY
orage_LDADD +=							\
	$(NOTIFY_LIBS)
endif

EXTRA_DIST =							\
	meson.build						\
	$(NULL)

# vi:set ts=8 sw=8 noet ai:
