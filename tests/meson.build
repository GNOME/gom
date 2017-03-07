test_adapter = executable('test-gom-adapter', 'test-gom-adapter.c',
  dependencies: [libgom_dep])
test('test-gom-adapter', test_adapter)

test_constraints = executable('test-gom-constraints', 'test-gom-constraints.c',
  dependencies: [libgom_dep])
test('test-gom-constraints', test_constraints)

test_find = executable('test-gom-find', 'test-gom-find.c',
  c_args: '-DDB="@0@/grl-bookmarks.db"'.format(meson.current_source_dir()),
  dependencies: [libgom_dep])
test('test-gom-find', test_find)

test_find_specific = executable('test-gom-find-specific', 'test-gom-find-specific.c',
  dependencies: [libgom_dep])
test('test-gom-find-specific', test_find_specific)

test_insert = executable('test-gom-insert', 'test-gom-insert.c',
  dependencies: [libgom_dep])
test('test-gom-insert', test_insert)

test_migration = executable('test-gom-migration', 'test-gom-migration.c',
  dependencies: [libgom_dep])
test('test-gom-migration', test_migration)

test_repository = executable('test-gom-repository', 'test-gom-repository.c',
  dependencies: [libgom_dep])
test('test-gom-repository', test_repository)

test_sorting = executable('test-gom-sorting', 'test-gom-sorting.c',
  dependencies: [libgom_dep])
test('test-gom-sorting', test_sorting)

test_stress = executable('test-gom-stress', 'test-gom-stress.c',
  dependencies: [libgom_dep])
test('test-gom-stress', test_stress, timeout: 300)

test_transform = executable('test-gom-transform', 'test-gom-transform.c',
  c_args: '-DIMAGE="@0@/gnome.png"'.format(meson.current_source_dir()),
  dependencies: [libgom_dep, gdkpixbuf_dep])
test('test-gom-transform', test_transform)

test_update = executable('test-gom-update', 'test-gom-update.c',
  dependencies: [libgom_dep])
test('test-gom-update', test_update)
