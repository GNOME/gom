if not get_option('introspection').allowed()
  error('API reference requires introspection.')
endif

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

docs_dir = get_option('datadir') / 'doc'

source_toml = configure_file(
  input: 'libgom.toml.in',
  output: 'libgom.toml',
  configuration: toml_conf,
  install: true,
  install_dir: docs_dir / 'libgom-@0@'.format(api_version),
)

custom_target('libgom-doc',
  input: [ source_toml, libgom_gir[0] ],
  output: 'libgom-@0@'.format(api_version),
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
