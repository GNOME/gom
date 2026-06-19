project('sqlite3mc', 'c',
  version: '2.3.4',
  meson_version: '>= 1.1.0',
  default_options: [
    'warning_level=0',
    'werror=false',
  ],
)

cc = meson.get_compiler('c')

sqlite3mc_include = include_directories(
  'src',
  'src/aegis/include',
  'src/argon2/include',
)

sqlite3mc_system_include = include_directories(
  'src',
  'src/aegis/include',
  'src/argon2/include',
  is_system: true,
)

sqlite3mc_c_args = [
  '-D_GNU_SOURCE',
  '-DCODEC_TYPE=CODEC_TYPE_SQLCIPHER',
  '-DSQLITE_THREADSAFE=1',
  '-DSQLITE_DQS=0',
  '-DSQLITE_MAX_ATTACHED=10',
  '-DSQLITE_SOUNDEX=1',
  '-DSQLITE_ENABLE_COLUMN_METADATA=1',
  '-DSQLITE_SECURE_DELETE=1',
  '-DSQLITE_ENABLE_DESERIALIZE=1',
  '-DSQLITE_ENABLE_FTS3=1',
  '-DSQLITE_ENABLE_FTS3_PARENTHESIS=1',
  '-DSQLITE_ENABLE_FTS4=1',
  '-DSQLITE_ENABLE_FTS5=1',
  '-DSQLITE_ENABLE_RTREE=1',
  '-DSQLITE_ENABLE_MATH_FUNCTIONS=1',
  '-DSQLITE_TEMP_STORE=2',
  '-DSQLITE_USE_URI=1',
  '-DSQLITE_USER_AUTHENTICATION=0',
  '-DSQLITE_CORE=1',
  '-DSQLITE_OS_WIN=0',
]

sqlite3mc_deps = []
libm_dep = cc.find_library('m', required: false)
if libm_dep.found()
  sqlite3mc_deps += libm_dep
endif

sqlite3mc_lib = static_library('sqlite3mc',
  files('src/sqlite3mc.c'),
  include_directories: sqlite3mc_include,
  c_args: sqlite3mc_c_args,
  dependencies: sqlite3mc_deps,
  gnu_symbol_visibility: 'hidden',
  install: false,
)

sqlite3mc_dep = declare_dependency(
  include_directories: sqlite3mc_system_include,
  compile_args: ['-DSQLITE_OS_WIN=0'],
  link_with: sqlite3mc_lib,
  dependencies: sqlite3mc_deps,
)
