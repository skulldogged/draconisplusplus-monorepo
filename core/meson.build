# ========================= #
#   Project Configuration   #
# ========================= #

cpp = meson.get_compiler('cpp')
host_system = host_machine.system()

# Get build date (cross-platform)
if host_system == 'windows'
  # Windows: use PowerShell to get date
  build_date = run_command('powershell', '-Command', 'Get-Date -Format "yyyy-MM-dd"', check: false)
else
  # Unix: use date command
  build_date = run_command('date', '+%Y-%m-%d', check: false)
endif
drac_build_date = build_date.returncode() == 0 ? build_date.stdout().strip() : 'unknown'

# Get git hash (short form)
git_hash = run_command('git', 'rev-parse', '--short', 'HEAD', check: false)
drac_git_hash = git_hash.returncode() == 0 ? git_hash.stdout().strip() : 'unknown'

# Preprocessor definitions
project_string_defines = {
  'DRAC_VERSION': '"' + meson.project_version() + '"',
  'DRAC_BUILD_DATE': '"' + drac_build_date + '"',
  'DRAC_GIT_HASH': '"' + drac_git_hash + '"',
  'DRAC_DEFAULT_LANGUAGE': '"' + get_option('default_language') + '"',
  '_WIN32_WINNT': '0x0602',
}

project_flag_defines = {
  'DRAC_ARCH_64BIT': cpp.sizeof('void*') == 8,
  'DRAC_ARCH_AARCH64': host_machine.cpu_family() == 'aarch64',
  'DRAC_ARCH_ARM': host_machine.cpu_family() == 'arm',
  'DRAC_ARCH_X86': host_machine.cpu_family() == 'x86',
  'DRAC_ARCH_X86_64': host_machine.cpu_family() == 'x86_64',
  'DRAC_DEBUG': get_option('debug'),
}

if host_system == 'windows'
  project_flag_defines += {
    'NOMINMAX': true,
    'WIN32_LEAN_AND_MEAN': true,
    'CURL_STATICLIB': true,
  }
endif

# Feature detection
feature_states = {}

# Define which options are features vs booleans
feature_options = [
  'caching',
  'weather',
  'packagecount',
  'plugins',
  'xcb',
  'wayland',
  'pugixml',
]

foreach option, define : {
  'caching': 'DRAC_ENABLE_CACHING',
  'packagecount': 'DRAC_ENABLE_PACKAGECOUNT',
  'plugins': 'DRAC_ENABLE_PLUGINS',
  'precompiled_config': 'DRAC_PRECOMPILED_CONFIG',
  'pugixml': 'DRAC_USE_PUGIXML',
  'use_linked_pci_ids': 'DRAC_USE_LINKED_PCI_IDS',
  'wayland': 'DRAC_USE_WAYLAND',
  'xcb': 'DRAC_USE_XCB',
}
  opt_val = get_option(option)

  # Check if this is a feature option
  if option in feature_options
    enabled = opt_val.enabled()
  else
    enabled = opt_val
  endif

  project_flag_defines += {define: enabled}
  feature_states += {option: enabled}
endforeach

# In precompiled config mode, plugins are enabled if and only if static_plugins is non-empty
if get_option('precompiled_config')
  static_plugins = get_option('static_plugins')
  plugins_enabled = static_plugins.length() > 0
  project_flag_defines += {'DRAC_ENABLE_PLUGINS': plugins_enabled}
  feature_states += {'plugins': plugins_enabled}
endif

cpp_args = []

foreach name, value : project_string_defines
  cpp_args += '-D@0@=@1@'.format(name, value)
endforeach

foreach name, value : project_flag_defines
  cpp_args += '-D@0@=@1@'.format(name, value ? '1' : '0')
endforeach

if cpp.get_id() in ['msvc', 'clang-cl']
  cpp_args += '/std:c++latest'
  cpp_args += cpp.get_supported_arguments(
    '/Zc:__cplusplus',
    '/Zc:preprocessor',
    '/external:W0',
    '/external:anglebrackets',
  )
else
  cpp_args += '-std=c++26'
endif

# Check compiler arguments in groups
cpp_args += cpp.get_supported_arguments(
  [
    # Common flags
    '-fno-strict-enums',
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',
    '-march=native',

    # Warning flags
    '-Wno-c++17-extensions',
    '-Wno-c++20-compat',
    '-Wno-c++20-extensions',
    '-Wno-c++98-compat-pedantic',
    '-Wno-c++98-compat',
    '-Wno-disabled-macro-expansion',
    '-Wno-gnu-conditional-omitted-operand',
    '-Wno-gnu-statement-expression-from-macro-expansion',
    '-Wno-missing-prototypes',
    '-Wno-padded',
    '-Wno-pre-c++20-compat-pedantic',
    '-Wno-unused-command-line-argument',
    '-Wunused-function',
  ],
)

add_project_arguments(cpp_args, language: 'cpp')

if host_system == 'darwin'
  add_languages('objcpp', native: false)
  add_project_arguments(cpp_args, language: 'objcpp')
endif

# =================== #
#   Include Folders   #
# =================== #
project_public_includes = include_directories('include', is_system: true)
third_party_includes = include_directories('third_party', is_system: true)

fs = import('fs')

# For static plugins, add the cloned plugins repo to include path
# This allows #include "plugins/..." to work with the cloned repo structure.
plugins_dir = meson.project_source_root() / 'plugins'
plugins_includes = []

if fs.is_dir(plugins_dir)
  plugins_includes = [include_directories('plugins', is_system: true)]
else
  message('Plugins directory not found; static plugins will be unavailable unless provided externally.')
endif

includes_dep = declare_dependency(
  include_directories: [project_public_includes, third_party_includes] + plugins_includes,
)

# ================ #
#   Dependencies   #
# ================ #
lib_deps = [includes_dep]

lib_deps += dependency(
  'magic_enum',
  fallback: ['magic_enum', 'magic_enum_dep'],
  include_type: 'system',
  static: true,
)

# mimalloc causes issues on macOS due to mixed allocator usage with system libraries
if host_system != 'darwin'
  lib_deps += dependency(
    'mimalloc',
    fallback: ['mimalloc', 'mi_dep'],
    include_type: 'system',
    static: true,
  )
endif

# Precompiled configuration
if get_option('precompiled_config') == true
  config_hpp_path = meson.project_source_root() / 'config.hpp'

  if not fs.is_file(config_hpp_path)
    error(
      'Option "precompiled_config" is enabled, but "@0@" was not found.'.format(config_hpp_path),
    )
  endif
endif

if feature_states['packagecount'] and host_system in ['darwin', 'linux']
  lib_deps += dependency('SQLiteCpp')
endif

# Plugin system dependencies
if feature_states['plugins']
  if host_system != 'windows'
    lib_deps += cpp.find_library('dl')
  endif
endif

# Platform-specific dependencies
if host_system == 'darwin'
  lib_deps += dependency(
    'appleframeworks',
    modules: [
      'coregraphics',
      'foundation',
      'iokit',
      'metal',
      'systemconfiguration',
    ],
  )
  lib_deps += cpp.find_library('iconv')
elif host_system == 'windows'
  lib_deps += [
    cpp.find_library('dwmapi'),
    cpp.find_library('setupapi'),
    cpp.find_library('dxgi'),
    cpp.find_library('dxguid'),
    cpp.find_library('ole32'),
    cpp.find_library('propsys'),
    cpp.find_library('iphlpapi'),
    cpp.find_library('ws2_32'),
  ]
elif host_system not in ['serenity', 'haiku']
  lib_deps += dependency('xau', required: get_option('xcb'))
  lib_deps += dependency('xcb', required: get_option('xcb'))
  lib_deps += dependency('xcb-randr', required: get_option('xcb'))
  lib_deps += dependency('xdmcp', required: get_option('xcb'))

  lib_deps += dependency('pugixml', required: get_option('pugixml'))

  lib_deps += dependency('wayland-client', required: get_option('wayland'))
endif

# Glaze (JSON/BEVE serializer/deserializer)
glaze_dep = dependency('glaze', include_type: 'system', required: false)

if not glaze_dep.found()
  cmake = import('cmake')
  options = cmake.subproject_options()
  options.add_cmake_defines(
    {
      'glaze_ENABLE_SSL': false,
      'glaze_BUILD_EXAMPLES': false,
      'glaze_DEVELOPER_MODE': false,
    },
  )
  glaze_proj = cmake.subproject('glaze', options: options)
  glaze_dep = glaze_proj.dependency('glaze_glaze', include_type: 'system')
endif

lib_deps += glaze_dep

# =============== #
#   Subprojects   #
# =============== #
subdir('src/Lib')

if get_option('build_cli')
  subdir('src/CLI')
endif

if get_option('build_examples')
  lib_deps += dependency('asio', static: true, fallback: ['asio', 'asio_dep'])
  subdir('examples')
endif

# Build unit tests
if get_option('build_tests')
  subdir('tests')
endif

install_subdir('include/Drac++', install_dir: get_option('includedir') / 'Drac++')
