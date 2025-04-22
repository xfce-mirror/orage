ACLOCAL_AMFLAGS = -I m4

AUTOMAKE_OPTIONS = 1.8 dist-bzip2

SUBDIRS = icons			\
	sounds			\
	src			\
	plugin			\
	themes			\
	po

desktopdir = $(datadir)/applications
desktop_in_files = org.xfce.orage.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

org.xfce.orage.desktop: org.xfce.orage.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

appdatadir = $(datadir)/metainfo
appdata_in_files = org.xfce.orage.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

org.xfce.orage.appdata.xml: org.xfce.orage.appdata.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

servicedir = $(datadir)/dbus-1/services
service_in_files = org.xfce.orage.service.in
service_DATA = $(service_in_files:.service.in=.service)
%.service: %.service.in
	$(AM_V_GEN) $(SED) -e "s,\@bindir\@,$(bindir),g" < $< > $@

EXTRA_DIST =								\
	$(desktop_in_files)						\
	$(appdata_in_files)						\
	$(service_in_files)						\
	meson.build							\
	meson_options.txt						\
	po/meson.build							\
	xfce-revision.h.in						\
	$(NULL)

DISTCLEANFILES =							\
	$(desktop_DATA)							\
	$(appdata_DATA)							\
	$(service_DATA)

distclean-local:
	rm -rf *.cache *~

# vi:set ts=8 sw=8 noet ai nocindent:
