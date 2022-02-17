bin_PROGRAMS = orage

noinst_LTLIBRARIES = liborage.la

liborage_la_SOURCES =						\
	functions.c functions.h					\
	tz_zoneinfo_read.c tz_zoneinfo_read.h

orage_SOURCES =							\
	$(orage_dbus_sources)					\
	about-xfcalendar.c					\
	about-xfcalendar.h					\
	appointment.c						\
	appointment.h						\
	day-view.c						\
	day-view.h						\
	event-list.c						\
	event-list.h						\
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
	tray_icon.h

liborage_la_CPPFLAGS =						\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"
	
liborage_la_CFLAGS =						\
	$(LIBGTK_CFLAGS)
	
orage_CPPFLAGS =						\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"

orage_CFLAGS =							\
	$(LIBGTK_CFLAGS)					\
	$(LIBX11_CFLAGS)

orage_LDADD =							\
	$(LIBGTK_LIBS)						\
	$(LIBX11_LIBS)						\
	-lm							\
	$(INTLLIBS)						\
	liborage.la

if INCLUDED_LIBICAL
orage_CPPFLAGS +=						\
	-I$(top_builddir)/libical/src/libical			\
	-I$(top_builddir)/libical/src/libicalss			\
	-I$(top_srcdir)/libical/src/libical			\
	-I$(top_srcdir)/libical/src/libicalss

orage_LDADD +=							\
	$(PTHREAD_LIBS)										\
	$(top_builddir)/libical/src/libical/libical.la		\
	$(top_builddir)/libical/src/libicalss/libicalss.la
else
orage_CFLAGS +=	$(LIBICAL_CFLAGS)
orage_LDADD += $(LIBICAL_LIBS)
endif

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

orage-dbus-service.h: $(srcdir)/orage-dbus-service.xml Makefile
	dbus-binding-tool --prefix=orage --mode=glib-server $(srcdir)/orage-dbus-service.xml > orage-dbus-service.h

BUILT_SOURCES = orage-dbus-service.h
DISTCLEANFILES = orage-dbus-service.h
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

# vi:set ts=8 sw=8 noet ai:
