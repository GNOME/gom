project('libgom', 'c',
          version: '2.0.alpha',
    meson_version: '>= 1.1.0',
  default_options: ['warning_level=2', 'werror=false'],
          license: 'LGPL-2.1-or-later',
)

api_version = '2'

cc = meson.get_compiler('c')
gnome = import('gnome')
pkg = import('pkgconfig')

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
libdir = join_paths(prefix, get_option('libdir'))
gom_modules_dir = join_paths(libdir, 'libgom-@0@'.format(api_version), 'modules')
girdir = join_paths(datadir, 'gir-1.0')
vapidir = join_paths(datadir, 'vala', 'vapi')
typelibdir = join_paths(libdir, 'girepository-1.0')
config_h.set_quoted('GOM_MODULES_DIR_DEFAULT', gom_modules_dir)

glib_req_version = '2.84'
glib_req = '>= @0@'.format(glib_req_version)
glib_dep = dependency('gio-2.0', version: glib_req)
gmodule_dep = dependency('gmodule-2.0', version: glib_req)
libm_dep = cc.find_library('m', required: false)
sysprof_dep = dependency('sysprof-capture-4', required: get_option('sysprof'))
config_h.set10('HAVE_SYSPROF', sysprof_dep.found())

dex_req_version = '1.2'
dex_req = '>= @0@'.format(dex_req_version)
dex_dep = dependency('libdex-1', version: dex_req)

if get_option('sqlite-backend').enabled()
  sqlite3mc_sp = subproject('sqlite3mc')
  sqlite3mc_dep = sqlite3mc_sp.get_variable('sqlite3mc_dep')
endif
if get_option('sqlite-vec1').enabled() and not get_option('sqlite-backend').enabled()
  error('sqlite-vec1 requires sqlite-backend')
endif
sqlite_vec1_enabled = get_option('sqlite-backend').enabled() and not get_option('sqlite-vec1').disabled()
config_h.set10('HAVE_SQLITE_VEC1', sqlite_vec1_enabled)

pgsql_dep = dependency('pgsql-glib-1', required: get_option('postgresql-backend'))

project_c_args = []
test_c_args = [
  '-Watomic-alignment',
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=redundant-decls',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-dangling-pointer',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wtrampolines',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fstrict-flex-arrays=3',
  '-fno-strict-aliasing',
  ['-Werror=format-security', '-Werror=format=2'],
  '-FImsvc_recommended_pragmas.h',
]
if get_option('buildtype') != 'plain' and get_option('stack-protector')
  if host_machine.system() != 'windows'
    test_c_args += '-fstack-protector-strong'
  endif
endif
foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    project_c_args += arg
  endif
endforeach
add_project_arguments(project_c_args, language: 'c')

# Detect and set symbol visibility
if get_option('default_library') != 'static'
  if host_machine.system() == 'windows'
    config_h.set('DLL_EXPORT', true)
    if cc.get_id() == 'msvc'
      config_h.set('_GOM_EXTERN', '__declspec(dllexport) extern')
    elif cc.has_argument('-fvisibility=hidden')
      config_h.set('_GOM_EXTERN', '__attribute__((visibility("default"))) __declspec(dllexport) extern')
    endif
  elif cc.has_argument('-fvisibility=hidden')
    config_h.set('_GOM_EXTERN', '__attribute__((visibility("default"))) extern')
  endif
endif

release_args = []
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
  '-Wl,-z,noexecstack',
]
if not get_option('buildtype').startswith('debug')
  release_args += ['-DG_DISABLE_ASSERT', '-DG_DISABLE_CAST_CHECKS']
  test_link_args += [
    '-Wl,-Bsymbolic',
    '-fno-plt',
  ]
endif
foreach link_arg: test_link_args
  if cc.has_link_argument(link_arg)
    global_link_args += link_arg
  endif
endforeach
add_project_link_arguments(global_link_args, language: 'c')

glib_major_version = glib_req_version.split('.')[0].to_int()
glib_minor_version = glib_req_version.split('.')[1].to_int()
if glib_minor_version % 2 == 1
  glib_minor_version = glib_minor_version + 1
endif

dex_major_version = dex_req_version.split('.')[0].to_int()
dex_minor_version = dex_req_version.split('.')[1].to_int()

deprecated_c_args = [
  '-DG_DISABLE_DEPRECATED',
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(glib_major_version, glib_minor_version),
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_@0@_@1@'.format(glib_major_version, glib_minor_version),
  '-DDEX_VERSION_MIN_REQUIRED=DEX_VERSION_@0@_@1@'.format(dex_major_version, dex_minor_version),
  '-DDEX_VERSION_MAX_ALLOWED=DEX_VERSION_@0@_@1@'.format(dex_major_version, dex_minor_version),
]

add_project_arguments('-I' + meson.project_build_root(), language: 'c')

subdir('lib')
if get_option('tests')
  subdir('testsuite')
endif
if get_option('demos')
  subdir('demo')
endif
if get_option('docs')
  subdir('doc')
endif

configure_file(output: 'config.h', configuration: config_h)
