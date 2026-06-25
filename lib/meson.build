libgom_sources = [
  'gom-delta.c',
  'gom-util.c',
  'gom-cursor.c',
  'gom-custom-migration.c',
  'gom-custom-migrator.c',
  'gom-entity-list-item.c',
  'gom-entity-list-model.c',
  'gom-deletion.c',
  'gom-deletion-builder.c',
  'gom-driver.c',
  'gom-driver-options.c',
  'gom-error.c',
  'gom-entity-migrator.c',
  'gom-entity.c',
  'gom-expression.c',
  'gom-insertion.c',
  'gom-insertion-builder.c',
  'gom-migration.c',
  'gom-meta.c',
  'gom-migrator.c',
  'gom-mutation.c',
  'gom-mutation-result.c',
  'gom-nested-migration.c',
  'gom-related-model.c',
  'gom-ordering.c',
  'gom-query.c',
  'gom-query-builder.c',
  'gom-query-model.c',
  'gom-repository.c',
  'gom-record.c',
  'gom-record-list-item.c',
  'gom-record-list-model.c',
  'gom-session.c',
  'gom-schema.c',
  'gom-sql-migration.c',
  'gom-update.c',
  'gom-update-builder.c',
  'gom-sync-coordinator.c',
  'gom-sync-transport.c',
  'gom-merge-decision.c',
  'gom-merge-policy.c',
  'gom-value.c',
  'gom-vector.c',
]

libgom_private_sources = [
  'gom-sync-history.c',
  'gom-tombstone.c',
  'gom-registry-diff.c',
  'gom-meta-version.c',
  'gom-mock-driver.c',
  'gom-trace.c',
]

libgom_headers = [
  'gom-delta.h',
  'gom-cursor.h',
  'gom-custom-migration.h',
  'gom-custom-migrator.h',
  'gom-entity-list-item.h',
  'gom-entity-list-model.h',
  'gom-deletion.h',
  'gom-deletion-builder.h',
  'gom-driver.h',
  'gom-driver-options.h',
  'gom-entity-migrator.h',
  'gom-entity.h',
  'gom-expression.h',
  'gom-insertion.h',
  'gom-insertion-builder.h',
  'gom-migration.h',
  'gom-meta.h',
  'gom-migrator.h',
  'gom-mutation.h',
  'gom-mutation-result.h',
  'gom-nested-migration.h',
  'gom-related-model.h',
  'gom-ordering.h',
  'gom-query.h',
  'gom-query-builder.h',
  'gom-query-model.h',
  'gom-repository.h',
  'gom-record.h',
  'gom-record-list-item.h',
  'gom-record-list-model.h',
  'gom-session.h',
  'gom-schema.h',
  'gom-sql-migration.h',
  'gom-sync-coordinator.h',
  'gom-sync-transport.h',
  'gom-merge-decision.h',
  'gom-merge-policy.h',
  'gom-types.h',
  'gom-update.h',
  'gom-update-builder.h',
  'gom-vector.h',
  'gom-version-macros.h',
  'libgom.h',
]

libgom_deps = [
  glib_dep,
  dex_dep,
  gmodule_dep,
  libm_dep,
]
if sysprof_dep.found()
  libgom_deps += [sysprof_dep]
endif

libgom_c_args = [
  deprecated_c_args,
  release_args,
  '-DG_LOG_DOMAIN="Gom"',
  '-DGOM_COMPILATION',
]

version_split = meson.project_version().split('.')
micro_version = version_split[2]
if version_split[2][0] in ['a', 'b', 'r']
  micro_version = '0'
endif
version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', version_split[0])
version_conf.set('MINOR_VERSION', version_split[1])
version_conf.set('MICRO_VERSION', micro_version)

gom_config_conf = configuration_data()
if get_option('sqlite-backend').enabled()
  gom_config_conf.set('GOM_DATABASE_SQLITE', 1)
endif
if sqlite_vec1_enabled
  gom_config_conf.set('GOM_DATABASE_SQLITE_VEC1', 1)
endif
if pgsql_dep.found()
  gom_config_conf.set('GOM_DATABASE_POSTGRESQL', 1)
endif

configure_file(
         input: 'gom-config.h.in',
        output: 'gom-config.h',
  configuration: gom_config_conf,
        install: true,
    install_dir: join_paths(get_option('includedir'), 'libgom-@0@'.format(api_version))
)

configure_file(
          input: 'gom-version.h.in',
         output: 'gom-version.h',
  configuration: version_conf,
        install: true,
    install_dir: join_paths(get_option('includedir'), 'libgom-@0@'.format(api_version))
)

libgom_static = static_library('gom-@0@-static'.format(api_version),
                    libgom_sources + libgom_private_sources,
                    dependencies: libgom_deps,
                          c_args: libgom_c_args,
                           pic: true,
                        install: false)

libgom = library('gom-@0@'.format(api_version),
           dependencies: libgom_deps,
                 c_args: libgom_c_args,
              link_whole: libgom_static,
                install: true,
                version: '@0@.0.0'.format(api_version),
        darwin_versions: '@0@.0'.format(api_version),
)

if get_option('sqlite-backend').enabled()
  subdir('sqlite')
endif

if pgsql_dep.found()
  subdir('pgsql')
endif

if get_option('default_library') == 'static'
  if get_option('introspection').enabled()
    error('introspection requires building a shared library!')
  endif
endif

libgom_dep_sources = []

install_headers(libgom_headers, subdir: 'libgom-@0@'.format(api_version))

libgom_pkg_requires = [
  'gio-2.0',
  'libdex-1',
]
libgom_vapi_packages = libgom_pkg_requires
if host_machine.system() != 'windows'
  libgom_vapi_packages += ['gio-unix-2.0']
endif

pkg.generate(
  description: 'Future-based programming for GLib-based applications and libraries',
    libraries: libgom,
         name: 'libgom',
     filebase: 'libgom-' + api_version,
      subdirs: 'libgom-@0@'.format(api_version),
     requires: libgom_pkg_requires,
)

if get_option('introspection').enabled()
  libgom_gir = gnome.generate_gir(libgom,
                sources: [libgom_sources, libgom_headers],
              nsversion: api_version,
              namespace: 'Gom',
        export_packages: 'libgom-@0@'.format(api_version),
          symbol_prefix: 'gom',
      identifier_prefix: 'Gom',
        install_dir_gir: girdir,
    install_dir_typelib: typelibdir,
               includes: ['Gio-2.0', 'Dex-1'],
                install: true,
                 header: 'libgom.h',
             extra_args: ['-DGOM_COMPILATION'],
  )

  libgom_dep_sources += [libgom_gir]

  if get_option('vapi')
    gom_vapi = gnome.generate_vapi('libgom-@0@'.format(api_version),
            sources: libgom_gir.get(0),
            install: true,
        install_dir: join_paths(datadir, 'vala', 'vapi'),
           packages: libgom_vapi_packages,
    )
  endif

  if get_option('pygobject')
    python3 = import('python').find_installation('python3')

    pygobject_override_dir = get_option('pygobject-override-dir')
    pygobject_override_files = files('Gom.py')

    if pygobject_override_dir == ''
      pygobject_override_dir = 'gi/overrides'
    else
      # Emulate install_data behavior
      pygobject_override_dir = join_paths(get_option('prefix'), pygobject_override_dir)
    endif

    python3.install_sources(pygobject_override_files,
          pure: true,
        subdir: pygobject_override_dir)
  endif
endif

libgom_static_dep = declare_dependency(
            link_with: libgom_static,
  include_directories: include_directories('.'),
         dependencies: libgom_deps,
              sources: [libgom_dep_sources],
)

libgom_dep = declare_dependency(
            link_with: libgom,
  include_directories: include_directories('.'),
         dependencies: libgom_deps,
              sources: [libgom_dep_sources],
)

meson.override_dependency('libgom-' + api_version, libgom_dep)
