project('gom', 'c',
  version: '0.4',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.48')

version = meson.project_version().split('.')
soversion = 0
current = 1
age = 0
lib_version = '@0@.@1@'.format(1, 0)
lib_name = '@0@-@1@'.format(meson.project_name(), lib_version)
lt_version = '@0@.@1@.@2@'.format(soversion, current, age)

gnome = import('gnome')
pkgconfig = import('pkgconfig')

enable_gir = get_option('enable-introspection')
enable_gtkdoc = get_option('enable-gtk-doc')

glib2_required = '>= 2.38'

gio_dep = dependency('gio-2.0', version: glib2_required)
glib_dep = dependency('glib-2.0', version: glib2_required)
gobject_dep = dependency('gobject-2.0', version: glib2_required)
sqlite3_dep = dependency('sqlite3', version: '>= 3.7')
gdkpixbuf_dep = dependency('gdk-pixbuf-2.0')

subdir('gom')
subdir('tests')

if enable_gir
  subdir('bindings')
endif

if enable_gtkdoc
  subdir('doc')
endif

pkgconfig.generate(
  name: meson.project_name(),
  description: 'GObject Data Mapper',
  version: meson.project_version(),
  filebase: lib_name,
  libraries: [libgom],
  requires: 'gobject-2.0',
  subdirs: lib_name)
