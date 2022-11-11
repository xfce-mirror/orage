bin_PROGRAMS = orage

orage_built_sources =						\
	orage-dbus-service.c					\
	orage-dbus-service.h

orage_SOURCES =							\
	$(orage_built_sources)					\
	$(orage_dbus_sources)					\
	about-xfcalendar.c					\
	about-xfcalendar.h					\
	appointment.c						\
	appointment.h						\
	day-view.c						\
	day-view.h						\
	event-list.c						\
	event-list.h						\
	functions.c						\
	functions.h						\
	ical-archive.c						\
	ical-code.c						\
	ical-code.h						\
	ical-internal.h						\
	ical-expimp.c						\
	interface.c						\
	interface.h						\
	main.c							\
	mainbox.c						\
	mainbox.h						\
	orage-css.c						\
	orage-css.h						\
	orage-i18n.h						\
	parameters.c						\
	parameters.h						\
	parameters_internal.h					\
	reminder.c						\
	reminder.h						\
	timezone_names.c					\
	timezone_selection.c					\
	timezone_selection.h					\
	tray_icon.c						\
	tray_icon.h						\
	tz_zoneinfo_read.c					\
	tz_zoneinfo_read.h					\
	xfical_exception.c					\
	xfical_exception.h
	
orage_CPPFLAGS =						\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"			\
	-DG_LOG_DOMAIN=\"orage\"

orage_CFLAGS =							\
	$(LIBGTK_CFLAGS)					\
	$(LIBX11_CFLAGS)					\
	$(LIBXFCE4UTIL_CFLAGS)					\
	$(LIBICAL_CFLAGS)					\
	$(GIO_CFLAGS)

orage_LDADD =							\
	$(LIBGTK_LIBS)						\
	$(LIBX11_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)					\
	$(LIBICAL_LIBS)						\
	$(GIO_LIBS)						\
	-lm							\
	$(INTLLIBS)

if HAVE_DBUS
orage_dbus_sources =						\
	orage-dbus-object.c					\
	orage-dbus-object.h					\
	orage-dbus-client.c					\
	orage-dbus-client.h					\
	orage-dbus-service.xml					\
	orage-dbus-service.h

orage_CPPFLAGS += -DDBUS_API_SUBJECT_TO_CHANGE
orage_CFLAGS += $(DBUS_CFLAGS)
orage_LDADD += $(DBUS_LIBS)

orage-dbus-service.h:
orage-dbus-service.c: orage-dbus-service.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
	--interface-prefix org.xfce.Orage \
	--c-namespace Orage   \
	--generate-c-code orage-dbus-service \
	$(srcdir)/orage-dbus-service.xml

BUILT_SOURCES = $(orage_built_sources)
DISTCLEANFILES = $(orage_built_sources)
endif

if HAVE_NOTIFY
orage_LDADD +=							\
	$(NOTIFY_LIBS)
endif

if HAVE_LIBXFCE4UI
orage_CFLAGS +=							\
	$(LIBXFCE4UI_CFLAGS)
	
orage_LDADD +=							\
	$(LIBXFCE4UI_LIBS)
endif

EXTRA_DIST = orage-dbus-service.xml

# vi:set ts=8 sw=8 noet ai:
