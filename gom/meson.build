gom_sources = [
  'gom-adapter.c',
  'gom-command-builder.c',
  'gom-command.c',
  'gom-cursor.c',
  'gom-error.c',
  'gom-filter.c',
  'gom-repository.c',
  'gom-resource.c',
  'gom-resource-group.c',
  'gom-sorting.c',
]

gom_headers = [
  'gom.h',
  'gom-adapter.h',
  'gom-command-builder.h',
  'gom-command.h',
  'gom-cursor.h',
  'gom-error.h',
  'gom-filter.h',
  'gom-repository.h',
  'gom-resource-group.h',
  'gom-resource.h',
  'gom-autocleanups.h',
  'gom-sorting.h',
]

gom_private_headers = [
  'gom-resource-priv.h',
  'reserved-keywords.h',
]

install_headers(gom_headers,
  subdir: '@0@/@1@'.format(lib_name, meson.project_name()))

libgom_c_args = [
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(glib2_major_version, glib2_minor_version),
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_@0@_@1@'.format(glib2_major_version, glib2_minor_version),
]

libgom = shared_library(lib_name,
  sources: gom_sources + gom_private_headers,
  install: true,
  c_args: libgom_c_args,
  soversion: soversion,
  version: lt_version,
  dependencies: [gio_dep, glib_dep, gobject_dep, sqlite3_dep])

libgom_dep = declare_dependency(
  dependencies: [gio_dep, glib_dep, gobject_dep, sqlite3_dep],
  include_directories: include_directories('..'),
  link_with: libgom)

if enable_gir
  gom_gir = gnome.generate_gir(libgom,
              sources: gom_headers + gom_sources,
            namespace: 'Gom',
            nsversion: lib_version,
      export_packages: '@0@-@1@'.format(meson.project_name(), lib_version),
        symbol_prefix: 'gom',
    identifier_prefix: 'Gom',
         dependencies: [gio_dep, gobject_dep],
             includes: ['Gio-2.0'],
              install: true,
               header: 'gom.h')
endif
