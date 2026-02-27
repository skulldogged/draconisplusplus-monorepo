python = import('python').find_installation(pure: false)
nanobind_dep = dependency('nanobind', method: 'auto')

python.extension_module('draconis',
  'src/bindings.cpp',
  dependencies: [nanobind_dep, drac_dep],
  install: true,
  subdir: '.',
)
