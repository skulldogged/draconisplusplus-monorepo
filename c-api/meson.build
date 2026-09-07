# ============================== #
#  C API (Stable ABI Layer)     #
# ============================== #

c_inc = include_directories('include')

draconis_c = static_library('draconis_c',
  'src/draconis_c.cpp',
  include_directories: c_inc,
  dependencies: draconis_dep,
  install: true,
)

draconis_c_dep = declare_dependency(
  link_with: draconis_c,
  include_directories: c_inc,
  dependencies: draconis_dep,
)

# A real shared runtime for P/Invoke and standalone C consumers. Keep the
# static dependency above for bindings that embed the native implementation.
draconis_c_shared = shared_library('draconis_c',
  'src/draconis_c.cpp',
  include_directories: c_inc,
  dependencies: draconis_dep,
  cpp_args: ['-DDRACONIS_C_SHARED=1', '-DDRACONIS_C_BUILD=1'],
  name_prefix: host_system == 'windows' ? '' : 'lib',
  install: true,
)
draconis_c_shared_dep = declare_dependency(
  link_with: draconis_c_shared,
  include_directories: c_inc,
  compile_args: ['-DDRACONIS_C_SHARED=1'],
)
import('pkgconfig').generate(draconis_c_shared,
  filebase: 'draconis-c', name: 'Draconis C API',
  description: 'System information through the standalone C ABI',
  subdirs: 'draconis', extra_cflags: ['-DDRACONIS_C_SHARED=1'],
)

install_headers('include/draconis_c.h', subdir: 'draconis')

if get_option('build_tests') and feature_states['plugins']
  lifecycle_fixture = shared_module('lifecycle_fixture', 'tests/fixture.cpp',
    dependencies: lib_deps,
    cpp_args: ['-DDRAC_PLUGIN_BUILD=1'],
    name_prefix: '',
    install: false,
  )
  lifecycle_test = executable('test_plugin_lifecycle', 'tests/test_plugin_lifecycle.cpp',
    dependencies: [draconis_c_dep, ut_dep],
  )
  legacy_fixture = shared_module('legacy_fixture', 'tests/legacy_plugin.cpp', name_prefix: '', install: false)
  test('Plugin Lifecycle', python_prog,
    args: [meson.project_source_root() / 'tools/run_isolated_test.py', lifecycle_test.full_path(), lifecycle_fixture.full_path(), legacy_fixture.full_path()],
    depends: [lifecycle_test, lifecycle_fixture, legacy_fixture], timeout: 15)
endif

if get_option('build_examples')
  # demo.c is a C TU linked against LTO C++ archives. clang + mold inspect the
  # C object before LTO when --as-needed is on, drop the C++ runtime, and fail
  # with undefined __cxx_global_var_init. Link as C++ and keep the runtime.
  demo_c_exe = executable('demo_c',
    'examples/demo.c',
    dependencies: draconis_c_dep,
    link_language: 'cpp',
    override_options: ['b_asneeded=false'],
    install: false,
  )

  custom_target('c_example',
    command: [demo_c_exe],
    output: 'c_example.stamp',
    console: true,
    build_by_default: false,
    depends: [demo_c_exe],
  )
endif
