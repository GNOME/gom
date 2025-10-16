tests = [
  'test-gom-adapter',
  'test-gom-constraints',
  'test-gom-find-specific',
  'test-gom-insert',
  'test-gom-migration',
  'test-gom-repository',
  'test-gom-sorting',
  'test-gom-table-name',
  'test-gom-update',
  'test-gom-datetime',
]

foreach test_name: tests
  exe = executable(test_name, '@0@.c'.format(test_name),
    c_args: [
      '-DG_ENABLE_DEBUG',
      '-UG_DISABLE_ASSERT',
      '-UG_DISABLE_CAST_CHECKS',
    ],
    dependencies: [libgom_dep])
  test(test_name, exe)
endforeach

# Some more tests requiring special handling

test_find = executable('test-gom-find', 'test-gom-find.c',
  c_args: '-DDB="@0@/grl-bookmarks.db"'.format(meson.current_source_dir()),
  dependencies: [libgom_dep])
test('test-gom-find', test_find)

test_stress = executable('test-gom-stress', 'test-gom-stress.c',
  dependencies: [libgom_dep])
test('test-gom-stress', test_stress, timeout: 300)

test_transform = executable('test-gom-transform', 'test-gom-transform.c',
  c_args: '-DIMAGE="@0@/gnome.png"'.format(meson.current_source_dir()),
  dependencies: [libgom_dep, gdkpixbuf_dep])
test('test-gom-transform', test_transform)
