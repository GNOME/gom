pygobject_override_dir = get_option('pygobject-override-dir')

get_overridedir = '''
import os
import sysconfig

libdir = sysconfig.get_config_var('SCRIPTDIR')

if not libdir:
  libdir = '/usr/lib'

try:
  import gi
  overridedir = gi._overridesdir
except ImportError:
  purelibdir = sysconfig.get_path('purelib')
  overridedir = os.path.join(purelibdir, 'gi', 'overrides')

if overridedir.startswith(libdir): # Should always be True..
  overridedir = overridedir[len(libdir) + 1:]

print(overridedir)
'''

if pygobject_override_dir == ''
    python3 = import('python3').find_python()

    ret = run_command([python3, '-c', get_overridedir])

    if ret.returncode() != 0
        error('Failed to determine pygobject override directory')
    else
        pygobject_override_dir = join_paths(get_option('libdir'), ret.stdout().strip())
    endif

endif

install_data('gi/overrides/Gom.py', install_dir: pygobject_override_dir)
