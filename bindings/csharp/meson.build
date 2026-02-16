dotnet = find_program('dotnet', required: true)
csharp_dir = meson.current_source_dir()

csharp_bindings = custom_target('csharp_bindings',
  command: [dotnet, 'build', csharp_dir / 'Draconis' / 'Draconis.csproj', '-c', 'Release'],
  output: 'csharp_bindings.stamp',
  console: true,
  build_by_default: true,
  depends: [draconis_c],
)

csharp_env = environment()
if host_system == 'windows'
  csharp_env.prepend('PATH', meson.project_build_root())
  draconis_c_path = meson.project_build_root() / 'draconis_c.dll'
elif host_system == 'darwin'
  csharp_env.prepend('DYLD_LIBRARY_PATH', meson.project_build_root())
  draconis_c_path = meson.project_build_root() / 'libdraconis_c.dylib'
else
  csharp_env.prepend('LD_LIBRARY_PATH', meson.project_build_root())
  draconis_c_path = meson.project_build_root() / 'libdraconis_c.so'
endif
csharp_env.set('DRACONIS_C_LIBRARY', draconis_c_path)

csharp_example = custom_target('csharp_example',
  command: [
    dotnet, 'run',
    '--project', csharp_dir / 'examples' / 'BasicInfo' / 'BasicInfo.csproj',
    '-c', 'Release',
  ],
  output: 'csharp_example.stamp',
  console: true,
  build_by_default: false,
  depends: [csharp_bindings],
  env: csharp_env,
)
