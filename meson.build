project(
  'orage',
  'c',
  version: '4.20.1-dev',
  license: 'GPL-2.0-or-later',
  meson_version: '>= 0.54.0',
  default_options: [
    'c_std=gnu11',
    'buildtype=debugoptimized',
    'warning_level=2',
  ]
)

project_namespace = 'apps'
pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()
copyright_year = '2025'

cc = meson.get_compiler('c')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')

dependency_versions = {
  'glib': '>= 2.58.0',
  'gtk': '>= 3.24.0',
  'xfce4': '>= 4.20.0',
  'libical': '>= 3.0',
  'libnotify': '>= 0.7.0',
}

glib = dependency('glib-2.0', version: dependency_versions['glib'])
gio = dependency('gio-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['xfce4'])
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['xfce4'])
libical = dependency('libical', version: dependency_versions['libical'])
libnotify = dependency('libnotify', version: dependency_versions['libnotify'], required: get_option('libnotify'))
libm = cc.find_library('m')

if libical.version().version_compare('= 3.0.15')
  error('Unsupported libical version 3.0.15, see https://gitlab.xfce.org/apps/orage/-/issues/20')
endif

feature_cflags = []

if libnotify.found()
  feature_cflags += '-DHAVE_NOTIFY=1'
endif

if cc.has_header_symbol('langinfo.h', '_NL_TIME_FIRST_WEEKDAY')
  feature_cflags += '-DHAVE__NL_TIME_FIRST_WEEKDAY=1'
endif
if cc.has_function('setsid')
  feature_cflags += '-DHAVE_SETSID=1'
endif

headers = [
  'sys/stat.h',
  'sys/types.h',
  'sys/wait.h',
  'errno.h',
  'stdlib.h',
  'string.h',
]
foreach header : headers
  if cc.check_header(header)
    feature_cflags += '-DHAVE_@0@=1'.format(header.underscorify().to_upper())
  endif
endforeach

if get_option('archive')
  feature_cflags += '-DHAVE_ARCHIVE=1'
endif
if get_option('x11-tray-icon')
  feature_cflags += '-DHAVE_X11_TRAY_ICON=1'
endif

# TODO: Add switch or condition to enable sync code
feature_cflags += '-DENABLE_SYNC=1'

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wnested-externs',
  '-Wno-unused-parameter',
  '-Wno-declaration-after-statement',
  '-Wno-missing-field-initializers',
  '-Werror=implicit-function-declaration',
  '-Wno-error=deprecated-declarations',
]

optimization = get_option('optimization')
if get_option('debug') and optimization in ['0', 'g']
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += [
    '-DDEBUG=1',
    '-DDEBUG_TRACE=1',
    '-DG_ENABLE_DEBUG',
  ]
elif optimization in ['3', 'minsize']
  extra_cflags += [
    '-DNDEBUG',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ]
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
    '-DG_LOG_DOMAIN="@0@"'.format(meson.project_name()),
    '-DG_LOG_USE_STRUCTURED=1',
  ]
endif

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DCOPYRIGHT_YEAR="@0@"'.format(copyright_year),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_URL="https://docs.xfce.org/@0@/@1@/start"'.format(project_namespace, meson.project_name()),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DDOCDIR="@0@"'.format(get_option('prefix') / 'doc' / meson.project_name()),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(feature_cflags, language: 'c')
add_project_arguments(extra_cflags, language: 'c')

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

i18n.merge_file(
  input: 'org.xfce.orage.appdata.xml.in',
  output: 'org.xfce.orage.appdata.xml',
  po_dir: 'po',
  type: 'xml',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'metainfo',
)

i18n.merge_file(
  input: 'org.xfce.orage.desktop.in',
  output: 'org.xfce.orage.desktop',
  po_dir: 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)

configure_file(
  configuration: configuration_data({
    'bindir': get_option('prefix') / get_option('bindir'),
  }),
  input: 'org.xfce.orage.service.in',
  output: 'org.xfce.orage.service',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'dbus-1' / 'services'
)

subdir('src')
subdir('icons')
subdir('plugin')
subdir('sounds')
subdir('themes')
subdir('po')
