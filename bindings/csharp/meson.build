dotnet = find_program('dotnet', required: true)
csharp_dir = meson.current_source_dir()

csharp_bindings = custom_target('csharp_bindings',
  depend_files: files('Draconis/DraconisClient.cs', 'Draconis/NativeMethods.cs', 'Draconis/Draconis.csproj'),
  command: [python_prog, meson.project_source_root() / 'tools/run_and_stamp.py', '@OUTPUT@', dotnet, 'build', csharp_dir / 'Draconis' / 'Draconis.csproj', '-c', 'Release'],
  output: 'csharp_bindings.stamp',
  console: true,
  build_by_default: true,
  depends: [draconis_c_shared],
)

csharp_env = environment()
draconis_c_path = draconis_c_shared.full_path()
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
