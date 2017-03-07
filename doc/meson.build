vdata = configuration_data()
vdata.set('VERSION', meson.project_version())
version_xml = configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration : vdata)


gnome.gtkdoc('gom',
  main_sgml: 'gom-docs.sgml',
  src_dir: [
    join_paths(meson.source_root(), 'gom'),
    join_paths(meson.build_root(), 'gom'),
  ],
  dependencies: libgom_dep,
  gobject_typesfile: 'gom.types',
  scan_args: ['--rebuild-types'],
  install: true)
