# Installed consumers must see the same feature and architecture definitions.
sdk_configuration = configuration_data()
sdk_definitions = ''
foreach name, value : project_string_defines
  sdk_definitions += '#ifndef @0@\n#define @0@ @1@\n#endif\n'.format(name, value)
endforeach
foreach name, value : project_flag_defines
  sdk_definitions += '#ifndef @0@\n#define @0@ @1@\n#endif\n'.format(name, value ? 1 : 0)
endforeach
sdk_configuration.set('SDK_DEFINITIONS', sdk_definitions)
configure_file(input: 'BuildConfig.hpp.in', output: 'BuildConfig.hpp', configuration: sdk_configuration,
  install_dir: get_option('includedir') / 'Drac++',
)
