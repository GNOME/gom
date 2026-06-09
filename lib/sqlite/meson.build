if not get_option('sqlite-backend').enabled()
  subdir_done()
endif

libgom_sqlite_module_deps = [
  glib_dep,
  dex_dep,
  gmodule_dep,
  sqlite3mc_dep,
]
if sysprof_dep.found()
  libgom_sqlite_module_deps += [sysprof_dep]
endif

libgom_sqlite_module_sources = [
  files('gom-sqlite-connection.c'),
  files('gom-sqlite-cursor.c'),
  files('gom-sqlite-driver.c'),
  files('gom-sqlite-lease.c'),
  files('gom-sqlite-pool.c'),
  files('gom-sqlite-session.c'),
  files('gom-sqlite-statement.c'),
]

libgom_sqlite_module_c_args = libgom_c_args + ['-D_GNU_SOURCE']
if sqlite_vec1_enabled
  libgom_sqlite_module_sources += files('vec1/vec1.c')
  libgom_sqlite_module_c_args += [
    '-DVEC1_STATIC',
    '-Wno-cast-function-type',
    '-Wno-declaration-after-statement',
    '-Wno-missing-declarations',
    '-Wno-missing-noreturn',
    '-Wno-nested-externs',
    '-Wno-old-style-definition',
    '-Wno-redundant-decls',
    '-Wno-shadow',
    '-Wno-strict-prototypes',
    '-Wno-switch-default',
    '-Wno-switch-enum',
    '-Wno-unused-but-set-variable',
    '-Wno-unused-function',
    '-Wno-unused-parameter',
    '-Wno-unused-variable',
  ]
endif

shared_module('gom-sqlite-module',
  libgom_sqlite_module_sources,
  link_with: libgom,
  dependencies: libgom_sqlite_module_deps,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
  c_args: libgom_sqlite_module_c_args,
  gnu_symbol_visibility: 'hidden',
  name_prefix: '',
  install: true,
  install_dir: join_paths(libdir, 'libgom-@0@'.format(api_version), 'modules'),
)
