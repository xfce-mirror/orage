desktopdir = $(datadir)/applications
desktop_in_files = org.xfce.orage-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

org.xfce.orage-settings.desktop: org.xfce.orage-settings.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = 								\
	$(desktop_in_files)						\
	meson.build							\
	$(NULL)

DISTCLEANFILES =							\
	$(desktop_DATA)

