# ============================== #
#  Draconis++ Monorepo          #
# ============================== #

project('draconisplusplus', 'cpp', 'c',
  version: '0.1.0',
  meson_version: '>=1.1',
  default_options: [
    'default_library=static',
    'buildtype=debugoptimized',
    'b_vscrt=mt',
    'b_ndebug=if-release',
    'warning_level=3',
  ],
)

cpp = meson.get_compiler('cpp')
host_system = host_machine.system()
fs = import('fs')

# ============================== #
#  Subdirectories                #
# ============================== #

# Core library (always built)
subdir('core')

# C API (stable ABI layer, always built)
subdir('c-api')

# Plugins - only for dynamic plugin builds (static plugins handled in core)
# For static plugins, the core library compiles them directly
if get_option('plugins').allowed() and get_option('static_plugins').length() == 0
  subdir('plugins')
endif

# ============================== #
#  Bindings                      #
# ============================== #

if get_option('build_rust')
  subdir('bindings/rust')
endif

if get_option('build_python')
  subdir('bindings/python')
endif

if get_option('build_lua')
  subdir('bindings/lua')
endif

if get_option('build_kotlin')
  subdir('bindings/kotlin')
endif

if get_option('build_c3')
  subdir('bindings/c3')
endif

if get_option('build_csharp')
  subdir('bindings/csharp')
endif

# ============================== #
#  Summary                       #
# ============================== #

summary({
  'Core library': true,
  'C API': true,
  'Plugins': get_option('plugins').allowed() or get_option('static_plugins').length() > 0,
  'CLI': get_option('build_cli'),
  'Examples': get_option('build_examples'),
  'Tests': get_option('build_tests'),
}, section: 'Components', bool_yn: true)

summary({
  'Rust': get_option('build_rust'),
  'Python': get_option('build_python'),
  'Lua': get_option('build_lua'),
  'Kotlin': get_option('build_kotlin'),
  'C3': get_option('build_c3'),
  'C#': get_option('build_csharp'),
}, section: 'Bindings', bool_yn: true)

summary({
  'C++ Compiler': cpp.get_id(),
  'Host system': host_system,
  'Build type': get_option('buildtype'),
  'Install prefix': get_option('prefix'),
}, section: 'System')
