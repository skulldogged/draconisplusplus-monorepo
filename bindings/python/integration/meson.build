python = import('python').find_installation(pure: false)
nanobind_dep = dependency('nanobind')
python_module = python.extension_module('draconis', '../src/bindings.cpp',
  dependencies: [nanobind_dep, draconis_c_dep], install: true,
)
test('Python Binding', python,
  args: ['-c', 'import draconis; s = draconis.SystemInfo(); assert s.get_mem_info().total_bytes > 0'],
  env: {'PYTHONPATH': meson.current_build_dir()}, depends: python_module,
)
