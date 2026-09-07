# ============ #
#  Unit Tests  #
# ============ #

ut_dep = dependency(
  'boost.ut',
  include_type: 'system',
)

test_deps = [ut_dep, draconis_dep] + lib_deps

# Environment utilities tests
test_env = executable(
  'test_env',
  'test_env.cpp',
  dependencies: test_deps,
)
test('Environment Utils', test_env)

# Types tests
test_types = executable(
  'test_types',
  'test_types.cpp',
  dependencies: test_deps,
)
test('Types', test_types)

# Error handling tests
test_error = executable(
  'test_error',
  'test_error.cpp',
  dependencies: test_deps,
)
test('Error Handling', test_error)

# Localization tests
test_localization = executable(
  'test_localization',
  'test_localization.cpp',
  dependencies: test_deps,
)
test('Localization', test_localization)

# Argument Parser tests
test_argparser = executable(
  'test_argparser',
  'test_argparser.cpp',
  dependencies: test_deps,
)
test('Argument Parser', test_argparser)

# Performance helper regression tests
test_performance_helpers = executable(
  'test_performance_helpers',
  'test_performance_helpers.cpp',
  dependencies: test_deps,
)
test('Performance Helpers', python_prog,
  args: [meson.project_source_root() / 'tools/run_isolated_test.py', test_performance_helpers.full_path()],
  depends: test_performance_helpers,
)

if host_system == 'windows'
  topology_test = executable('test_windows_topology', 'test_windows_topology.cpp', dependencies: test_deps)
  test('Windows Topology', topology_test)
endif

# Optional sibling plugin fixtures are compiled from the configured plugin
# roots. No network or installed container engine is needed.
foreach plugin_root : get_option('plugin_dirs')
  plugin_root = fs.is_absolute(plugin_root) ? plugin_root : meson.project_source_root() / plugin_root
  foreach fixture : ['weather_cache', 'pipe', 'yaml']
    fixture_source = plugin_root / 'tests' / ('test_' + fixture + '.cpp')
    if fs.is_file(fixture_source) and (fixture != 'pipe' or host_system == 'windows') and (fixture != 'weather_cache' or feature_states['caching'])
      fixture_exe = executable('test_plugin_' + fixture, fixture_source,
        dependencies: test_deps, cpp_args: ['-DDRAC_PRECOMPILED_CONFIG=0'])
      test('Plugin ' + fixture, python_prog,
        args: [meson.project_source_root() / 'tools/run_isolated_test.py', fixture_exe.full_path()],
        depends: fixture_exe, timeout: 15)
    endif
  endforeach
  http_fixture = plugin_root / 'tests' / 'test_http.cpp'
  http_runner = plugin_root / 'tests' / 'run_http_test.py'
  if fs.is_file(http_fixture) and fs.is_file(http_runner)
    http_exe = executable('test_plugin_http', http_fixture,
      dependencies: test_deps, cpp_args: ['-DDRAC_PRECOMPILED_CONFIG=0'])
    test('Plugin HTTP Sessions', python_prog, args: [http_runner, http_exe.full_path()],
      depends: http_exe, timeout: 20)
  endif
endforeach

# Selection must not initialize or collect unrelated providers.
collection_selection = executable('test_collection_selection',
  ['test_collection_selection.cpp', '../src/CLI/Core/SystemInfo.cpp', '../src/CLI/UI/UI.cpp'],
  include_directories: include_directories('../src/CLI'),
  dependencies: test_deps)
test('Collection Selection', python_prog,
  args: [meson.project_source_root() / 'tools/run_isolated_test.py', collection_selection.full_path()],
  depends: collection_selection)
