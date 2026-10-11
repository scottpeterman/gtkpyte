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
util_dep = meson.get_compiler('cpp').find_library('util', required : false)  # forkpty

# The widget and the pty plumbing, as one library the app and tests share.
gtkpyte_lib = static_library('gtkpyte',
  'src/gtkpyte-terminal.cpp',
  'src/keymap.cpp',
  'src/palette.cpp',
  'src/pty.cpp',
  'src/ptysession.cpp',
  dependencies : [pyte_dep, gtk_dep, util_dep],
)

gtkpyte_dep = declare_dependency(
  link_with : gtkpyte_lib,
  include_directories : include_directories('src'),
  dependencies : [pyte_dep, gtk_dep, util_dep],
)

executable('gtkpyte',
  'src/main.cpp',
  dependencies : gtkpyte_dep,
  install : true,
)

executable('gtkpyte-envcheck',
  'src/envcheck.cpp',
  dependencies : [pyte_dep, gtk_dep],
)

if get_option('tests')
  gtkpyte_tests = executable('gtkpyte_tests',
    'tests/main.cpp',
    'tests/test_keymap.cpp',
    'tests/test_pty.cpp',
    dependencies : [gtkpyte_dep, doctest_dep],
  )
  test('gtkpyte_tests', gtkpyte_tests)
endif

# Exec gets the absolute path: launchers (the ChromeOS shelf included) do not
# reliably start apps with ~/.local/bin on PATH.
desktop_conf = configuration_data()
desktop_conf.set('bindir', get_option('prefix') / get_option('bindir'))
configure_file(
  input : 'data/io.github.scottpeterman.gtkpyte.desktop.in',
  output : 'io.github.scottpeterman.gtkpyte.desktop',
  configuration : desktop_conf,
  install : true,
  install_dir : get_option('datadir') / 'applications',
)
