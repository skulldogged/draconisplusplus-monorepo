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
    enabled = opt_val.allowed()
  else
    enabled = opt_val
  endif

  project_flag_defines += {define: enabled}
  feature_states += {option: enabled}
endforeach

# Resolve feature options before emitting their public macros.
optional_platform_deps = []
foreach option, names : {'xcb': ['xau', 'xcb', 'xcb-randr', 'xdmcp'], 'wayland': ['wayland-client'], 'pugixml': ['pugixml']}
  supported = host_system in ['linux', 'freebsd', 'netbsd', 'openbsd', 'dragonfly']
  resolved = supported and not get_option(option).disabled()
  detected = []
  if resolved
    foreach name : names
      dep = dependency(name, required: get_option(option))
      resolved = resolved and dep.found()
      detected += dep
    endforeach
  endif
  if resolved
    optional_platform_deps += detected
  endif
  feature_states += {option: resolved}
  define = {'xcb': 'DRAC_USE_XCB', 'wayland': 'DRAC_USE_WAYLAND', 'pugixml': 'DRAC_USE_PUGIXML'}[option]
  project_flag_defines += {define: resolved}
endforeach

# Plugin system: 'auto' enables the dynamic loader (it has no extra
# dependencies), and requesting static plugins forces the system on.
plugins_enabled = get_option('plugins').allowed() or static_plugin_list.length() > 0

# In precompiled config mode, plugins are enabled if and only if static plugins are requested
if get_option('precompiled_config')
  plugins_enabled = static_plugin_list.length() > 0
endif

project_flag_defines += {'DRAC_ENABLE_PLUGINS': plugins_enabled}
feature_states += {'plugins': plugins_enabled}

subdir('include/Drac++')

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

# LTO bitcode archives have no usable symbol index, so consumers needed
# --whole-archive to see static plugin registration. Fat LTO objects keep
# both IR and a real ELF symbol table, so normal archive extraction works
# and clang+mold no longer fails with undefined __cxx_global_var_init.
if get_option('b_lto')
  cpp_args += cpp.get_supported_arguments('-ffat-lto-objects')
endif

# Check compiler arguments in groups
cpp_args += cpp.get_supported_arguments(
  [
    # Common flags
    '-fno-strict-enums',
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',

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

if get_option('native_tuning')
  cpp_args += cpp.get_supported_arguments('-march=native')
endif

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

# Make headers from external plugin roots visible to core. This supports, for
# example, a precompiled configuration that includes plugin config headers.
plugins_includes = []

foreach extra_dir : get_option('plugin_dirs')
  if fs.is_absolute(extra_dir)
    if fs.is_dir(extra_dir)
      plugins_includes += include_directories(extra_dir, is_system: true)
    endif
  else
    extra_abs_dir = meson.project_source_root() / extra_dir
    if fs.is_dir(extra_abs_dir)
      plugins_includes += include_directories('..' / extra_dir, is_system: true)
    endif
  endif
endforeach

includes_dep = declare_dependency(
  include_directories: [project_public_includes, third_party_includes] + plugins_includes,
)

# ================ #
#   Dependencies   #
# ================ #
lib_deps = [includes_dep] + optional_platform_deps

lib_deps += dependency(
  'magic_enum',
  fallback: ['magic_enum', 'magic_enum_dep'],
  include_type: 'system',
  static: true,
)

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
    cpp.find_library('iphlpapi'),
    cpp.find_library('ws2_32'),
  ]
endif

# Glaze (JSON/BEVE serializer/deserializer)
glaze_dep = dependency('glaze', include_type: 'system', required: false, version: '>=6.0.0')

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

install_subdir('include/Drac++', install_dir: get_option('includedir'), exclude_files: ['meson.build', 'BuildConfig.hpp.in'])
install_subdir('third_party', install_dir: get_option('includedir') / 'draconis-third-party', strip_directory: true)

if not dependency('glaze', required: false).found()
  install_subdir('../subprojects/glaze/include/glaze', install_dir: get_option('includedir'))
endif
import('pkgconfig').generate(libdrac,
  filebase: 'draconis', name: 'Draconis C++ SDK',
  description: 'C++26 system information library',
  subdirs: ['.', 'draconis-third-party'],
  libraries_private: lib_deps,
)
