c3c = find_program('c3c', required: true)

c3_dir = meson.current_source_dir()
c3_build_dir = meson.current_build_dir()
example_src = c3_dir / 'examples' / 'demo.c3'

example_output = host_system == 'windows' ? 'demo_c3.exe' : 'demo_c3'

build_root = meson.project_build_root()
# Prefer the static archive target if the main library is shared.
if build_shared_draconis_c
  c_archive = draconis_c_static.full_path()
  c3_native_dep = draconis_c_static
else
  c_archive = draconis_c.full_path()
  c3_native_dep = draconis_c
endif
dracpp_archive = build_root / 'subprojects' / 'draconis' / 'src' / 'Lib' / 'libdrac++.a'

c3_example_exe = custom_target('demo_c3',
  input: example_src,
  output: example_output,
  command: [
    c3c, 'compile', '@INPUT@',
    '--sources', c3_dir / 'draconis.c3', c3_dir / 'draconis_raw.c3',
    '--path', c3_build_dir,
    '--path', c3_dir,
    '-o', '@OUTPUT@',
    '--wincrt=static',
    '-l', 'advapi32',
    '-l', 'ole32',
    '-l', 'dxgi',
    '-l', 'iphlpapi',
    '-z', c_archive,
    '-z', dracpp_archive,
    '--trust=include',
  ],
  depends: [c3_native_dep],
  console: true,
  build_by_default: false,
)

demo_c3_path = c3_build_dir / example_output

# Meson requires run_target programs to exist at configure time.
# Use c3c compile-run instead to build+run the example on demand.
c3_example = custom_target('c3_example',
  input: example_src,
  output: 'c3_example.stamp',
  command: [
    c3c, 'compile-run', '@INPUT@',
    '--sources', c3_dir / 'draconis.c3', c3_dir / 'draconis_raw.c3',
    '--path', c3_build_dir,
    '--path', c3_dir,
    '--wincrt=static',
    '-l', 'advapi32',
    '-l', 'ole32',
    '-l', 'dxgi',
    '-l', 'iphlpapi',
    '-z', c_archive,
    '-z', dracpp_archive,
    '--trust=include',
  ],
  depends: [c3_native_dep],
  console: true,
  build_by_default: false,
)
