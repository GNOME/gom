lib_test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@/data/settings'.format(meson.project_build_root()),
  'G_DEBUG=gc-friendly',
  'MALLOC_CHECK_=2',
  'LD_LIBRARY_PATH=@0@/lib'.format(meson.project_build_root()),
]

lib_sqlite_test_env = lib_test_env + [
  'GOM_MODULES_DIR=@0@/lib/sqlite'.format(meson.project_build_root()),
]

lib_pgsql_test_env = lib_test_env + [
  'GOM_MODULES_DIR=@0@/lib/pgsql'.format(meson.project_build_root()),
]

lib_testsuite_c_args = [
  '-DG_ENABLE_DEBUG',
  '-UG_DISABLE_ASSERT',
  '-UG_DISABLE_CAST_CHECKS',
]

lib_testsuite_deps = [
  libgom_static_dep,
]

if get_option('sqlite-backend').enabled()
  lib_testsuite_deps += sqlite3mc_dep
endif

testsuite_static_library = static_library(
  'testsuite',
  'test-gom-entity-type-objects.c',
  c_args: lib_testsuite_c_args,
  dependencies: lib_testsuite_deps,
  include_directories: [include_directories('..'), include_directories('.')],
)

lib_testsuite = [
  'test-gom-util',
  'test-gom',
  'test-gom-entity-type-base',
  'test-gom-entity-type-base-custom-identity',
  'test-gom-entity-type-base-inherited-metadata',
  'test-gom-entity-type-invalid-inverse-source',
  'test-gom-entity-type-invalid-inverse-target',
  'test-gom-entity-type-versioned-inverse-source',
  'test-gom-entity-type-missing-metadata',
  'test-gom-sync',
]

pgsql_test_user = run_command(find_program('id'), '-un', check: true).stdout().strip()
pgsql_test_dbname = get_option('pgsql-tests-dbname')
pgsql_test_tablespace = get_option('pgsql-tests-tablespace')
pgsql_test_uri = get_option('pgsql-tests-uri')

if pgsql_test_uri == ''
  pgsql_test_uri = 'postgresql:///@0@?user=@1@'.format(pgsql_test_dbname, pgsql_test_user)
endif

foreach test: lib_testsuite
  test_exe = executable(
    test,
    '@0@.c'.format(test),
    c_args: lib_testsuite_c_args,
    dependencies: lib_testsuite_deps,
    include_directories: [include_directories('..'), include_directories('.')],
    link_with: [testsuite_static_library],
  )
  test_env = lib_test_env
  if test == 'test-gom-sync' and get_option('sqlite-backend').enabled()
    test_env = lib_sqlite_test_env
  endif

  test(test, test_exe, env: test_env)
endforeach

if get_option('sqlite-backend').enabled()
  lib_sqlite_testsuite = {
    'test-gom-migrator': {},
    'test-gom-sqlite-encryption': {'timeout': 60},
    'test-gom-sqlite': {},
    'test-gom-relations': {},
    'test-read-only-object': {},
    'test-gom-sqlite-entity-migrator': {},
    'test-gom-sqlite-stress': {},
  }

  foreach test, params: lib_sqlite_testsuite
    test_exe = executable(test, ['@0@.c'.format(test)],
                   c_args: lib_testsuite_c_args,
             dependencies: [libgom_static_dep, sqlite3mc_dep],
      include_directories: [include_directories('..'), include_directories('.')],
    )
    if params.get('performance', false)
      if get_option('performance-tests')
        test(test, test_exe, env: lib_sqlite_test_env, suite: ['performance'], timeout: params.get('timeout', 30))
      endif
    else
      test(test, test_exe, env: lib_sqlite_test_env, timeout: params.get('timeout', 30))
    endif
  endforeach

  executable('test-manuals', ['test-manuals.c'],
    c_args: lib_testsuite_c_args,
    dependencies: [libgom_static_dep, sqlite3mc_dep],
    include_directories: [include_directories('..'), include_directories('.')],
  )

  executable('describe-sqlite', ['describe-sqlite.c'],
    c_args: lib_testsuite_c_args,
    dependencies: [libgom_static_dep, sqlite3mc_dep],
    include_directories: [include_directories('..'), include_directories('.')],
  )

  if get_option('performance-tests')
    test_gom_sqlite_stress_performance = executable(
      'test-gom-sqlite-stress-performance',
      ['test-gom-sqlite-stress.c'],
      c_args: lib_testsuite_c_args + [
        '-DSTRESS_WORKER_COUNT=8',
        '-DSTRESS_ITERATIONS=50',
      ],
      dependencies: [libgom_static_dep, sqlite3mc_dep],
      include_directories: [include_directories('..'), include_directories('.')],
    )

    test('test-gom-sqlite-stress-performance',
         test_gom_sqlite_stress_performance,
         env: lib_sqlite_test_env,
         suite: ['performance'])
  endif
endif

if pgsql_dep.found()
  test_gom_pgsql = executable('test-gom-pgsql',
                              ['test-gom-pgsql.c'],
                              c_args: lib_testsuite_c_args,
                              dependencies: [libgom_static_dep, pgsql_dep],
                              include_directories: [include_directories('..'), include_directories('.')],
  )
  test('test-gom-pgsql',
       test_gom_pgsql,
       env: lib_pgsql_test_env + [
         'GOM_PGSQL_TEST_URI=@0@'.format(pgsql_test_uri),
         'GOM_PGSQL_TEST_USER=@0@'.format(pgsql_test_user),
         'GOM_PGSQL_TEST_DATABASE=@0@'.format(pgsql_test_dbname),
         'GOM_PGSQL_TEST_TABLESPACE=@0@'.format(pgsql_test_tablespace),
       ],
       is_parallel: false)
endif
