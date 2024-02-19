toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

gidocgen = find_program('gi-docgen')

gidocgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]

if get_option('werror')
  gidocgen_common_args += ['--fatal-warnings']
endif

docs_dir = join_paths(get_option('datadir'), 'doc')

source_toml = configure_file(
  input: 'Gom.toml.in',
  output: 'Gom.toml',
  configuration: toml_conf,
  install: true,
  install_dir: join_paths(docs_dir, 'gom-@0@'.format(lib_version)),
)

custom_target('gom-doc',
  input: [ source_toml, gom_gir[0] ],
  output: 'gom-@0@'.format(lib_version),
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  install: true,
  install_dir: docs_dir,
)
