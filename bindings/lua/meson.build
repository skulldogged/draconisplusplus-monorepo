lua_dep = dependency('lua', required: true, fallback: ['lua', 'lua_dep'])

sol2_dep = dependency('sol2', fallback: ['sol2', 'sol2_dep'])

lua_module = shared_module('draconis',
  'src/bindings.cpp',
  include_directories: c_inc,
  name_prefix: '',
  dependencies: [lua_dep, sol2_dep, draconis_c_dep],
  install: true,
)

lua_example = []
lua_interp = find_program('lua', 'lua5.4', 'lua54', required: false)
if lua_interp.found()
  build_root = meson.current_build_dir()
  lua_example += custom_target('lua_example',
    command: [lua_interp, meson.current_source_dir() / 'examples' / 'demo.lua'],
    output: 'lua_example.stamp',
    console: true,
    build_by_default: false,
    depends: [lua_module],
    env: {'LUA_CPATH': build_root / '?.dll;' + build_root / '?.so;;'},
  )
endif

lua_test_command = []
if lua_interp.found()
  lua_test_command = [lua_interp]
elif lua_dep.type_name() == 'internal'
  lua_test_command = [subproject('lua').get_variable('lua_exe')]
endif
if lua_test_command.length() > 0
  test('Lua Binding', lua_test_command[0],
    args: ['-e', 'local d=require("draconis"); local s=d.SystemInfo.new(); assert(s:get_mem_info().total_bytes > 0)'],
    env: {'LUA_CPATH': meson.current_build_dir() / '?.dll;' + meson.current_build_dir() / '?.so;;'}, depends: lua_module)
endif
