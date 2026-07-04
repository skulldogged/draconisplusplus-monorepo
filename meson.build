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
#  Plugin Discovery              #
# ============================== #

# Plugins are self-contained directories containing a plugin.json manifest.
# The core repository does not vendor official plugin implementations; pass
# external plugin roots via -Dplugin_dirs=. A local plugins/ checkout is also
# honored for development convenience. tools/plugin_helper.py is the single
# source of truth for manifest parsing; see plugins/README.md for the schema.
python_prog = find_program('python3', 'python')
plugin_helper = meson.project_source_root() / 'tools' / 'plugin_helper.py'

plugin_search_dirs = [meson.project_source_root() / 'plugins']
foreach extra_dir : get_option('plugin_dirs')
  if fs.is_absolute(extra_dir)
    plugin_search_dirs += extra_dir
  else
    plugin_search_dirs += meson.project_source_root() / extra_dir
  endif
endforeach

# Map of plugin name -> absolute plugin directory
discovered_plugins = {}
plugin_discovery = run_command(python_prog, plugin_helper, 'discover', plugin_search_dirs, check: true)
discovery_stdout = plugin_discovery.stdout().strip()
if discovery_stdout != ''
  foreach line : discovery_stdout.split('\n')
    parts = line.split('|')
    discovered_plugins += {parts[0].strip(): parts[1].strip()}
  endforeach
endif

# Resolve the requested static plugins ('all' expands to every discovered plugin)
static_plugin_list = []
if get_option('static_plugins').contains('all')
  foreach plugin_name, plugin_dir : discovered_plugins
    static_plugin_list += plugin_name
  endforeach
else
  foreach plugin_name : get_option('static_plugins')
    if plugin_name not in discovered_plugins
      error(
        'Static plugin "@0@" was not found. Discovered plugins: @1@.\nPass the parent directory of the plugin checkout via -Dplugin_dirs=, or place a local checkout under plugins/.'.format(
          plugin_name,
          ', '.join(discovered_plugins.keys()),
        ),
      )
    endif
    static_plugin_list += plugin_name
  endforeach
endif

if get_option('plugins').disabled() and static_plugin_list.length() > 0
  error('-Dstatic_plugins= requires the plugin system; remove -Dplugins=disabled.')
endif

# ============================== #
#  Subdirectories                #
# ============================== #

# Core library (always built; compiles static plugins directly)
subdir('core')

# C API (stable ABI layer, always built)
subdir('c-api')

# Dynamic plugin builds (static plugins are compiled into core instead)
if get_option('plugins').allowed() and static_plugin_list.length() == 0
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
  'Plugins': get_option('plugins').allowed() or static_plugin_list.length() > 0,
  'CLI': get_option('build_cli'),
  'Examples': get_option('build_examples'),
  'Tests': get_option('build_tests'),
}, section: 'Components', bool_yn: true)

summary({
  'Discovered': ', '.join(discovered_plugins.keys()),
  'Static': static_plugin_list.length() > 0 ? ', '.join(static_plugin_list) : '(none)',
}, section: 'Plugins')

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
