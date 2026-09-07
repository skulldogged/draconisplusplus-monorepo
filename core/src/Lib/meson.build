# ------- #
#  Files  #
# ------- #

fs = import('fs')

# Structured source organization
lib_sources = {
  'base' : files('Localization.cpp', 'OS/Unavailable.cpp'),
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
  'openbsd' : files(),
  'serenity' : files('OS/Serenity.cpp'),
  'windows' : files('OS/Windows.cpp'),
}

# Build final source list
lib_all_sources = lib_sources['base']

if feature_states['packagecount']
  lib_all_sources += lib_sources['packages']
endif

# Add platform sources
if host_system not in platform_sources
  error('Unsupported platform: ' + host_system)
endif
lib_all_sources += platform_sources[host_system]

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

# Embed pci.ids as a C++ TU so clang LTO + mold can see it as bitcode.
# The old `ld -r -b binary` pci_ids.o is a non-LTO ELF object; mixing that
# into an LTO link makes mold fail with undefined __cxx_global_var_init.
if get_option('use_linked_pci_ids')
  pci_ids_path = get_option('pci_ids_path')
  if pci_ids_path == ''
    foreach candidate : [
      meson.project_source_root() / 'pci.ids',
      '/usr/share/hwdata/pci.ids',
      '/usr/share/misc/pci.ids',
      '/usr/share/pci.ids',
    ]
      if fs.is_file(candidate)
        pci_ids_path = candidate
        break
      endif
    endforeach
  endif

  if pci_ids_path == ''
    error('use_linked_pci_ids requires pci.ids. Pass -Dpci_ids_path=/path/to/pci.ids')
  endif

  if not fs.is_absolute(pci_ids_path)
    pci_ids_path = meson.project_source_root() / pci_ids_path
  endif

  if not fs.is_file(pci_ids_path)
    error('pci.ids not found at "@0@"'.format(pci_ids_path))
  endif

  lib_all_sources += configure_file(
    input: 'OS/PciIdsEmbed.cpp.in',
    output: 'PciIdsEmbed.cpp',
    configuration: {'PCI_IDS_PATH': pci_ids_path.replace('\\', '/')},
  )
endif

# ----------------- #
#  Static Library   #
# ----------------- #
libdrac = static_library(
  'drac++',
  lib_all_sources,
  dependencies : lib_deps,
  cpp_args : static_plugin_cpp_args,
  install : true,
)

# Standard dependency (for most targets)
# Includes lib_deps so consumers get all transitive dependencies automatically
draconis_dep = declare_dependency(
  link_with : libdrac,
  dependencies : [includes_dep] + lib_deps,
)
