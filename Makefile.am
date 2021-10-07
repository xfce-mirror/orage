# $Id: Makefile.am 285 2021-10-07 06:46:38Z erx $

if INCLUDED_LIBICAL
LIBICAL_SUBDIR=						\
	libical
endif

SUBDIRS = 							\
	$(LIBICAL_SUBDIR)				\
	doc								\
	icons							\
	po								\
	sounds							\
	src								\
	tz_convert
	
# FIXME: plugin, panel-plugin and globaltime are not ported to GTK3

DIST_SUBDIRS = $(SUBDIRS) plugin panel-plugin globaltime

desktopdir = $(datadir)/applications
desktop_in_files = xfcalendar.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

if HAVE_DBUS
servicedir = $(datadir)/dbus-1/services
service_in_files = org.xfce.calendar.service.in org.xfce.orage.service.in
service_DATA = $(service_in_files:.service.in=.service)
%.service: %.service.in
	sed -e "s,\@bindir\@,$(bindir),g" < $< > $@
endif

EXTRA_DIST =								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	xfcalendar.spec							\
	$(desktop_in_files)						\
	$(service_in_files)

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	$(desktop_DATA)							\
	$(service_DATA)

AUTOMAKE_OPTIONS =							\
	1.8							        	\
	dist-bzip2

distclean-local:
	rm -rf *.cache *~ *.spec

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

html: Makefile
	make -C doc html

# vi:set ts=8 sw=8 noet ai nocindent:
