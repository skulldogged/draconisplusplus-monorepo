# ================================== #
#  Draconis++ Dynamic Plugin Builds  #
# ================================== #

# Builds every discovered plugin (bundled plugins/ subdirectories plus any
# -Dplugin_dirs= directories) as a loadable module. Plugin metadata comes from
# each plugin's plugin.json manifest - see plugins/README.md for the schema.
#
# This directory is only entered for dynamic plugin builds; when static
# plugins are requested via -Dstatic_plugins=, their sources are compiled
# directly into the core library instead (see core/src/Lib/meson.build).

plugin_cpp_args = ['-DDRAC_PLUGIN_BUILD=1']

if cpp.get_id() not in ['msvc', 'clang-cl']
  plugin_cpp_args += '-fvisibility=default'
endif

if host_system == 'windows'
  plugin_name_suffix = 'dll'
  plugin_cpp_args += '-DDRAC_PLUGIN_WINDOWS=1'
elif host_system == 'darwin'
  plugin_name_suffix = 'dylib'
  plugin_cpp_args += '-DDRAC_PLUGIN_MACOS=1'
else
  plugin_name_suffix = 'so'
  plugin_cpp_args += '-DDRAC_PLUGIN_UNIX=1'
endif

plugin_install_dir = get_option('libdir') / 'draconis++' / 'plugins'

foreach plugin_name, plugin_dir : discovered_plugins
  plugin_info = run_command(
    python_prog, plugin_helper, 'info', plugin_dir, '--platform', host_system,
    check: true,
  )

  plugin_supported = true
  plugin_sources = []
  plugin_deps = []
  plugin_missing_dep = ''

  foreach line : plugin_info.stdout().strip().split('\n')
    parts = line.split('|')
    if parts[0] == 'SUPPORTED' and parts[1] == 'no'
      plugin_supported = false
    elif parts[0] == 'SRC'
      plugin_sources += files(parts[1])
    elif parts[0] == 'DEP'
      plugin_dep = dependency(
        parts[1],
        include_type: parts[2],
        static: parts[3] == 'true',
        required: false,
      )
      if plugin_dep.found()
        plugin_deps += plugin_dep
      else
        plugin_missing_dep = parts[1]
      endif
    endif
  endforeach

  if not plugin_supported
    message('Skipping plugin "@0@": not supported on @1@'.format(plugin_name, host_system))
  elif plugin_missing_dep != ''
    warning('Skipping plugin "@0@": missing dependency "@1@"'.format(plugin_name, plugin_missing_dep))
  else
    shared_module(
      plugin_name,
      plugin_sources,
      dependencies: [draconis_dep, glaze_dep] + plugin_deps,
      cpp_args: plugin_cpp_args,
      objcpp_args: plugin_cpp_args,
      name_prefix: '',
      name_suffix: plugin_name_suffix,
      install: true,
      install_dir: plugin_install_dir,
    )
  endif
endforeach
