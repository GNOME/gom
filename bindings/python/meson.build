python3 = import('python').find_installation('python3')

pygobject_override_dir = get_option('pygobject-override-dir')
pygobject_override_files = files('gi/overrides/Gom.py')

if pygobject_override_dir == ''
  pygobject_override_dir = 'gi/overrides'
else
  # Emulate install_data behavior for backwards compatibility
  pygobject_override_dir = join_paths(get_option('prefix'), pygobject_override_dir)
endif

python3.install_sources(
  pygobject_override_files,
  pure: true,
  subdir: pygobject_override_dir)
