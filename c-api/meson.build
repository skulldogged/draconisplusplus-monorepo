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

install_headers('include/draconis_c.h', subdir: 'draconis')

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
