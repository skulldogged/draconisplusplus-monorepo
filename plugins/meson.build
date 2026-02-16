# ================================ #
#   Draconis++ Plugin Build System #
# ================================ #

# This file is currently not used for dynamic plugin builds.
# Dynamic plugins should be built as standalone Meson projects.
# Static plugins are handled directly in core/src/Lib/meson.build.

# Plugin build configuration (kept for reference)
plugin_cpp_args = []
plugin_base_deps = [draconis_dep, glaze_dep]

if cpp.get_id() in ['msvc', 'clang-cl']
  plugin_cpp_args += [
    '/DDRAC_PLUGIN_BUILD=1',
    '/DNDEBUG',
    '/O2',
  ]
else
  plugin_cpp_args += [
    '-DDRAC_PLUGIN_BUILD=1',
    '-DNDEBUG',
    '-fvisibility=default',
    '-O3',
  ]
endif

if host_system == 'windows'
  plugin_suffix = '.dll'
  if cpp.get_id() in ['msvc', 'clang-cl']
    plugin_cpp_args += '/DDRAC_PLUGIN_WINDOWS=1'
  else
    plugin_cpp_args += '-DDRAC_PLUGIN_WINDOWS=1'
  endif
elif host_system == 'darwin'
  plugin_suffix = '.dylib'
  plugin_cpp_args += '-DDRAC_PLUGIN_MACOS=1'
else
  plugin_suffix = '.so'
  plugin_cpp_args += '-DDRAC_PLUGIN_UNIX=1'
endif

plugin_install_dir = get_option('libdir') / 'draconis++' / 'plugins'
