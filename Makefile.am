ACLOCAL_AMFLAGS = -I m4

AUTOMAKE_OPTIONS = 1.8 dist-bzip2

if INCLUDED_LIBICAL
LIBICAL_SUBDIR = libical
endif

SUBDIRS = $(LIBICAL_SUBDIR)	\
	icons			\
	po			\
	sounds			\
	src			\
	globaltime		\
	plugin			\
	panel-plugin		\
	tz_convert

desktopdir = $(datadir)/applications
desktop_in_files = org.xfce.orage.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

appdatadir = $(datadir)/metainfo
appdata_in_files = org.xfce.orage.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

if HAVE_DBUS
servicedir = $(datadir)/dbus-1/services
service_in_files = org.xfce.calendar.service.in org.xfce.orage.service.in
service_DATA = $(service_in_files:.service.in=.service)
%.service: %.service.in
	$(AM_V_GEN) $(SED) -e "s,\@bindir\@,$(bindir),g" < $< > $@
endif

EXTRA_DIST =								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	$(desktop_in_files)						\
	$(appdata_in_files)						\
	$(service_in_files)

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	$(desktop_DATA)							\
	$(appdata_DATA)							\
	$(service_DATA)

distclean-local:
	rm -rf *.cache *~

# vi:set ts=8 sw=8 noet ai nocindent:
