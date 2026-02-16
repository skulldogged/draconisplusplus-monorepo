lua_dep = dependency('lua', required: true, fallback: ['lua', 'lua_dep'])

lua_module = shared_module('draconis',
  'src/bindings.cpp',
  include_directories: [c_inc, sol2_inc],
  dependencies: [lua_dep, draconis_c_dep],
  install: true,
)

lua_example = []
lua_interp = find_program('lua', 'lua5.4', 'lua54', required: false)
if lua_interp.found()
  build_root = meson.project_build_root()
  lua_example += custom_target('lua_example',
    command: [lua_interp, meson.current_source_dir() / 'examples' / 'demo.lua'],
    output: 'lua_example.stamp',
    console: true,
    build_by_default: false,
    depends: [lua_module],
    env: {'LUA_CPATH': build_root / 'lua' / '?.dll;' + build_root / 'lua' / '?.so;;'},
  )
endif
