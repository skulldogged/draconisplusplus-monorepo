project('draconis-python', 'cpp', version: '0.1.0', default_options: ['cpp_std=c++20'])
python = import('python').find_installation(pure: false)
nanobind_dep = dependency('nanobind', method: 'auto')

python.extension_module('draconis',
  'src/bindings.cpp',
  dependencies: [nanobind_dep, dependency('draconis-c')],
  install: true,
  subdir: '.',
)
