# pyte/meson.build -- the emulation core. No GTK here: this directory builds
# and tests on its own, and the widget layer only sees pyte_dep.

pyte_inc = include_directories('include')

pyte_lib = static_library('pyte_core',
  'src/rowstore.cpp',
  'src/screen.cpp',
  'src/stream.cpp',
  'src/unicode.cpp',
  'src/screen_range.cpp',
  include_directories : pyte_inc,
  dependencies : utf8proc_dep,
)

pyte_dep = declare_dependency(
  link_with : pyte_lib,
  include_directories : pyte_inc,
)

ptdump = executable('ptdump', 'tools/ptdump/main.cpp',
  dependencies : pyte_dep,
)

if get_option('tests')
  doctest_dep = subproject('doctest').get_variable('doctest_dep')
  pyte_tests = executable('pyte_tests',
    'tests/main.cpp',
    'tests/test_altscreen.cpp',
    'tests/test_history.cpp',
    'tests/test_screen.cpp',
    'tests/test_stream.cpp',
    'tests/test_range.cpp',
    dependencies : [pyte_dep, doctest_dep],
  )
  test('pyte_tests', pyte_tests)
endif
