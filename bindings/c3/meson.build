c3c = find_program('c3c', required: true)
c3_runtime_dir = fs.parent(draconis_c_shared.full_path())
c3_flags = ['-L', c3_runtime_dir, '-l', 'draconis_c', '--trust=include']
if host_system == 'windows'
  c3_flags += '--wincrt=static'
endif
c3_example_exe = custom_target('demo_c3',
  input: ['examples/demo.c3', 'draconis.c3', 'draconis_raw.c3'],
  output: host_system == 'windows' ? 'demo_c3.exe' : 'demo_c3',
  command: [c3c, 'compile', '@INPUT@', '-o', meson.current_build_dir() / 'demo_c3'] + c3_flags,
  depends: draconis_c_shared, build_by_default: true,
)
c3_env = environment()
if host_system == 'windows'
  c3_env.prepend('PATH', c3_runtime_dir)
elif host_system == 'darwin'
  c3_env.prepend('DYLD_LIBRARY_PATH', c3_runtime_dir)
else
  c3_env.prepend('LD_LIBRARY_PATH', c3_runtime_dir)
endif
run_target('c3_example', command: c3_example_exe, depends: draconis_c_shared, env: c3_env)
