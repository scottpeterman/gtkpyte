project('gtkpyte', ['c', 'cpp'],
  version : '0.0.1',
  license : 'GPL-3.0-or-later',
  meson_version : '>= 1.0.0',
  default_options : [
    'cpp_std=c++17',
    'warning_level=2',
    'buildtype=debugoptimized',
  ],
)

# utf8proc pinned to v2.11.3 so the East Asian width table matches the other
# pyte builds cell-for-cell. Debian's libutf8proc-dev is older.
utf8proc_dep = subproject('utf8proc').get_variable('utf8proc_dep')

subdir('pyte')

gtk_dep = dependency('gtk4', version : '>= 4.8')

executable('gtkpyte-envcheck',
  'src/envcheck.cpp',
  dependencies : [pyte_dep, gtk_dep],
)
