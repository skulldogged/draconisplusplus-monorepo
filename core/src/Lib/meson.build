# ------- #
#  Files  #
# ------- #

fs = import('fs')

# Structured source organization
lib_sources = {
  'base' : files('Localization.cpp'),
  'packages' : files('Services/Packages.cpp'),
  'plugins' : files('Core/PluginManager.cpp'),
}

# Static plugin registry - always compiled when plugins are enabled so that
# DracInitStaticPlugins() exists in every configuration (it registers zero
# plugins in dynamic-only builds).
static_plugin_registry = files('Core/StaticPlugins.cpp')

# Platform-specific sources
platform_sources = {
  'darwin' : files('OS/macOS.cpp', 'OS/macOS/Bridge.mm'),
  'dragonfly' : files('OS/BSD.cpp'),
  'freebsd' : files('OS/BSD.cpp'),
  'haiku' : files('OS/Haiku.cpp'),
  'linux' : files('OS/Linux.cpp'),
  'netbsd' : files('OS/BSD.cpp'),
  'serenity' : files('OS/Serenity.cpp'),
  'windows' : files('OS/Windows.cpp'),
}

# Build final source list
lib_all_sources = lib_sources['base']

if get_option('packagecount').enabled()
  lib_all_sources += lib_sources['packages']
endif

# Add platform sources
lib_all_sources += platform_sources.get(host_system, files())

# Plugin configuration
static_plugin_cpp_args = []

# Whether any plugin requires codesigning (macOS only, for private framework access)
plugin_requires_codesign = false

if feature_states['plugins']
  lib_all_sources += lib_sources['plugins']
  lib_all_sources += static_plugin_registry

  # Plugin classes the generated DracInitStaticPlugins() must register
  static_plugin_classes = []

  if static_plugin_list.length() > 0
    # Static plugin sources are compiled directly into the library;
    # DRAC_PLUGIN() expands to self-registration functions instead of
    # dlopen entry points.
    static_plugin_cpp_args += '-DDRAC_STATIC_PLUGIN_BUILD=1'

    foreach plugin_name : static_plugin_list
      plugin_dir = discovered_plugins[plugin_name]
      plugin_info = run_command(
        python_prog, plugin_helper, 'info', plugin_dir, '--platform', host_system,
        check: true,
      )

      foreach line : plugin_info.stdout().strip().split('\n')
        parts = line.split('|')
        if parts[0] == 'SUPPORTED' and parts[1] == 'no'
          error('Static plugin "@0@" does not support @1@.'.format(plugin_name, host_system))
        elif parts[0] == 'CLASS'
          static_plugin_classes += parts[1]
        elif parts[0] == 'CODESIGN' and parts[1] == 'yes'
          plugin_requires_codesign = true
        elif parts[0] == 'SRC'
          lib_all_sources += files(parts[1])
        elif parts[0] == 'DEP'
          lib_deps += dependency(parts[1], include_type: parts[2], static: parts[3] == 'true')
        endif
      endforeach
    endforeach
  endif

  # Generate DracInitStaticPlugins(); registers zero plugins in dynamic-only builds
  lib_all_sources += configure_file(
    output: 'StaticPluginInit.cpp',
    command: [python_prog, plugin_helper, 'registry'] + static_plugin_classes,
    capture: true,
  )
endif

# Link arguments for pci_ids (only applied at final executable level, not on dependencies)
pci_ids_link_args = []

if get_option('use_linked_pci_ids') == true
  pci_ids_link_args += meson.project_source_root() / 'pci_ids.o'
endif

# ----------------- #
#  Static Library   #
# ----------------- #
libdrac = static_library(
  'drac++',
  lib_all_sources,
  dependencies : lib_deps,
  cpp_args : static_plugin_cpp_args,
  install : false,
)

# Standard dependency (for most targets)
# Includes lib_deps so consumers get all transitive dependencies automatically
# NOTE: pci_ids.o is NOT included here to avoid duplication through dependency chains.
# Executables that need it should add pci_ids_link_args to their own link_args.
draconis_dep = declare_dependency(
  link_with : libdrac,
  dependencies : [includes_dep] + lib_deps,
  compile_args : static_plugin_cpp_args,
)

# Dependency with link_whole for the main CLI (ensures static plugin self-registration isn't stripped)
if static_plugin_list.length() > 0
  draconis_dep_whole = declare_dependency(
    link_whole : libdrac,
    dependencies : [includes_dep] + lib_deps,
    compile_args : static_plugin_cpp_args,
  )
else
  draconis_dep_whole = draconis_dep
endif
