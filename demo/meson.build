if not get_option('sqlite-backend').enabled()
  error('demos require sqlite-backend')
endif

gtk_dep = dependency('gtk4', version: '>= 4.20')

gom_demo_resources = gnome.compile_resources('gom-demo-resources',
  'gom-demo.gresource.xml',
  c_name: 'gom_demo',
)

executable('gom-demo',
  [
    'gom-demo.c',
    'gom-demo-view.c',
    'gom-demo-view.h',
    'gom-demo-view-cell.c',
    'gom-demo-view-cell.h',
    gom_demo_resources,
  ],
  c_args: [
  ],
  dependencies: [libgom_dep, gtk_dep],
  include_directories: [include_directories('..')],
  install: false,
)
