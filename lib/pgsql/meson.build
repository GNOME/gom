if not pgsql_dep.found()
  subdir_done()
endif

libgom_pgsql_module_deps = [
  glib_dep,
  dex_dep,
  gmodule_dep,
  pgsql_dep,
]
if sysprof_dep.found()
  libgom_pgsql_module_deps += [sysprof_dep]
endif

libgom_pgsql_module_sources = [
  files('gom-pgsql-cursor.c'),
  files('gom-pgsql-driver.c'),
  files('gom-pgsql-session.c'),
]

shared_module('gom-pgsql-module',
  libgom_pgsql_module_sources,
  link_with: libgom,
  dependencies: libgom_pgsql_module_deps,
  include_directories: [
    include_directories('.'),
    include_directories('..'),
  ],
  c_args: libgom_c_args,
  gnu_symbol_visibility: 'hidden',
  name_prefix: '',
  install: true,
  install_dir: join_paths(libdir, 'libgom-@0@'.format(api_version), 'modules'),
)
