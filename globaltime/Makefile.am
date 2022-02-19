bin_PROGRAMS = globaltime

globaltime_SOURCES =							\
	globaltime.c							\
	globaltime.h							\
	gt_parfile.c							\
	gt_prefs.c							\
	timezone_selection.c						\
	timezone_selection.h

desktopdir = $(datadir)/applications
desktop_in_files = org.xfce.orage.globaltime.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

globaltime_CPPFLAGS =							\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"				\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\"

globaltime_CFLAGS =							\
	$(LIBGTK_CFLAGS)						\
	$(LIBX11_CFLAGS)

globaltime_LDADD =							\
	$(LIBGTK_LIBS)							\
	$(LIBX11_LIBS)							\
	$(INTLLIBS)							\
	$(top_builddir)/src/liborage.la

EXTRA_DIST =								\
	$(desktop_in_files)

DISTCLEANFILES = $(desktop_DATA)

# vi:set ts=8 sw=8 noet ai:
