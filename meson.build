# Summary.
project('solo', 'c',
  version: '1.0',
  license: 'GPLv3',
  default_options: [
    'c_std=c11',
    'buildtype=release',
    'warning_level=3',
    'werror=true'])

flags = ['-Wshadow']
add_project_arguments(flags, language: 'c')

executable('solo', 'solo.c', [], install: true)
