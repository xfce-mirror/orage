if HAVE_LIBPOPT
bin_PROGRAMS = tz_convert
endif

tz_convert_SOURCES =						\
	tz_convert.c

tz_convert_CFLAGS =						\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"				\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	$(GLIB_CFLAGS)				\
	$(LIBPOPT_CFLAGS)

tz_convert_LDADD =						\
	$(INTLLIBS) \
	$(GLIB_LIBS) \
	$(LIBPOPT_LIBS)

# vi:set ts=8 sw=8 noet ai:
